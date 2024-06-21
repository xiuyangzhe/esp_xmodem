idf_component_register(SRCS "uart_async_rxtxtasks_main.c"
                    INCLUDE_DIRS "."
                    REQUIRES wifi_connect esp_xmodem esp_http_client mbedtls nvs_flash
                    )
