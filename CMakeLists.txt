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
        "include" "examples/shared" "examples/shared/tinydtls"
    REQUIRES "esp_hw_support"
    )

target_compile_definitions(${COMPONENT_TARGET}
                    PUBLIC
                    "LWM2M_CLIENT_MODE"
                    "LWM2M_COAP_DEFAULT_BLOCK_SIZE=512"
                    "LWM2M_SUPPORT_SENML_JSON"
                    "LWM2M_BOOTSTRAP"
                  #  "LWM2M_WITH_LOGS"
                    "WITH_TINYDTLS"
                    "ESPIDF_VERSION"
                    "WITH_LWIP"
                    "WITH_SHA256"
                    "WITH_ECC"
                    )

set(USES_TERMINAL_DOWNLOAD TRUE)
set(USES_TERMINAL_CONFIGURE TRUE)
set(USES_TERMINAL_BUILD TRUE)
