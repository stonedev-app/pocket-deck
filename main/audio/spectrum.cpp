#include "audio/spectrum.hpp"

#include <math.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"  // IWYU pragma: keep
#include "freertos/task.h"
#include "task_config.hpp"

namespace spectrum {

static const char *TAG = "spectrum";

// FFT の代わりに Goertzel 法で、12 帯域の中心周波数だけを求める。FFT のバッファや係数表が要らず、
// 窓 512 フレーム × 12 帯域 = 約 6000 回の積和を、チャンクごとに 1 回行うだけで済む（CPU の目安は PR 参照）
static constexpr size_t WINDOW = 512;     // 1 回の計算に使うフレーム数
static constexpr float F_MIN = 100.0f;    // 最も低い帯域の中心周波数 [Hz]
static constexpr float F_MAX = 8000.0f;   // 最も高い帯域の中心周波数 [Hz]
static constexpr float DB_FLOOR = -60.0f; // この dB 以下をレベル 0 にする（フルスケールの正弦波が 0 dB）
static constexpr float DB_CEIL = -12.0f;  // この dB 以上をレベル 100 にする

// 計算は専用のタスクで行う（優先度・コア・スタックは task_config.hpp）

static float s_mono[WINDOW];                 // 窓のモノラル化したデータ。計算のタスクだけが使う
static int16_t s_copy[WINDOW * 2];           // submit() が渡す窓のコピー（ステレオなら L/R 交互）
static volatile bool s_busy = false;         // true の間は計算のタスクが s_copy を使う。submit() は触らない
static volatile bool s_stereo = false;       // s_copy がステレオか
static volatile bool s_active = false;       // start() から stop() までの間 true。false の間は結果を書かない
static TaskHandle_t s_task = nullptr;
static float s_coeff[BANDS];                 // Goertzel の係数 2cos(w)。0 の帯域は計算しない
static volatile uint8_t s_level[BANDS];      // 読み出し用のレベル（0〜100）

/** @brief レベルをすべて 0 に戻す */
static void clearLevels()
{
    for (int b = 0; b < BANDS; b++) s_level[b] = 0;
}

/** @brief s_copy の窓から、各帯域のレベルを求める（計算のタスクで呼ぶ） */
static void compute()
{
    const int16_t *data = s_copy;
    const bool stereo = s_stereo;
    uint8_t out[BANDS];
    for (size_t i = 0; i < WINDOW; i++) {
        s_mono[i] = stereo ? 0.5f * ((float)data[2 * i] + (float)data[2 * i + 1]) : (float)data[i];
    }
    for (int b = 0; b < BANDS; b++) {
        const float c = s_coeff[b];
        if (c == 0.0f) {
            out[b] = 0;
            continue;
        }
        float s1 = 0.0f, s2 = 0.0f;
        for (size_t i = 0; i < WINDOW; i++) {
            const float s = s_mono[i] + c * s1 - s2;
            s2 = s1;
            s1 = s;
        }
        float power = s1 * s1 + s2 * s2 - c * s1 * s2;
        if (power < 0.0f) power = 0.0f;
        // 振幅をフルスケール（32768）に対する比にそろえる。正弦波なら 1.0 で 0 dB
        const float amp = 2.0f * sqrtf(power) / ((float)WINDOW * 32768.0f);
        float level = 0.0f;
        if (amp > 1e-6f) {
            level = (20.0f * log10f(amp) - DB_FLOOR) / (DB_CEIL - DB_FLOOR) * 100.0f;
        }
        if (level < 0.0f) level = 0.0f;
        if (level > 100.0f) level = 100.0f;
        out[b] = (uint8_t)level;
    }
    // 再生が終わっていたら書かない（stop() でクリアした 0 を上書きしないため）
    if (!s_active) return;
    for (int b = 0; b < BANDS; b++) s_level[b] = out[b];
}

/** @brief 計算のタスク。通知を受けるたびに s_copy を計算する。作りっぱなしで、begin() が 1 回だけ作る */
static void taskFn(void *)
{
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        compute();
        s_busy = false;
    }
}

bool begin()
{
    if (s_task) return true;
    if (xTaskCreatePinnedToCore(taskFn, "spectrum", task_config::SPECTRUM.stackBytes, NULL,
                                task_config::SPECTRUM.priority, &s_task, task_config::SPECTRUM.core) != pdPASS) {
        ESP_LOGE(TAG, "task create failed");
        s_task = nullptr;
        return false;
    }
    return true;
}

void start(uint32_t sampleRate)
{
    // 係数を作る。ナイキスト周波数に近い帯域は使わない
    for (int b = 0; b < BANDS; b++) {
        const float f = F_MIN * powf(F_MAX / F_MIN, (float)b / (BANDS - 1));
        if (sampleRate == 0 || f >= 0.45f * (float)sampleRate) {
            s_coeff[b] = 0.0f;
        } else {
            s_coeff[b] = 2.0f * cosf(2.0f * (float)M_PI * f / (float)sampleRate);
        }
    }
    clearLevels();
    s_active = true;
}

void stop()
{
    s_active = false;  // 先に下ろす。その後のクリアを、計算側が上書きしない
    clearLevels();
}

void submit(const int16_t *data, size_t n, bool stereo)
{
    const size_t frames = stereo ? n / 2 : n;
    if (frames < WINDOW || s_busy || !s_task) return;
    memcpy(s_copy, data, WINDOW * (stereo ? 2 : 1) * sizeof(int16_t));
    s_stereo = stereo;
    s_busy = true;
    xTaskNotifyGive(s_task);
}

void levels(uint8_t out[BANDS])
{
    for (int b = 0; b < BANDS; b++) out[b] = s_level[b];
}

}  // namespace spectrum
