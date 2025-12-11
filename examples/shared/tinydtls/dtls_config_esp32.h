#ifndef DTLS_CONFIG_H_
#define DTLS_CONFIG_H_

/* ESP32/Arduino specific configuration for TinyDTLS */

/* Define to 1 if building with ECC support. */
#define DTLS_ECC 1

/* Define to 1 if building with PSK support */
#define DTLS_PSK 1

/* ESP32 Arduino framework compatibility */
#define WITH_LWIP 1
#define WITH_SHA256 1
#define WITH_ECC 1

/* Disable POSIX to avoid conflicts */
#undef WITH_POSIX
#undef IS_WINDOWS

/* ESP32 has these headers */
#define HAVE_ASSERT_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_MEMORY_H 1
#define HAVE_STDINT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_UNISTD_H 1

/* ESP32 specific */
#define HAVE_SYS_TIME_H 1
#define HAVE_TIME_H 1

/* Random number generation */
#define HAVE_GETRANDOM 1

/* Network functions - ESP32 has these via LWIP */
#define HAVE_ARPA_INET_H 1
#define HAVE_NETINET_IN_H 1
#define HAVE_SYS_SOCKET_H 1

/* Size definitions */
#define SIZEOF_SIZE_T 4

/* Endianness - ESP32 is little endian */
#define WORDS_BIGENDIAN 0

/* Package info */
#define PACKAGE_BUGREPORT ""
#define PACKAGE_NAME "tinydtls"
#define PACKAGE_STRING "tinydtls 0.9.0"
#define PACKAGE_TARNAME "tinydtls"
#define PACKAGE_URL ""
#define PACKAGE_VERSION "0.9.0"

#endif /* DTLS_CONFIG_H_ */