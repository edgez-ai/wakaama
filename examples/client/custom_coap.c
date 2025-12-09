/*
 * Custom CoAP message sending utilities for LwM2M client
 * Provides helper functions to send custom CoAP messages through existing connections
 * with proper Block1 wise transfer support for DTLS
 */

#include "liblwm2m.h"
#include "internals.h"
#include "er-coap-13/er-coap-13.h"
#include <string.h>

#ifdef LWM2M_WITH_LOGS
#include <stdio.h>
#define LOG(...) fprintf(stderr, __VA_ARGS__)
#else
#define LOG(...)
#endif

// Structure to hold block transfer state
typedef struct {
    const uint8_t *data;
    size_t dataLen;
    const char *path;
    unsigned int contentType;
    uint16_t blockSize;
    uint32_t currentBlock;
    size_t offset;
    lwm2m_context_t *contextP;
    lwm2m_server_t *server;
    uint8_t token[4];  // Token to maintain consistency across blocks
    uint8_t tokenLen;
} block_transfer_state_t;

// Callback for handling Block1 transfer responses
static void block_transfer_callback(lwm2m_context_t *contextP,
                                     lwm2m_transaction_t *transacP,
                                     void *message)
{
    coap_packet_t *packet = (coap_packet_t *)message;
    block_transfer_state_t *state = (block_transfer_state_t *)transacP->userData;
    
    LOG("custom_coap: block_transfer_callback invoked! packet=%p, state=%p\n", packet, state);
    
    if (!state) {
        LOG("custom_coap: No transfer state in callback\n");
        return;
    }
    
    if (!packet) {
        LOG("custom_coap: No response packet (timeout?)\n");
        lwm2m_free(state);
        return;
    }
    
    LOG("custom_coap: Response code: %d.%02d\n", packet->code >> 5, packet->code & 0x1F);
    
    // Check response code
    if (packet && packet->code == COAP_231_CONTINUE) {
        LOG("custom_coap: Block %lu acknowledged (current offset=%zu)\n", 
            (unsigned long)state->currentBlock, state->offset);
        
        // Move to next block - increment offset by the actual block size that was sent
        size_t last_block_size = (state->dataLen - state->offset > state->blockSize) ? 
                                  state->blockSize : (state->dataLen - state->offset);
        state->offset += last_block_size;
        state->currentBlock++;
        
        LOG("custom_coap: After update: block=%lu, offset=%zu\n",
            (unsigned long)state->currentBlock, state->offset);
        
        // Check if there are more blocks to send
        if (state->offset < state->dataLen) {
            size_t remaining = state->dataLen - state->offset;
            size_t block_len = (remaining > state->blockSize) ? state->blockSize : remaining;
            uint8_t more = (remaining > state->blockSize) ? 1 : 0;
            
            LOG("custom_coap: Sending block %lu (offset=%zu, len=%zu, more=%d, data_ptr=%p)\n",
                (unsigned long)state->currentBlock, state->offset, block_len, more, 
                (void*)(state->data + state->offset));
            
            // Create transaction for next block
            // IMPORTANT: Use the same token as the first block so server can track the transfer
            lwm2m_transaction_t *nextTrans = transaction_new(
                state->server->sessionH,
                COAP_POST,
                NULL, NULL,
                contextP->nextMID++,
                state->tokenLen, state->token
            );
            
            if (!nextTrans) {
                LOG("custom_coap: Failed to create transaction for block %lu\n", 
                    (unsigned long)state->currentBlock);
                lwm2m_free(state);
                return;
            }
            
            // Set CoAP headers
            coap_set_header_uri_path(nextTrans->message, state->path);
            coap_set_header_content_type(nextTrans->message, state->contentType);
            coap_set_header_block1(nextTrans->message, state->currentBlock, more, state->blockSize);
            coap_set_payload(nextTrans->message, state->data + state->offset, block_len);
            
            // Keep the state for next callback
            nextTrans->userData = state;
            nextTrans->callback = block_transfer_callback;
            
            // Add and send transaction
            contextP->transactionList = (lwm2m_transaction_t*)LWM2M_LIST_ADD(
                contextP->transactionList, nextTrans);
            
            int result = transaction_send(contextP, nextTrans);
            if (result != 0) {
                LOG("custom_coap: Failed to send block %lu (error=%d)\n", 
                    (unsigned long)state->currentBlock, result);
                lwm2m_free(state);
            }
        } else {
            LOG("custom_coap: All blocks sent successfully (%zu bytes total)\n", state->dataLen);
            lwm2m_free(state);
        }
    } else if (packet && packet->code == COAP_204_CHANGED) {
        LOG("custom_coap: Transfer complete (%zu bytes total)\n", state->dataLen);
        lwm2m_free(state);
    } else {
        LOG("custom_coap: Block transfer failed with code %d.%02d\n",
            packet ? packet->code >> 5 : 0, packet ? packet->code & 0x1F : 0);
        lwm2m_free(state);
    }
}

/**
 * Send binary data to a custom CoAP endpoint with CoAP Block1 wise transfer
 * This implementation properly implements Block1-wise transfer by waiting for
 * server acknowledgment before sending the next block.
 * 
 * @param contextP LwM2M context pointer
 * @param path URI path for the CoAP POST request (e.g., "image")
 * @param data Binary data to send
 * @param dataLen Length of binary data
 * @param contentType CoAP content type (e.g., 42 for application/octet-stream)
 * @return 0 on success, error code on failure
 */
int lwm2m_send_coap_post(lwm2m_context_t *contextP, const char *path, 
                          const uint8_t *data, size_t dataLen, 
                          unsigned int contentType)
{
    if (!contextP || !path || !data || dataLen == 0) {
        LOG("custom_coap: Invalid parameters\n");
        return -1;
    }
    
    // Check if context is ready
    if (contextP->state != STATE_READY) {
        LOG("custom_coap: LwM2M context not ready (state=%d)\n", contextP->state);
        return -2;
    }
    
    // Get the first server from the server list
    lwm2m_server_t *server = contextP->serverList;
    if (!server || !server->sessionH) {
        LOG("custom_coap: No server connection available\n");
        return -3;
    }
    
    // CoAP Block1 size options: 16, 32, 64, 128, 256, 512, 1024 bytes
    // Use 512 byte blocks to align with other parts of the codebase
    const uint16_t BLOCK_SIZE = 512;
    
    // Allocate state for block transfer
    block_transfer_state_t *state = (block_transfer_state_t *)lwm2m_malloc(sizeof(block_transfer_state_t));
    if (!state) {
        LOG("custom_coap: Failed to allocate transfer state\n");
        return -4;
    }
    
    state->data = data;
    state->dataLen = dataLen;
    state->path = path;
    state->contentType = contentType;
    state->blockSize = BLOCK_SIZE;
    state->currentBlock = 0;
    state->offset = 0;
    state->contextP = contextP;
    state->server = server;
    
    // Generate a unique token for this block transfer
    // This is crucial for the server to track the blockwise transfer
    // Store the token in the state so all blocks use the same token
    state->token[0] = (contextP->nextMID >> 24) & 0xFF;
    state->token[1] = (contextP->nextMID >> 16) & 0xFF;
    state->token[2] = (contextP->nextMID >> 8) & 0xFF;
    state->token[3] = contextP->nextMID & 0xFF;
    state->tokenLen = 4;
    
    uint32_t num_blocks = (dataLen + BLOCK_SIZE - 1) / BLOCK_SIZE;
    LOG("custom_coap: Starting block transfer: %zu bytes in %lu blocks of %u bytes each\n", 
        dataLen, (unsigned long)num_blocks, BLOCK_SIZE);
    LOG("custom_coap: Using token: %02x%02x%02x%02x\n", 
        state->token[0], state->token[1], state->token[2], state->token[3]);
    
    // Send first block
    size_t block_len = (dataLen > BLOCK_SIZE) ? BLOCK_SIZE : dataLen;
    uint8_t more = (dataLen > BLOCK_SIZE) ? 1 : 0;
    
    LOG("custom_coap: Sending block 0 (offset=0, len=%zu, more=%d, data_ptr=%p, first_bytes=%02x %02x %02x %02x)\n", 
        block_len, more, (void*)data, data[0], data[1], data[2], data[3]);
    
    // Create transaction for first block
    lwm2m_transaction_t *transaction = transaction_new(
        server->sessionH,
        COAP_POST,
        NULL, NULL,
        contextP->nextMID++,
        state->tokenLen, state->token  // Use the token stored in state
    );
    
    if (!transaction) {
        LOG("custom_coap: Failed to create transaction for first block\n");
        lwm2m_free(state);
        return -5;
    }
    
    // Set CoAP headers
    coap_set_header_uri_path(transaction->message, path);
    coap_set_header_content_type(transaction->message, contentType);
    coap_set_header_block1(transaction->message, 0, more, BLOCK_SIZE);
    coap_set_payload(transaction->message, data, block_len);
    
    // Set callback and state
    transaction->userData = state;
    transaction->callback = block_transfer_callback;
    
    // Add transaction to the list
    contextP->transactionList = (lwm2m_transaction_t*)LWM2M_LIST_ADD(
        contextP->transactionList, transaction);
    
    // Send the first block
    int result = transaction_send(contextP, transaction);
    if (result != 0) {
        LOG("custom_coap: Failed to send first block (error=%d)\n", result);
        lwm2m_free(state);
        return result;
    }
    
    LOG("custom_coap: First block sent, waiting for acknowledgment\n");
    return 0;
}
