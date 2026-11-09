#include "device/sdcard.hpp"

#include <dirent.h>
#include <string.h>
#include <strings.h>

#include "driver/sdspi_host.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include <M5Unified.hpp>

// SPI 接続とマウントの手順は docs/cardputer-adv-peripherals.md の 9 章に基づく。

namespace sdcard {

static const char *TAG = "sdcard";

// Cardputer-Adv の基板で SD スロットに固定配線されているピン
static constexpr gpio_num_t PIN_SCLK = GPIO_NUM_40;
static constexpr gpio_num_t PIN_MOSI = GPIO_NUM_14;
static constexpr gpio_num_t PIN_MISO = GPIO_NUM_39;
static constexpr gpio_num_t PIN_CS   = GPIO_NUM_12;
static constexpr const char *MOUNT_POINT = "/sdcard";

static bool mounted = false;

/**
 * @brief SPI バスを初期化して SD カードを MOUNT_POINT にマウントする
 *
 * 失敗したときは画面とログにエラーを出し、初期化した SPI バスを解放して false を返す。
 */
static bool mount()
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
        M5.Display.printf("SPI init failed\n0x%x\n", (unsigned)ret);
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
        M5.Display.printf("SD mount failed\n%s\n0x%x\n", kind, (unsigned)ret);
        // バスを解放しておく。しないと、再度 begin() したときに spi_bus_initialize が失敗する
        spi_bus_free((spi_host_device_t)host.slot);
        return false;
    }

    ESP_LOGI(TAG, "mounted: %s", card->cid.name);
    return true;
}

/**
 * @brief ファイルの拡張子が .wav（大文字小文字を区別しない）か
 *
 * macOS がコピー時に作る隠しファイル（._ で始まる名前）は音声ではないので除く。
 */
static bool isWav(const struct dirent *ent)
{
    if (ent->d_type == DT_DIR) return false;
    if (ent->d_name[0] == '.') return false;
    const char *dot = strrchr(ent->d_name, '.');
    return dot && strcasecmp(dot, ".wav") == 0;
}

bool begin()
{
    if (mounted) return true;
    mounted = mount();
    return mounted;
}

bool listWav(std::vector<std::string> &names)
{
    names.clear();
    DIR *dir = opendir(MOUNT_POINT);
    if (!dir) {
        ESP_LOGE(TAG, "opendir failed: %s", MOUNT_POINT);
        return false;
    }

    int count = 0;
    struct dirent *ent;
    while ((ent = readdir(dir)) != nullptr) {
        if (!isWav(ent)) continue;
        ESP_LOGI(TAG, "wav: %s", ent->d_name);
        names.push_back(ent->d_name);
        count++;
    }
    closedir(dir);

    ESP_LOGI(TAG, "wav files: %d", count);
    return true;
}

std::string wavPath(const std::string &name)
{
    return std::string(MOUNT_POINT) + "/" + name;
}

}  // namespace sdcard
