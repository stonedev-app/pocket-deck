#include "volume.hpp"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <M5Unified.hpp>

namespace volume {

static const char *TAG = "volume";

static constexpr const char *NVS_NAMESPACE = "pocket-deck";
static constexpr const char *NVS_KEY = "vol";
static constexpr TickType_t SAVE_DELAY = pdMS_TO_TICKS(1000);  // 最後の変更から書き込みまで

static int s_level = DEFAULT_LEVEL;
static bool s_muted = false;
static bool s_dirty = false;        // 保存待ちの変更がある
static TickType_t s_changedAt = 0;  // 最後に段階を変えた時刻
static bool s_nvsReady = false;

/** @brief 現在の段階と消音を M5.Speaker に反映する。16 段目は 255 に丸める */
static void apply()
{
    int v = s_muted ? 0 : s_level * 16;
    if (v > 255) v = 255;
    M5.Speaker.setVolume((uint8_t)v);
}

static void markChanged()
{
    s_dirty = true;
    s_changedAt = xTaskGetTickCount();
}

static void load()
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs init failed: %s", esp_err_to_name(err));
        return;
    }
    s_nvsReady = true;

    nvs_handle_t h;
    err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &h);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return;  // まだ保存されていない。既定値のまま
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs open failed: %s", esp_err_to_name(err));
        return;
    }
    uint8_t v = 0;
    err = nvs_get_u8(h, NVS_KEY, &v);
    nvs_close(h);
    if (err == ESP_OK && v <= MAX_LEVEL) {
        s_level = v;
    } else if (err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(TAG, "nvs read failed: %s (value=%u)", esp_err_to_name(err), (unsigned)v);
    }
}

static void save()
{
    if (!s_nvsReady) return;
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs open failed: %s", esp_err_to_name(err));
        return;
    }
    err = nvs_set_u8(h, NVS_KEY, (uint8_t)s_level);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs write failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "saved: %d", s_level);
    }
}

void begin()
{
    load();
    s_muted = false;  // 消音は保存しない。起動時は必ず解除
    apply();
    ESP_LOGI(TAG, "level: %d/%d", s_level, MAX_LEVEL);
}

void up()
{
    s_muted = false;
    if (s_level < MAX_LEVEL) {
        s_level++;
        markChanged();
    }
    apply();
}

void down()
{
    s_muted = false;
    if (s_level > 0) {
        s_level--;
        markChanged();
    }
    apply();
}

void toggleMute()
{
    s_muted = !s_muted;
    apply();
}

void tick()
{
    if (s_dirty && (TickType_t)(xTaskGetTickCount() - s_changedAt) >= SAVE_DELAY) {
        s_dirty = false;
        save();
    }
}

int level() { return s_level; }
bool muted() { return s_muted; }

}  // namespace volume
