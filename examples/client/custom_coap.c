/*
 * Custom CoAP message sending utilities for LwM2M client
 * Provides helper functions to send custom CoAP messages through existing connections
 */

#include "liblwm2m.h"
#include "internals.h"
#include "er-coap-13/er-coap-13.h"

/**
 * Send binary data to a custom CoAP endpoint
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
    
    // Create a CoAP transaction for POST request
    lwm2m_transaction_t *transaction = transaction_new(
        server->sessionH,
        COAP_POST,
        NULL,  // no altPath
        NULL,  // no URI (we'll set path directly)
        contextP->nextMID++,
        0,     // token_len
        NULL   // token
    );
    
    if (!transaction) {
        return -4;
    }
    
    // Set URI path
    coap_set_header_uri_path(transaction->message, path);
    
    // Set content type
    coap_set_header_content_type(transaction->message, contentType);
    
    // Set the payload (binary data)
    coap_set_payload(transaction->message, data, dataLen);
    
    // Add transaction to the context's transaction list
    contextP->transactionList = (lwm2m_transaction_t*)LWM2M_LIST_ADD(
        contextP->transactionList, transaction);
    
    // Send the transaction
    int result = transaction_send(contextP, transaction);
    
    return result;
}
