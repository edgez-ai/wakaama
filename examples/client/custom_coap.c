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

/**
 * Send binary data to a custom CoAP endpoint with CoAP Block1 wise transfer
 * This implementation uses Wakaama's native blockwise transfer mechanism which
 * automatically handles DTLS encryption per block.
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
    // and keep a comfortable margin for CoAP headers and DTLS overhead
    const uint16_t BLOCK_SIZE = 512;
    
    // Calculate number of blocks needed
    uint32_t num_blocks = (dataLen + BLOCK_SIZE - 1) / BLOCK_SIZE;
    size_t offset = 0;
    uint32_t block_num = 0;
    
    LOG("custom_coap: Sending %zu bytes in %lu blocks of %u bytes each\n", 
        dataLen, (unsigned long)num_blocks, BLOCK_SIZE);
    
    // Send each block
    while (offset < dataLen) {
        size_t remaining = dataLen - offset;
        size_t block_len = (remaining > BLOCK_SIZE) ? BLOCK_SIZE : remaining;
        uint8_t more = (remaining > BLOCK_SIZE) ? 1 : 0;
        
        LOG("custom_coap: Sending block %lu/%lu (offset=%zu, len=%zu, more=%d)\n",
            (unsigned long)block_num, (unsigned long)num_blocks, offset, block_len, more);
        
        // Create a new transaction for this block
        lwm2m_transaction_t *transaction = transaction_new(
            server->sessionH,
            COAP_POST,
            NULL, NULL,
            contextP->nextMID++,
            0, NULL
        );
        
        if (!transaction) {
            LOG("custom_coap: Failed to create transaction for block %lu\n", (unsigned long)block_num);
            return -4;
        }
        
        // Set CoAP headers
        coap_set_header_uri_path(transaction->message, path);
        coap_set_header_content_type(transaction->message, contentType);
        
        // Set Block1 option for blockwise transfer
        // block_num: sequence number of block
        // more: 1 if more blocks follow, 0 for last block
        // BLOCK_SIZE: size of each block
        coap_set_header_block1(transaction->message, block_num, more, BLOCK_SIZE);
        
        // Set the payload for this block
        coap_set_payload(transaction->message, data + offset, block_len);
        
        // Add transaction to the list
        contextP->transactionList = (lwm2m_transaction_t*)LWM2M_LIST_ADD(
            contextP->transactionList, transaction);
        
        // Send the transaction
        // The DTLS layer will automatically encrypt each block
        int result = transaction_send(contextP, transaction);
        if (result != 0) {
            LOG("custom_coap: Failed to send block %lu (error=%d)\n", (unsigned long)block_num, result);
            return result;
        }
        
        // Move to next block
        offset += block_len;
        block_num++;
    }
    
    LOG("custom_coap: Successfully sent all %lu blocks (%zu bytes total)\n",
        (unsigned long)num_blocks, dataLen);
    
    return 0;
}
