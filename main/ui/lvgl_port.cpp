#include "ui/lvgl_port.hpp"

#include <stdint.h>
#include <stdio.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"  // IWYU pragma: keep
#include "freertos/task.h"
#include "lvgl.h"               // IWYU pragma: keep
#include "task_config.hpp"
#include "util/ticks.hpp"

namespace lvgl_port {

static const char *TAG = "lvgl_port";

// start() で受け取った、画面への転送の関数とキーの取り出しの関数
static FlushFn s_flush = nullptr;
static PollKeyFn s_pollKey = nullptr;

// 描画バッファの行数。画面全体（240x135x2 = 約 63KB）ではなく、数行ずつ描いて転送する（PARTIAL）。
// PSRAM がないので、内部 RAM に小さく取る。40 行で約 19.2KB。
// 描き直す領域がこの行数を超えると、分けて描く回数だけ手間が増える。曲名（高さ 20 行強）と
// スペクトルアナライザーが 1〜2 回で描けるよう、20 行から増やした（#57）
static constexpr int BUF_LINES = 40;

// lv_timer_handler() を呼ぶ間隔の下限と上限 [ms]。実際の待ち時間は lv_timer_handler() が返す値を使う
static constexpr uint32_t HANDLER_MIN_MS = 5;
static constexpr uint32_t HANDLER_MAX_MS = 50;

/** @brief LVGL に渡す時刻 [ms]。アニメーションや再描画の周期の基準になる */
static uint32_t tickCb()
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

/**
 * @brief LVGL が描いた領域を、渡された転送の関数（s_flush）で画面に出す
 *
 * LV_COLOR_DEPTH=16（RGB565）の画素をそのまま渡す。色がおかしい場合は、バイト順（swap）を疑う。
 * 転送は同期で終わるので、その場で lv_display_flush_ready() を呼ぶ。
 */
static void flushCb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    const int32_t w = area->x2 - area->x1 + 1;
    const int32_t h = area->y2 - area->y1 + 1;
    s_flush(area->x1, area->y1, w, h, reinterpret_cast<const uint16_t *>(px_map));
    lv_display_flush_ready(disp);
}

/**
 * @brief LVGL のキー入力デバイスの読み出し
 *
 * キーボードは「押された瞬間」しか出さない（離上は出ない）ので、1 件取れたら押下を返し、
 * 続けて離上を返す（continue_reading で、同じ周期の中でもう一度呼ばせる）。
 * LVGL のタスクの中で呼ばれる。
 */
static void keypadReadCb(lv_indev_t *, lv_indev_data_t *data)
{
    static bool releasePending = false;
    static uint32_t lastKey = 0;

    data->key = lastKey;
    data->state = LV_INDEV_STATE_RELEASED;
    if (releasePending) {
        releasePending = false;
        return;
    }
    uint32_t key;
    if (s_pollKey(key)) {
        lastKey = key;
        releasePending = true;
        data->key = lastKey;
        data->state = LV_INDEV_STATE_PRESSED;
        data->continue_reading = true;
    }
}

/**
 * @brief LVGL のタスク。lv_timer_handler() を周期的に呼んで、描画とアニメーションを進める
 *
 * 待ち時間は lv_timer_handler() が返す「次に呼ぶまでの ms」を HANDLER_MIN_MS〜HANDLER_MAX_MS に
 * 収めて使う。msToTicks() は 0 を返さない（0 だとタスクが CPU を占有し、ウォッチドッグが作動する）。
 */
static void lvglTask(void *)
{
    while (1) {
        uint32_t ms = lv_timer_handler();
        if (ms < HANDLER_MIN_MS) ms = HANDLER_MIN_MS;
        if (ms > HANDLER_MAX_MS) ms = HANDLER_MAX_MS;
        vTaskDelay(msToTicks(ms));
    }
}

bool start(int32_t width, int32_t height, FlushFn flush, PollKeyFn pollKey, BuildUiFn buildUi)
{
    if (!flush || !pollKey || !buildUi || width <= 0 || height <= 0) {
        ESP_LOGE(TAG, "invalid arguments");
        return false;
    }
    s_flush = flush;
    s_pollKey = pollKey;
    const int32_t w = width;
    const int32_t h = height;
    ESP_LOGI(TAG, "display %d x %d, lvgl %d.%d.%d", (int)w, (int)h, LVGL_VERSION_MAJOR,
             LVGL_VERSION_MINOR, LVGL_VERSION_PATCH);

    lv_init();
    lv_tick_set_cb(tickCb);

    lv_display_t *disp = lv_display_create(w, h);
    if (!disp) {
        ESP_LOGE(TAG, "lv_display_create failed");
        return false;
    }

    const size_t bufBytes = (size_t)w * BUF_LINES * sizeof(uint16_t);
    void *buf = heap_caps_malloc(bufBytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!buf) {
        ESP_LOGE(TAG, "draw buffer alloc failed (%u bytes)", (unsigned)bufBytes);
        return false;
    }
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(disp, buf, nullptr, bufBytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, flushCb);

    // キー入力デバイス。項目の選択（フォーカス）はグループで管理し、キーボードをそのグループに結びつける
    lv_group_t *group = lv_group_create();
    lv_group_set_default(group);
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(indev, keypadReadCb);
    lv_indev_set_group(indev, group);

    // タスクを起動する前なので、ここで LVGL を触ってよい
    buildUi();

    if (xTaskCreatePinnedToCore(lvglTask, "lvgl", task_config::LVGL.stackBytes, nullptr,
                                task_config::LVGL.priority, nullptr, task_config::LVGL.core) != pdPASS) {
        ESP_LOGE(TAG, "task create failed");
        return false;
    }
    return true;
}

}  // namespace lvgl_port
