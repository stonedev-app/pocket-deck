idf_component_register(SRCS "pocket-deck.cpp" "device/keyboard.cpp" "device/sdcard.cpp" "audio/wav.cpp" "audio/player.cpp" "audio/spectrum.cpp""audio/volume.cpp" "ui/ui.cpp" "ui/lvgl_port.cpp" "ui/status_bar.cpp" "ui/file_list.cpp" "ui/now_playing.cpp" "ui/fonts/font_jp_16.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES nvs_flash fatfs sdmmc esp_driver_sdspi esp_driver_spi driver esp_timer)
