# ESP32 compatible CMakeLists.txt for Wakaama LWM2M Client with TinyDTLS
cmake_minimum_required(VERSION 3.21)

# Wakaama configuration for ESP32
set(WAKAAMA_MODE_CLIENT ON)
set(WAKAAMA_MODE_SERVER OFF)
set(WAKAAMA_MODE_BOOTSTRAP_SERVER OFF)
set(WAKAAMA_CLIENT_INITIATED_BOOTSTRAP ON)
set(WAKAAMA_DATA_SENML_JSON ON)
set(WAKAAMA_DATA_SENML_CBOR OFF)
set(WAKAAMA_CLI ON)
set(WAKAAMA_TRANSPORT TINYDTLS)
set(WAKAAMA_PLATFORM ESP32)

# ESP-IDF component registration
idf_component_register(
    SRCS
        # Core Wakaama sources
        "coap/block.c"
        "coap/transaction.c"
        "coap/er-coap-13/er-coap-13.c"
        "core/bootstrap.c"
        "core/discover.c"
        "core/liblwm2m.c"
        "core/list.c"
        "core/logging.c"
        "core/management.c"
        "core/objects.c"
        "core/observe.c"
        "core/packet.c"
        "core/registration.c"
        "core/reporting.c"
        "core/uri.c"
        "core/utils.c"
    "core/getline_compat.c"
        "data/cbor.c"
        "data/data.c"
        "data/json_common.c"
        "data/json.c"
        "data/senml_cbor.c"
        "data/senml_common.c"
        "data/senml_json.c"
        "data/tlv.c"
        
        # Client common sources (equivalent to lwm2mclient_tinydtls sources)
        "examples/client/common/lwm2mclient.c"
        "examples/client/common/object_access_control.c"
        "examples/client/common/object_connectivity_moni.c"
        "examples/client/common/object_connectivity_stat.c"
        "examples/client/common/object_device.c"
        "examples/client/common/object_firmware.c"
        "examples/client/common/object_location.c"
        "examples/client/common/object_security.c"
        "examples/client/common/object_server.c"
        "examples/client/common/object_test.c"
        "examples/client/common/system_api.c"
        
        # Shared platform sources
        "examples/shared/platform.c"
        "examples/shared/commandline.c"
        
        # TinyDTLS transport layer
        "transport/tinydtls/connection.c"
        
        # TinyDTLS third-party sources
        "transport/tinydtls/third_party/tinydtls/dtls.c"
        "transport/tinydtls/third_party/tinydtls/crypto.c"
        "transport/tinydtls/third_party/tinydtls/hmac.c"
        "transport/tinydtls/third_party/tinydtls/dtls_debug.c"
        "transport/tinydtls/third_party/tinydtls/dtls_prng.c"
        "transport/tinydtls/third_party/tinydtls/netq.c"
        "transport/tinydtls/third_party/tinydtls/peer.c"
        "transport/tinydtls/third_party/tinydtls/dtls_time.c"
        "transport/tinydtls/third_party/tinydtls/session.c"
        "transport/tinydtls/third_party/tinydtls/ccm.c"
        "transport/tinydtls/third_party/tinydtls/aes/rijndael.c"
        "transport/tinydtls/third_party/tinydtls/sha2/sha2.c"
        "transport/tinydtls/third_party/tinydtls/ecc/ecc.c"

    INCLUDE_DIRS
        "include"
        "examples/shared"
        "transport/tinydtls/include"
        "examples/client/common"
        
    PRIV_INCLUDE_DIRS
        "core"
        "coap"
        "coap/er-coap-13"
        # TinyDTLS include root. We need the parent directory so that
        #   #include "tinydtls/dtls.h" resolves to third_party/tinydtls/dtls.h
        # (previously we added third_party/tinydtls which caused the preprocessor
        #  to search for third_party/tinydtls/tinydtls/dtls.h and fail.)
        "transport/tinydtls/third_party"
        "transport/tinydtls/third_party/tinydtls/aes"
        "transport/tinydtls/third_party/tinydtls/sha2"
        "transport/tinydtls/third_party/tinydtls/ecc"
        
    REQUIRES 
        "esp_hw_support"
        "lwip"
        "esp_netif"
        "nvs_flash"
    )

# Compile definitions matching the Wakaama configuration
target_compile_definitions(${COMPONENT_TARGET}
    PUBLIC
        # Core LWM2M definitions
        "LWM2M_CLIENT_MODE"
        "LWM2M_BOOTSTRAP"
        "LWM2M_SUPPORT_SENML_JSON"
        "LWM2M_COAP_DEFAULT_BLOCK_SIZE=512"
        "LWM2M_COAP_DEFAULT_MAX_RETRANSMIT=4"
        "LWM2M_COAP_SEPARATE_TIMEOUT=2000"
        "LWM2M_COAP_MAX_MESSAGE_SIZE=1152"
        
         #
        
        # Transport and platform definitions
        "WITH_TINYDTLS"
        "ESPIDF_VERSION"
        "WITH_LWIP"
        "WITH_SHA256"
        "WITH_ECC"
        
        # ESP32 specific definitions
    # Rely on ESP-IDF lwIP configuration (sdkconfig) for IPv4/IPv6 settings instead
    # of forcing them here. Defining LWIP_IPV6=0 caused a re-definition warning
    # against the ESP-IDF provided lwipopts.h (which sets it to 1).
        
        # Optional: Enable logging (uncomment if needed)
        # "LWM2M_WITH_LOGS"
    )

# ESP32 specific compiler flags
target_compile_options(${COMPONENT_TARGET}
    PRIVATE
        -Wno-unused-function
        -Wno-unused-variable
        -Wno-format
    )

set(USES_TERMINAL_DOWNLOAD TRUE)
set(USES_TERMINAL_CONFIGURE TRUE)
set(USES_TERMINAL_BUILD TRUE)
