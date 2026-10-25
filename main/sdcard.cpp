#include "sdcard.hpp"

#include <dirent.h>
#include <string.h>
#include <strings.h>

#include "driver/gpio.h"
#include "driver/sdspi_host.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include <M5Unified.hpp>

// SPI 接続とマウントの手順は docs/cardputer-adv-peripherals.md の 9 章に基づく。

namespace sdcard {

static const char *TAG = "sdcard";

static constexpr gpio_num_t PIN_SCLK = GPIO_NUM_40;
static constexpr gpio_num_t PIN_MOSI = GPIO_NUM_14;
static constexpr gpio_num_t PIN_MISO = GPIO_NUM_39;
static constexpr gpio_num_t PIN_CS   = GPIO_NUM_12;
static constexpr const char *MOUNT_POINT = "/sdcard";

// 画面は 240x135。文字サイズ 2（12x16）で 1 行 16 ピクセルなので、見出し 1 行を除いて 7 行まで入る
static constexpr int MAX_DISPLAY_FILES = 7;

static bool mount()
{
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();

    spi_bus_config_t bus_cfg = {};
    bus_cfg.mosi_io_num     = PIN_MOSI;
    bus_cfg.miso_io_num     = PIN_MISO;
    bus_cfg.sclk_io_num     = PIN_SCLK;
    bus_cfg.quadwp_io_num   = -1;
    bus_cfg.quadhd_io_num   = -1;
    bus_cfg.max_transfer_sz = 4000;
    esp_err_t ret = spi_bus_initialize((spi_host_device_t)host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_initialize failed: %s (0x%x)", esp_err_to_name(ret), (unsigned)ret);
        M5.Display.printf("SPI init failed\n0x%x\n", (unsigned)ret);
        return false;
    }

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {};
    mount_config.format_if_mount_failed = false;
    mount_config.max_files              = 5;
    mount_config.allocation_unit_size   = 16 * 1024;

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_CS;
    slot_config.host_id = (spi_host_device_t)host.slot;

    sdmmc_card_t *card = nullptr;
    ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &card);
    if (ret != ESP_OK) {
        // ESP_FAIL はファイルシステムのマウント失敗、それ以外はカードの初期化失敗
        const char *kind = (ret == ESP_FAIL) ? "filesystem" : "card init";
        ESP_LOGE(TAG, "mount failed (%s): %s (0x%x)", kind, esp_err_to_name(ret), (unsigned)ret);
        M5.Display.printf("SD mount failed\n%s\n0x%x\n", kind, (unsigned)ret);
        spi_bus_free((spi_host_device_t)host.slot);
        return false;
    }

    ESP_LOGI(TAG, "mounted: %s", card->cid.name);
    return true;
}

// ファイルの拡張子が .wav（大文字小文字を区別しない）か
static bool isWav(const struct dirent *ent)
{
    if (ent->d_type == DT_DIR) return false;
    const char *dot = strrchr(ent->d_name, '.');
    return dot && strcasecmp(dot, ".wav") == 0;
}

bool showWavList()
{
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setCursor(0, 0);
    M5.Display.setTextSize(2);

    if (!mount()) return false;

    DIR *dir = opendir(MOUNT_POINT);
    if (!dir) {
        ESP_LOGE(TAG, "opendir failed: %s", MOUNT_POINT);
        M5.Display.println("opendir failed");
        return false;
    }

    M5.Display.println("WAV files:");
    int count = 0;
    struct dirent *ent;
    while ((ent = readdir(dir)) != nullptr) {
        if (!isWav(ent)) continue;
        ESP_LOGI(TAG, "wav: %s", ent->d_name);
        // 画面に入らない分はログだけに出す
        if (count < MAX_DISPLAY_FILES) {
            M5.Display.println(ent->d_name);
        }
        count++;
    }
    closedir(dir);

    ESP_LOGI(TAG, "wav files: %d", count);
    if (count == 0) {
        M5.Display.println("(none)");
    }
    return true;
}

}  // namespace sdcard
