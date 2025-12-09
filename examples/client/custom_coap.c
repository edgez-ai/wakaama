/*
 * Custom CoAP message sending utilities for LwM2M client
 * Provides helper functions to send custom CoAP messages through existing connections
 */

#include "liblwm2m.h"
#include "internals.h"
#include "er-coap-13/er-coap-13.h"

/**
 * Send binary data to a custom CoAP endpoint with block-wise transfer
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
        return -1;
    }
    
    // Check if context is ready
    if (contextP->state != STATE_READY) {
        return -2;
    }
    
    // Get the first server from the server list
    lwm2m_server_t *server = contextP->serverList;
    if (!server || !server->sessionH) {
        return -3;
    }
    
    // Maximum safe payload size per block (leave room for CoAP headers)
    // DTLS max record is 16KB (16384), use 15KB to leave room for CoAP/DTLS overhead
    const size_t MAX_BLOCK_SIZE = 15360; // 15KB
    
    // If data fits in one message, send it directly
    if (dataLen <= MAX_BLOCK_SIZE) {
        lwm2m_transaction_t *transaction = transaction_new(
            server->sessionH,
            COAP_POST,
            NULL, NULL,
            contextP->nextMID++,
            0, NULL
        );
        
        if (!transaction) {
            return -4;
        }
        
        coap_set_header_uri_path(transaction->message, path);
        coap_set_header_content_type(transaction->message, contentType);
        coap_set_payload(transaction->message, data, dataLen);
        
        contextP->transactionList = (lwm2m_transaction_t*)LWM2M_LIST_ADD(
            contextP->transactionList, transaction);
        
        return transaction_send(contextP, transaction);
    }
    
    // Send data in chunks using multiple POST requests
    size_t offset = 0;
    int chunk_num = 0;
    
    while (offset < dataLen) {
        size_t chunk_size = (dataLen - offset > MAX_BLOCK_SIZE) ? MAX_BLOCK_SIZE : (dataLen - offset);
        
        lwm2m_transaction_t *transaction = transaction_new(
            server->sessionH,
            COAP_POST,
            NULL, NULL,
            contextP->nextMID++,
            0, NULL
        );
        
        if (!transaction) {
            return -4;
        }
        
        coap_set_header_uri_path(transaction->message, path);
        coap_set_header_content_type(transaction->message, contentType);
        coap_set_payload(transaction->message, data + offset, chunk_size);
        
        contextP->transactionList = (lwm2m_transaction_t*)LWM2M_LIST_ADD(
            contextP->transactionList, transaction);
        
        int result = transaction_send(contextP, transaction);
        if (result != 0) {
            return result;
        }
        
        offset += chunk_size;
        chunk_num++;
        
        // Small delay between chunks to avoid overwhelming the network
        // (wakaama will handle retransmissions)
    }
    
    return 0;
}
