idf_component_register(SRCS "pocket-deck.cpp" "keyboard.cpp" "sdcard.cpp" "wav.cpp" "player.cpp" "volume.cpp"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES nvs_flash fatfs sdmmc esp_driver_sdspi esp_driver_spi driver)
