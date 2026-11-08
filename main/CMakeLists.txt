idf_component_register(SRCS "pocket-deck.cpp" "device/keyboard.cpp" "device/sdcard.cpp" "audio/wav.cpp" "audio/player.cpp" "audio/volume.cpp" "ui/ui.cpp"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES nvs_flash fatfs sdmmc esp_driver_sdspi esp_driver_spi driver)
