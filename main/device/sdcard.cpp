#include "device/sdcard.hpp"

#include <stdio.h>

#include "driver/sdspi_host.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"

// SPI 接続とマウントの手順は docs/cardputer-adv-peripherals.md の 9 章に基づく。

namespace sdcard {

static const char *TAG = "sdcard";

// Cardputer-Adv の基板で SD スロットに固定配線されているピン
static constexpr gpio_num_t PIN_SCLK = GPIO_NUM_40;
static constexpr gpio_num_t PIN_MOSI = GPIO_NUM_14;
static constexpr gpio_num_t PIN_MISO = GPIO_NUM_39;
static constexpr gpio_num_t PIN_CS   = GPIO_NUM_12;

static bool mounted = false;

/**
 * @brief SPI バスを初期化して SD カードを MOUNT_POINT にマウントする
 *
 * 失敗したときは、ログにエラーを出し、理由を err に書き、初期化した SPI バスを解放して false を返す。
 */
static bool mount(char *err, size_t errSize)
{
    // SD カードは SDMMC ではなく SPI モードで配線されているので、SDSPI ホストを使う
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();

    spi_bus_config_t bus_cfg = {};
    bus_cfg.mosi_io_num     = PIN_MOSI;
    bus_cfg.miso_io_num     = PIN_MISO;
    bus_cfg.sclk_io_num     = PIN_SCLK;
    // WP / HD は SD カードでは使わない（クアッド SPI ではない）
    bus_cfg.quadwp_io_num   = -1;
    bus_cfg.quadhd_io_num   = -1;
    // 1 回の転送の上限。ESP-IDF の SD SPI サンプルと同じ値
    bus_cfg.max_transfer_sz = 4000;
    esp_err_t ret = spi_bus_initialize((spi_host_device_t)host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_initialize failed: %s (0x%x)", esp_err_to_name(ret), (unsigned)ret);
        snprintf(err, errSize, "SPI init failed\n0x%x", (unsigned)ret);
        return false;
    }

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {};
    // マウントに失敗してもフォーマットしない。カード上の WAV を誤って消さないため
    mount_config.format_if_mount_failed = false;
    // 同時に開けるファイル数。再生は 1 つしか開かないが、ESP-IDF のサンプルと同じ値にしてある
    mount_config.max_files              = 5;
    // フォーマット時のクラスタサイズ。上でフォーマットしないので実質使われず、サンプルの既定値のまま
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
        snprintf(err, errSize, "SD mount failed\n%s\n0x%x", kind, (unsigned)ret);
        // バスを解放しておく。しないと、再度 begin() したときに spi_bus_initialize が失敗する
        spi_bus_free((spi_host_device_t)host.slot);
        return false;
    }

    ESP_LOGI(TAG, "mounted: %s", card->cid.name);
    return true;
}

bool begin(char *err, size_t errSize)
{
    if (mounted) return true;
    mounted = mount(err, errSize);
    return mounted;
}

}  // namespace sdcard
