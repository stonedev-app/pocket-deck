#include "audio/player.hpp"

#include <stdio.h>
#include <string.h>

#include <string>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"  // IWYU pragma: keep
#include "freertos/queue.h"
#include "freertos/task.h"
#include <M5Unified.hpp>

#include "audio/spectrum.hpp"
#include "util/ticks.hpp"

namespace player {

static const char *TAG = "player";

// 終了通知のキューの長さ。1 回の再生で 1 件しか出ず、受け取る側が毎回取り出すので 1 件でも足りるが、
// 取り出しが遅れても溜められるように 4 にした。満杯のときは新しい通知を捨ててログに出す
// （再生中に次の再生は始められないので、実際には満杯にならない）
static constexpr int EVENT_QUEUE_LEN = 4;

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
// 音の経路（スピーカー・再生用・イコライザーの計算のタスク）はコア 0 に置く。描画（LVGL）はコア 1。
// SD の読み出しは CPU を使い続けるので、描画と同じコアだと描画が間に合わない（実機で計測、#57）
static constexpr BaseType_t TASK_CORE = 0;
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
            spectrum::submit(buf[i], n, stereo);
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
    spectrum::stop();  // 止まる・終わる・一時停止のどれでも、棒は 0 に戻す
    vTaskDelete(NULL);
}

bool begin()
{
    if (!eventQ) {
        eventQ = xQueueCreate(EVENT_QUEUE_LEN, sizeof(EndReason));
        if (!eventQ) {
            ESP_LOGE(TAG, "queue create failed");
            return false;
        }
    }
    return true;
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
    spectrum::start(playInfo.sampleRate);
    running = true;
    if (xTaskCreatePinnedToCore(playTask, "player", TASK_STACK, NULL, TASK_PRIORITY, NULL, TASK_CORE) != pdPASS) {
        ESP_LOGE(TAG, "task create failed");
        running = false;
        spectrum::stop();
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
