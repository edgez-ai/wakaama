/*
 * Custom CoAP message sending utilities for LwM2M client
 * Provides helper functions to send custom CoAP messages through existing connections
 * with proper Block1 wise transfer support for DTLS
 */

#include "liblwm2m.h"
#include "internals.h"
#include "er-coap-13/er-coap-13.h"
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifdef ESP_PLATFORM
#include "esp_log.h"
#define LOG(...) ESP_LOGI("custom_coap", __VA_ARGS__)
#elif defined(LWM2M_WITH_LOGS)
#include <stdio.h>
#define LOG(...) fprintf(stderr, __VA_ARGS__)
#else
#define LOG(...)
#endif

// Global flag to track if a block transfer is in progress
static bool g_transfer_in_progress = false;

// Pipeline configuration - number of blocks to send before waiting for ACK
// Start with 1 for testing to avoid watchdog issues
#define COAP_PIPELINE_WINDOW 1  // Send 1 block at a time

// Simple CRC32 (IEEE 802.3) for per-block integrity logging
static uint32_t crc32_ieee(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            uint32_t mask = -(crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320 & mask);
        }
    }
    return ~crc;
}

// Log block digest and edge bytes to help detect corruption end-to-end
static void log_block_digest(uint32_t block_num, size_t offset, const uint8_t *data, size_t len)
{
    uint32_t crc = crc32_ieee(data, len);
    size_t head_len = len < 8 ? len : 8;
    size_t tail_len = len < 8 ? 0 : 8;  // avoid double-print when len < 8
    char head_buf[3 * 8 + 1] = {0};
    for (size_t i = 0; i < head_len; i++) {
        snprintf(head_buf + 3 * i, sizeof(head_buf) - 3 * i, "%02x ", data[i]);
    }

    char tail_buf[3 * 8 + 1] = {0};
    if (tail_len > 0) {
        for (size_t i = 0; i < tail_len; i++) {
            size_t idx = len - tail_len + i;
            snprintf(tail_buf + 3 * i, sizeof(tail_buf) - 3 * i, "%02x ", data[idx]);
        }
    }

    LOG("custom_coap: Block %lu CRC32=0x%08lx (offset=%zu, len=%zu)\n",
        (unsigned long)block_num, (unsigned long)crc, offset, len);
    LOG("custom_coap:   Begin bytes (%zu): %s\n", head_len, head_buf);
    if (tail_len > 0) {
        LOG("custom_coap:   End bytes (%zu): %s\n", tail_len, tail_buf);
    }
}

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
    uint32_t nextBlockToSend;  // Track next block to send for pipelining
    uint32_t lastAckedBlock;   // Track last acknowledged block
} block_transfer_state_t;

// Forward declaration for callback
static void block_transfer_callback(lwm2m_context_t *contextP,
                                   lwm2m_transaction_t *transacP,
                                   void *userData);

// Helper function to send the next window of blocks
static void send_block_window(lwm2m_context_t *contextP, block_transfer_state_t *state)
{
    // Send up to COAP_PIPELINE_WINDOW blocks without waiting for ACK
    uint32_t window_end = state->nextBlockToSend + COAP_PIPELINE_WINDOW;
    uint32_t total_blocks = (state->dataLen + state->blockSize - 1) / state->blockSize;
    
    while (state->nextBlockToSend < window_end && 
           state->nextBlockToSend < total_blocks) {
        
        size_t block_offset = state->nextBlockToSend * state->blockSize;
        size_t remaining = state->dataLen - block_offset;
        size_t block_len = (remaining > state->blockSize) ? state->blockSize : remaining;
        uint8_t more = (remaining > block_len) ? 1 : 0;
        
        LOG("custom_coap: Sending block %lu (offset=%zu, len=%zu, more=%d, remaining=%zu, total=%zu)\n",
            (unsigned long)state->nextBlockToSend, block_offset, block_len, more, remaining, state->dataLen);
        log_block_digest(state->nextBlockToSend, block_offset, state->data + block_offset, block_len);
        
        // Create transaction for this block
        lwm2m_transaction_t *trans = transaction_new(
            state->server->sessionH,
            COAP_POST,
            NULL, NULL,
            contextP->nextMID++,
            state->tokenLen, state->token
        );
        
        if (!trans) {
            LOG("custom_coap: Failed to create transaction for block %lu\n", 
                (unsigned long)state->nextBlockToSend);
            return;
        }
        
        // Set CoAP headers
        coap_set_header_uri_path(trans->message, state->path);
        coap_set_header_content_type(trans->message, state->contentType);
        coap_set_header_block1(trans->message, state->nextBlockToSend, more, state->blockSize);
        coap_set_payload(trans->message, state->data + block_offset, block_len);
        
        // Keep the state for callback
        trans->userData = state;
        trans->callback = block_transfer_callback;
        
        // Add and send transaction
        contextP->transactionList = (lwm2m_transaction_t*)LWM2M_LIST_ADD(
            contextP->transactionList, trans);
        
        int result = transaction_send(contextP, trans);
        if (result != 0) {
            LOG("custom_coap: Failed to send block %lu (error=%d)\n", 
                (unsigned long)state->nextBlockToSend, result);
            return;
        }
        
        state->nextBlockToSend++;
    }
}

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
        g_transfer_in_progress = false;
        lwm2m_free(state);
        return;
    }
    
    LOG("custom_coap: Response code: %d.%02d\n", packet->code >> 5, packet->code & 0x1F);
    
    // Check response code
    if (packet && packet->code == COAP_231_CONTINUE) {
        LOG("custom_coap: Block %lu acknowledged (current offset=%zu)\n", 
            (unsigned long)state->currentBlock, state->offset);
        
        // Update last acknowledged block
        if (state->currentBlock > state->lastAckedBlock) {
            state->lastAckedBlock = state->currentBlock;
        }
        
        // Move to next block - increment offset by the actual block size that was sent
        size_t last_block_size = (state->dataLen - state->offset > state->blockSize) ? 
                                  state->blockSize : (state->dataLen - state->offset);
        state->offset += last_block_size;
        state->currentBlock++;
        
        LOG("custom_coap: After update: block=%lu, offset=%zu\n",
            (unsigned long)state->currentBlock, state->offset);
        
        // Check if there are more blocks to send
        if (state->offset < state->dataLen) {
            // Send next window of blocks if we haven't already sent them
            if (state->nextBlockToSend <= state->currentBlock) {
                send_block_window(contextP, state);
            }
        } else {
            LOG("custom_coap: ✅ All blocks sent successfully - total %zu bytes (offset reached: %zu)\n", 
                state->dataLen, state->offset);
            g_transfer_in_progress = false;
            lwm2m_free(state);
        }
    } else if (packet && packet->code == COAP_204_CHANGED) {
        LOG("custom_coap: ✅ Transfer complete - %zu bytes total (offset: %zu)\n", 
            state->dataLen, state->offset);
        g_transfer_in_progress = false;
        lwm2m_free(state);
    } else {
        LOG("custom_coap: Block transfer failed with code %d.%02d\n",
            packet ? packet->code >> 5 : 0, packet ? packet->code & 0x1F : 0);
        g_transfer_in_progress = false;
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
    
    // Check if a transfer is already in progress
    if (g_transfer_in_progress) {
        LOG("custom_coap: Transfer already in progress, rejecting new request\n");
        return -6;  // New error code for transfer in progress
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
    // Use 1024 byte blocks for faster transfer (optimal for WiFi networks)
    const uint16_t BLOCK_SIZE = 1024;
    
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
    state->nextBlockToSend = 0;  // Initialize pipeline tracking
    state->lastAckedBlock = 0;
    
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
    
    // Mark transfer as in progress
    g_transfer_in_progress = true;
    
    // Send first window of blocks (pipelined)
    LOG("custom_coap: Sending initial window of %d blocks\n", COAP_PIPELINE_WINDOW);
    send_block_window(contextP, state);
    
    LOG("custom_coap: Initial blocks sent, waiting for acknowledgments\n");
    return 0;
}
