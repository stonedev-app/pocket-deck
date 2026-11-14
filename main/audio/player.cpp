#include "audio/player.hpp"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <string>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"  // IWYU pragma: keep
#include "freertos/queue.h"
#include "freertos/task.h"
#include <M5Unified.hpp>

#include "util/ticks.hpp"

namespace player {

static const char *TAG = "player";

// playRaw は渡したバッファを再生が終わるまで参照し続け、内部のキューにはポインタしか積まない。
// そのため、再生中のバッファを読み込みで上書きしないよう複数個を順番に使う。
// 1 個 2048 要素（4KB）× 3 個で、SD の読み出しが多少遅れても音が途切れにくい長さを確保しつつ、
// 内部 SRAM（PSRAM はない）を 12KB に抑える。音切れの有無は実機で調整する
static constexpr int BUF_COUNT = 3;        // バッファの個数。順番に使う
static constexpr size_t BUF_ELEMS = 2048;  // 1 個あたりの要素数（int16_t）
static constexpr uint8_t CHANNEL = 0;      // playRaw に明示するチャンネル。音量・停止も同じ番号で操作する
// ファイル読み出しと playRaw の呼び出しだけなので 4KB で足りる。
// 優先度はスピーカーのタスクと同じ 6（ui のタスクの 5 より高い）。再開後は SD の読み出しが遅く、
// 同じ 5 だと LVGL の描画に CPU を取られて、音のキューが空になるため
static constexpr uint32_t TASK_STACK = 4096;
static constexpr UBaseType_t TASK_PRIORITY = 6;
static constexpr int STOP_WAIT_MS = 2000;  // stop() がタスクの終了を待つ上限
// 待ちループの刻み。CONFIG_FREERTOS_HZ=100（1 tick = 10ms）では 5ms が 0 tick になり、
// vTaskDelay(0) では IDLE が走れずウォッチドッグが発火するため、必ず 1 tick 以上にする
static constexpr int POLL_MS = 20;
static constexpr TickType_t POLL_TICKS = msToTicks(POLL_MS);

// 内部 SRAM に静的に取る（PSRAM はない）。再生が終わるまで書き換えない・解放しない
static int16_t buf[BUF_COUNT][BUF_ELEMS];

static std::string playPath;
static wav::Info playInfo;
static volatile bool running = false;  // 再生用のタスクが動いている間 true
static volatile bool stopReq = false;  // stop() からタスクへの停止の要求
static volatile uint32_t playedBytes = 0;  // playRaw に渡したバイト数（data の先頭から）。再生位置の目安
static uint32_t startOffset = 0;           // タスクが読み始める data 内の位置 [byte]。再開で 0 以外になる
static bool paused = false;                // pause() で止めていて、resume() で続けられる間 true
static QueueHandle_t eventQ = nullptr;  // 終了通知のキュー

// ---- イコライザー用のレベル計算 ----
// FFT の代わりに Goertzel 法で、12 帯域の中心周波数だけを求める。FFT のバッファや係数表が要らず、
// 窓 512 フレーム × 12 帯域 = 約 6000 回の積和を、チャンクごとに 1 回行うだけで済む（CPU の目安は PR 参照）
static constexpr size_t EQ_WINDOW = 512;     // 1 回の計算に使うフレーム数
static constexpr float EQ_F_MIN = 100.0f;    // 最も低い帯域の中心周波数 [Hz]
static constexpr float EQ_F_MAX = 8000.0f;   // 最も高い帯域の中心周波数 [Hz]
static constexpr float EQ_DB_FLOOR = -60.0f; // この dB 以下をレベル 0 にする（フルスケールの正弦波が 0 dB）
static constexpr float EQ_DB_CEIL = -12.0f;  // この dB 以上をレベル 100 にする

// 計算は専用のタスクで行う。再生用のタスクは窓をコピーして知らせるだけで、計算を待たない。
// 優先度は、スピーカー・再生用のタスク（6）> LVGL（5）> この計算（1）の順。
// 計算は 1 回に約 6ms（最大約 12ms、実機で計測）かかるので、音の経路より低くして、音を優先する。
// 計算が間に合わなければ、そのチャンクは捨てる（棒の動きが粗くなるだけ）
static constexpr UBaseType_t EQ_TASK_PRIORITY = 1;
static constexpr uint32_t EQ_TASK_STACK = 3072;

static float eqMono[EQ_WINDOW];                 // 窓のモノラル化したデータ。計算のタスクだけが使う
static int16_t eqCopy[EQ_WINDOW * 2];           // 再生用のタスクが渡す窓のコピー（ステレオなら L/R 交互）
static volatile bool eqBusy = false;            // true の間は計算のタスクが eqCopy を使う。再生用のタスクは触らない
static volatile bool eqStereo = false;          // eqCopy がステレオか
static TaskHandle_t eqTask = nullptr;
static float eqCoeff[EQ_BANDS];                 // Goertzel の係数 2cos(w)。0 の帯域は計算しない
static volatile uint8_t eqLevel[EQ_BANDS];      // 読み出し用のレベル（0〜100）

/** @brief レベルをすべて 0 に戻す */
static void eqClear()
{
    for (int b = 0; b < EQ_BANDS; b++) eqLevel[b] = 0;
}

/** @brief サンプルレートに合わせて係数を作る。ナイキスト周波数に近い帯域は使わない */
static void eqPrepare(uint32_t sampleRate)
{
    for (int b = 0; b < EQ_BANDS; b++) {
        const float f = EQ_F_MIN * powf(EQ_F_MAX / EQ_F_MIN, (float)b / (EQ_BANDS - 1));
        if (sampleRate == 0 || f >= 0.45f * (float)sampleRate) {
            eqCoeff[b] = 0.0f;
        } else {
            eqCoeff[b] = 2.0f * cosf(2.0f * (float)M_PI * f / (float)sampleRate);
        }
    }
}

/** @brief eqCopy の窓から、各帯域のレベルを求める（計算のタスクで呼ぶ） */
static void eqCompute()
{
    const int16_t *data = eqCopy;
    const bool stereo = eqStereo;
    uint8_t out[EQ_BANDS];
    for (size_t i = 0; i < EQ_WINDOW; i++) {
        eqMono[i] = stereo ? 0.5f * ((float)data[2 * i] + (float)data[2 * i + 1]) : (float)data[i];
    }
    for (int b = 0; b < EQ_BANDS; b++) {
        const float c = eqCoeff[b];
        if (c == 0.0f) {
            out[b] = 0;
            continue;
        }
        float s1 = 0.0f, s2 = 0.0f;
        for (size_t i = 0; i < EQ_WINDOW; i++) {
            const float s = eqMono[i] + c * s1 - s2;
            s2 = s1;
            s1 = s;
        }
        float power = s1 * s1 + s2 * s2 - c * s1 * s2;
        if (power < 0.0f) power = 0.0f;
        // 振幅をフルスケール（32768）に対する比にそろえる。正弦波なら 1.0 で 0 dB
        const float amp = 2.0f * sqrtf(power) / ((float)EQ_WINDOW * 32768.0f);
        float level = 0.0f;
        if (amp > 1e-6f) {
            level = (20.0f * log10f(amp) - EQ_DB_FLOOR) / (EQ_DB_CEIL - EQ_DB_FLOOR) * 100.0f;
        }
        if (level < 0.0f) level = 0.0f;
        if (level > 100.0f) level = 100.0f;
        out[b] = (uint8_t)level;
    }
    // 再生が終わっていたら書かない（終了時にクリアした 0 を上書きしないため）
    if (!running) return;
    for (int b = 0; b < EQ_BANDS; b++) eqLevel[b] = out[b];
}

/** @brief 計算のタスク。通知を受けるたびに eqCopy を計算する。作りっぱなしで、begin() が 1 回だけ作る */
static void eqTaskFn(void *)
{
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        eqCompute();
        eqBusy = false;
    }
}

/**
 * @brief data（n 要素）の先頭の窓を計算のタスクへ渡す。再生用のタスクから呼ぶ
 *
 * 窓をコピーして通知するだけで、計算は待たない。前の計算が終わっていなければ、このチャンクは捨てる。
 */
static void eqSubmit(const int16_t *data, size_t n, bool stereo)
{
    const size_t frames = stereo ? n / 2 : n;
    if (frames < EQ_WINDOW || eqBusy || !eqTask) return;
    memcpy(eqCopy, data, EQ_WINDOW * (stereo ? 2 : 1) * sizeof(int16_t));
    eqStereo = stereo;
    eqBusy = true;
    xTaskNotifyGive(eqTask);
}

/** @brief 終了通知をキューへ入れる。満杯なら捨てる */
static void notifyEnd(EndReason reason)
{
    if (!eventQ || xQueueSend(eventQ, &reason, 0) != pdTRUE) {
        ESP_LOGE(TAG, "end event dropped");
    }
}

/**
 * @brief 再生用のタスク。1 回の再生ごとに start() が作り、終わると自分で削除する
 *
 * SD の読み出しは遅く、ui のタスクで行うとキー入力や描画が止まるので、専用のタスクに分けている。
 * 終了の通知は、どの経路で終わっても最後に必ず 1 件だけ送る。
 */
static void playTask(void *)
{
    EndReason reason = EndReason::Finished;
    FILE *f = fopen(playPath.c_str(), "rb");
    if (!f) {
        ESP_LOGE(TAG, "cannot open: %s", playPath.c_str());
        reason = EndReason::Failed;
    } else if (fseek(f, (long)(playInfo.dataOffset + startOffset), SEEK_SET) != 0) {
        ESP_LOGE(TAG, "seek failed");
        reason = EndReason::Failed;
    } else {
        bool stereo = playInfo.channels == 2;
        size_t remain = (playInfo.dataBytes - startOffset) / sizeof(int16_t);  // 残りの要素数
        int i = 0;
        while (remain > 0 && !stopReq) {
            size_t want = remain < BUF_ELEMS ? remain : BUF_ELEMS;
            if (stereo) want &= ~(size_t)1;  // ステレオは L/R が対なので、チャンクの切れ目で片チャンネルだけにならないよう偶数にそろえる
            if (want == 0) break;
            size_t n = fread(buf[i], sizeof(int16_t), want, f);
            if (stereo) n &= ~(size_t)1;
            if (n == 0) {  // データが残っているのに読めない
                ESP_LOGE(TAG, "read failed");
                reason = EndReason::Failed;
                break;
            }
            // キューに空きがないときは、空きが出るまでここで待つ
            M5.Speaker.playRaw(buf[i], n, playInfo.sampleRate, stereo, 1, CHANNEL);
            // 映像が音より先に動かないよう、playRaw が戻った（キューに空きができた）後に渡す
            eqSubmit(buf[i], n, stereo);
            i = (i + 1) % BUF_COUNT;
            remain -= n;
            playedBytes = playedBytes + (uint32_t)(n * sizeof(int16_t));
        }
        // 最後まで読んだら、鳴り終わるまで待つ
        while (M5.Speaker.isPlaying(CHANNEL) && !stopReq) vTaskDelay(POLL_TICKS);
        if (stopReq) M5.Speaker.stop(CHANNEL);
    }
    if (f) fclose(f);
    if (stopReq) reason = EndReason::Stopped;

    // running を下ろす前に送る。stop() から戻った時点で、通知がキューに入っていることを保証する
    notifyEnd(reason);
    running = false;
    eqClear();  // 止まる・終わる・一時停止のどれでも、棒は 0 に戻す。running を下ろした後なので計算側は上書きしない
    vTaskDelete(NULL);
}

bool begin()
{
    if (!eqTask &&
        xTaskCreate(eqTaskFn, "player_eq", EQ_TASK_STACK, NULL, EQ_TASK_PRIORITY, &eqTask) != pdPASS) {
        ESP_LOGE(TAG, "eq task create failed");
        eqTask = nullptr;  // イコライザーなしで再生は続ける
    }
    if (!eventQ) {
        eventQ = xQueueCreate(EVENT_QUEUE_LEN, sizeof(EndReason));
        if (!eventQ) {
            ESP_LOGE(TAG, "queue create failed");
            return false;
        }
    }
    return true;
}

QueueHandle_t eventQueue()
{
    return eventQ;
}

bool receive(EndReason &out, TickType_t timeout)
{
    if (!eventQ) return false;
    return xQueueReceive(eventQ, &out, timeout) == pdTRUE;
}

/** @brief offset（data 内の位置 [byte]）から読むタスクを作る。playPath / playInfo は設定済みであること */
static bool launch(uint32_t offset)
{
    if (running || !eventQ) return false;
    // タスクを作る前に値を設定する。タスクは引数ではなくこれらの static 変数を読む
    // （running を先に立てるのは、タスクが即座に動いても二重に start() されないようにするため）
    startOffset = offset;
    paused = false;
    stopReq = false;
    playedBytes = offset;
    eqPrepare(playInfo.sampleRate);
    eqClear();
    running = true;
    if (xTaskCreate(playTask, "player", TASK_STACK, NULL, TASK_PRIORITY, NULL) != pdPASS) {
        ESP_LOGE(TAG, "task create failed");
        running = false;
        return false;
    }
    return true;
}

bool start(const char *path, const wav::Info &info)
{
    if (running || !eventQ) return false;
    playPath = path;
    playInfo = info;
    return launch(0);
}

bool pause()
{
    if (!running) return false;
    stop();
    if (running) return false;  // タスクが終わらなかった
    // 位置は、ステレオなら L/R の対、モノラルでも 1 サンプルの境目にそろえる
    const uint32_t frame = playInfo.channels * sizeof(int16_t);
    uint32_t off = playedBytes;
    if (frame) off -= off % frame;
    if (off > playInfo.dataBytes) off = playInfo.dataBytes;
    startOffset = off;
    playedBytes = off;
    paused = true;
    return true;
}

bool resume()
{
    if (running || !paused) return false;
    return launch(startOffset);
}

uint32_t durationMs(const wav::Info &info)
{
    const uint64_t bytesPerSec = (uint64_t)info.sampleRate * info.channels * sizeof(int16_t);
    if (bytesPerSec == 0) return 0;
    return (uint32_t)((uint64_t)info.dataBytes * 1000 / bytesPerSec);
}

uint32_t positionMs()
{
    const uint64_t bytesPerSec = (uint64_t)playInfo.sampleRate * playInfo.channels * sizeof(int16_t);
    if (bytesPerSec == 0) return 0;
    uint64_t bytes = playedBytes;
    if (bytes > playInfo.dataBytes) bytes = playInfo.dataBytes;
    return (uint32_t)(bytes * 1000 / bytesPerSec);
}

void eqLevels(uint8_t out[EQ_BANDS])
{
    for (int b = 0; b < EQ_BANDS; b++) out[b] = eqLevel[b];
}

void stop()
{
    if (!running) return;
    stopReq = true;
    // playRaw は内部のキューに空きがないと待つ。タスクがそこで止まっていると stopReq を見られないので、
    // 再生中のデータを捨ててキューを空け、タスクを起こす
    M5.Speaker.stop(CHANNEL);
    for (int waited = 0; running && waited < STOP_WAIT_MS; waited += POLL_MS) {
        vTaskDelay(POLL_TICKS);
    }
    if (running) ESP_LOGE(TAG, "player task did not finish");
}

}  // namespace player
