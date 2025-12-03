/*
 * Custom CoAP message sending utilities for LwM2M client
 * Provides helper functions to send custom CoAP messages through existing connections
 */

#ifndef CUSTOM_COAP_H_
#define CUSTOM_COAP_H_

#include "liblwm2m.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Send binary data to a custom CoAP endpoint via POST
 * 
 * @param contextP LwM2M context pointer
 * @param path URI path for the CoAP POST request (e.g., "image")
 * @param data Binary data to send
 * @param dataLen Length of binary data
 * @param contentType CoAP content type (e.g., 42 for application/octet-stream)
 * @return 0 on success, negative error code on failure:
 *         -1: Invalid parameters
 *         -2: LwM2M context not ready
 *         -3: No server connection available
 *         -4: Failed to create transaction
 *         Other: Error from transaction_send
 */
int lwm2m_send_coap_post(lwm2m_context_t *contextP, const char *path, 
                          const uint8_t *data, size_t dataLen, 
                          unsigned int contentType);

#ifdef __cplusplus
}
#endif

#endif /* CUSTOM_COAP_H_ */
