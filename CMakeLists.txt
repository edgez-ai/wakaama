idf_component_register(
    SRC_DIRS
        "coap/"
        "coap/er-coap-13/"
        "examples/shared"
        "examples/client"
        "examples/shared/tinydtls"
        "examples/shared/tinydtls/aes"
        "examples/shared/tinydtls/sha2"
        "examples/shared/tinydtls/ecc"
        "core/"
        "data/"

    PRIV_INCLUDE_DIRS
        "core"
        "coap/"
        "examples/shared/tinydtls"
        "examples/shared/tinydtls/aes"
        "examples/shared/tinydtls/sha2"
        "examples/shared/tinydtls/ecc"
        "examples/client"
        "examples/shared"

    INCLUDE_DIRS
        "include" "examples/shared" "examples/shared/tinydtls" "examples/client"
    REQUIRES "esp_hw_support" "esp_https_ota" "app_update" "esp_http_client"
    )

target_compile_definitions(${COMPONENT_TARGET}
                    PUBLIC
                    "LWM2M_CLIENT_MODE"
                    "LWM2M_COAP_DEFAULT_BLOCK_SIZE=512"
                    "LWM2M_COAP_MAX_MESSAGE_SIZE=48384"
                                        "LWM2M_COAP_DEFAULT_MAX_RETRANSMIT=2"
                                        "LWM2M_COAP_SEPARATE_TIMEOUT=30"
                    "LWM2M_SUPPORT_SENML_JSON"
                    "LWM2M_SUPPORT_TLV"
                    "LWM2M_SUPPORT_JSON"
                    "LWM2M_BOOTSTRAP"
                  #  "LWM2M_WITH_LOGS"
                    "WITH_TINYDTLS"
                    "LWM2M_LITTLE_ENDIAN"
                    "ESPIDF_VERSION"
                    "SHA2_USE_INTTYPES_H"
                    "DTLS_CHECK_CONTENTTYPE"
                    "DTLS_CONSTRAINED_STACK"
                    "WITH_LWIP"
                    "WITH_SHA256"
                    "WITH_ECC"
                    )

set(USES_TERMINAL_DOWNLOAD TRUE)
set(USES_TERMINAL_CONFIGURE TRUE)
set(USES_TERMINAL_BUILD TRUE)
