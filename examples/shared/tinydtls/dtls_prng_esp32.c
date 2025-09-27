/*
 * ESP32-specific PRNG implementation for TinyDTLS
 */

#include "dtls_config.h"

#ifdef WITH_TINYDTLS

#include "dtls_prng.h"
#include <stdlib.h>
#include <string.h>

#ifdef ESP_PLATFORM
#include "esp_random.h"
#endif

int dtls_prng(unsigned char *buf, size_t len) {
    if (!buf || len == 0) {
        return 0;
    }

#ifdef ESP_PLATFORM
    /* Use ESP32's hardware random number generator */
    for (size_t i = 0; i < len; i += 4) {
        uint32_t random_val = esp_random();
        size_t copy_len = (len - i) < 4 ? (len - i) : 4;
        memcpy(buf + i, &random_val, copy_len);
    }
#else
    /* Fallback to standard rand() - not cryptographically secure */
    for (size_t i = 0; i < len; i++) {
        buf[i] = rand() & 0xFF;
    }
#endif

    return len;
}

/**
 * Seeds the random number generator for DTLS operations on ESP32.
 * Uses ESP32's hardware random number generator for entropy.
 * 
 * @param seed The seed value (ignored on ESP32 as hardware RNG is used)
 */
void dtls_prng_init(unsigned seed) {
    // ESP32 has a hardware random number generator, so we don't need to seed it
    // The seed parameter is ignored - ESP32 uses hardware entropy
    (void)seed; // Suppress unused parameter warning
}

#endif /* WITH_TINYDTLS */