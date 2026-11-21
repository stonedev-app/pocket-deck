#include "audio/volume.hpp"

#include "esp_log.h"
#include "freertos/FreeRTOS.h" // IWYU pragma: keep
#include "freertos/task.h"
#include "freertos/timers.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "device/speaker.hpp"
#include "util/ticks.hpp"

namespace volume {

static const char *TAG = "volume";

static constexpr const char *NVS_NAMESPACE = "pocket-deck";
static constexpr const char *NVS_KEY = "vol";
// 最後の変更から書き込みまで。キーを連打するたびに書くとフラッシュの書き込み回数（寿命）を
// 消費し、書き込み中は他の NVS アクセスもブロックされるので、操作が落ち着いてから 1 回だけ書く
static constexpr TickType_t SAVE_DELAY = msToTicks(1000);  // [ms]

static int s_level = DEFAULT_LEVEL;
static bool s_muted = false;
static TimerHandle_t s_saveTimer = nullptr;  // 最後の変更から SAVE_DELAY 後に save() を呼ぶ one-shot
static bool s_nvsReady = false;

// 段階（0〜16）から setVolume に渡す値（0〜255）への対応表。下の側を細かくしてある。
// 人の耳は音量を対数的に感じるので、等間隔にすると上の段階だけ大きく変わり、下の段階は差が分からなくなる。
// 値は実機で聞いて調整するので、直すときはここだけを変える。
static constexpr uint8_t VOLUME_TABLE[MAX_LEVEL + 1] = {
    0,                                                    // 0 段目: 無音
    6, 8, 10, 13, 16, 21, 27, 35, 44, 57, 73, 94, 120, 154, 198, 255,  // 1〜16 段目
};

/** @brief 現在の段階と消音をスピーカーに反映する。消音中は 0、それ以外は表から引く */
static void apply()
{
    uint8_t v = s_muted ? 0 : VOLUME_TABLE[s_level];
    speaker::setVolume(v);
}

/**
 * @brief NVS を初期化し、保存済みの段階があれば s_level に読み込む
 *
 * 保存がない・読めないときは s_level を変えず、既定値のまま続ける。
 * NVS の初期化に失敗すると s_nvsReady が false のままになり、save() は何もしない。
 */
static void load()
{
    // NVS の初期化はこのモジュールが担当する（他に NVS を使う所がないため）。
    // 空きページがない・新しい版で作られた領域のときは、消して作り直す。音量の設定が消えるだけで済む
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

/** @brief 現在の段階 s_level を NVS に書き込む。NVS が使えないとき（s_nvsReady が false）は何もしない */
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

/**
 * @brief 保存タイマーが切れたときに save() を呼ぶ
 *
 * タイマータスクで動く。s_level を読むだけにする（NVS 書き込みのブロックはタイマータスクだけが受ける）。
 */
static void onSaveTimer(TimerHandle_t)
{
    save();
}

/** @brief 保存タイマーを張り直す。連打しても最後の変更から SAVE_DELAY 後に 1 回だけ保存される */
static void markChanged()
{
    if (s_saveTimer) xTimerReset(s_saveTimer, 0);
}

void begin()
{
    // one-shot（第 3 引数 pdFALSE）。変更のたびに markChanged() が張り直すので、連打の間は発火しない
    s_saveTimer = xTimerCreate("vol_save", SAVE_DELAY, pdFALSE, nullptr, onSaveTimer);
    if (!s_saveTimer) ESP_LOGE(TAG, "save timer create failed");
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

int level() { return s_level; }
bool muted() { return s_muted; }

}  // namespace volume
