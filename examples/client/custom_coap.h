/*
 * Custom CoAP message sending utilities for LwM2M client
 * Provides helper functions to send custom CoAP messages through existing connections
 * with proper CoAP Block1 wise transfer support for DTLS
 */

#ifndef CUSTOM_COAP_H_
#define CUSTOM_COAP_H_

#include "liblwm2m.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Send binary data to a custom CoAP endpoint via POST with Block1 wise transfer
 * 
 * This function uses CoAP Block1 wise transfer (RFC 7959) to send large binary
 * data reliably over DTLS. Each block is automatically encrypted by the DTLS layer.
 * 
 * Block size is set to 1024 bytes, which fits comfortably within typical MTU sizes
 * while leaving room for CoAP headers (~20 bytes) and DTLS overhead (~13-29 bytes).
 * 
 * @param contextP LwM2M context pointer (must be in STATE_READY)
 * @param path URI path for the CoAP POST request (e.g., "image")
 * @param data Binary data to send
 * @param dataLen Length of binary data in bytes
 * @param contentType CoAP content type (e.g., 42 for application/octet-stream, 22 for image/jpeg)
 * 
 * @return 0 on success, negative error code on failure:
 *         -1: Invalid parameters (NULL pointers or zero length)
 *         -2: LwM2M context not ready (state != STATE_READY)
 *         -3: No server connection available (no server or session)
 *         -4: Failed to create transaction (out of memory)
 *         Other: Error from transaction_send (network or DTLS error)
 * 
 * @note This function sends blocks synchronously. For large transfers, ensure
 *       adequate task stack size and watchdog timeout.
 * @note Each block is sent as a separate DTLS record, providing encryption
 *       and authentication per block.
 */
int lwm2m_send_coap_post(lwm2m_context_t *contextP, const char *path, 
                          const uint8_t *data, size_t dataLen, 
                          unsigned int contentType);

#ifdef __cplusplus
}
#endif

#endif /* CUSTOM_COAP_H_ */
