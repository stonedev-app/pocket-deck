#include "audio/player.hpp"

#include <stdio.h>

#include <string>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"  // IWYU pragma: keep
#include "freertos/queue.h"
#include "freertos/task.h"
#include <M5Unified.hpp>

namespace player {

static const char *TAG = "player";

static constexpr int BUF_COUNT = 3;        // バッファの個数。順番に使う
static constexpr size_t BUF_ELEMS = 2048;  // 1 個あたりの要素数（int16_t）
static constexpr uint8_t CHANNEL = 0;      // playRaw に明示するチャンネル
static constexpr uint32_t TASK_STACK = 4096;
static constexpr UBaseType_t TASK_PRIORITY = 5;
static constexpr int STOP_WAIT_MS = 2000;  // stop() がタスクの終了を待つ上限
// 待ちループの刻み。CONFIG_FREERTOS_HZ=100（1 tick = 10ms）では 5ms が 0 tick になり、
// vTaskDelay(0) では IDLE が走れずウォッチドッグが発火するため、必ず 1 tick 以上にする
static constexpr int POLL_MS = 20;
static constexpr TickType_t POLL_TICKS = pdMS_TO_TICKS(POLL_MS) > 0 ? pdMS_TO_TICKS(POLL_MS) : 1;

// 内部 SRAM に静的に取る（PSRAM はない）。再生が終わるまで書き換えない・解放しない
static int16_t buf[BUF_COUNT][BUF_ELEMS];

static std::string playPath;
static wav::Info playInfo;
static volatile bool running = false;  // 再生用のタスクが動いている間 true
static volatile bool stopReq = false;  // stop() からタスクへの停止の要求
static QueueHandle_t eventQ = nullptr;  // 終了通知のキュー

/** @brief 終了通知をキューへ入れる。満杯なら捨てる */
static void notifyEnd(EndReason reason)
{
    if (!eventQ || xQueueSend(eventQ, &reason, 0) != pdTRUE) {
        ESP_LOGE(TAG, "end event dropped");
    }
}

static void playTask(void *)
{
    EndReason reason = EndReason::Finished;
    FILE *f = fopen(playPath.c_str(), "rb");
    if (!f) {
        ESP_LOGE(TAG, "cannot open: %s", playPath.c_str());
        reason = EndReason::Failed;
    } else if (fseek(f, (long)playInfo.dataOffset, SEEK_SET) != 0) {
        ESP_LOGE(TAG, "seek failed");
        reason = EndReason::Failed;
    } else {
        bool stereo = playInfo.channels == 2;
        size_t remain = playInfo.dataBytes / sizeof(int16_t);  // 残りの要素数
        int i = 0;
        while (remain > 0 && !stopReq) {
            size_t want = remain < BUF_ELEMS ? remain : BUF_ELEMS;
            if (stereo) want &= ~(size_t)1;  // ステレオは偶数にそろえる
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
            i = (i + 1) % BUF_COUNT;
            remain -= n;
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

QueueHandle_t eventQueue()
{
    return eventQ;
}

bool receive(EndReason &out, TickType_t timeout)
{
    if (!eventQ) return false;
    return xQueueReceive(eventQ, &out, timeout) == pdTRUE;
}

bool start(const char *path, const wav::Info &info)
{
    if (running || !eventQ) return false;
    playPath = path;
    playInfo = info;
    stopReq = false;
    running = true;
    if (xTaskCreate(playTask, "player", TASK_STACK, NULL, TASK_PRIORITY, NULL) != pdPASS) {
        ESP_LOGE(TAG, "task create failed");
        running = false;
        return false;
    }
    return true;
}

void stop()
{
    if (!running) return;
    stopReq = true;
    // playRaw で待っているタスクを起こすために、キューを空にする
    M5.Speaker.stop(CHANNEL);
    for (int waited = 0; running && waited < STOP_WAIT_MS; waited += POLL_MS) {
        vTaskDelay(POLL_TICKS);
    }
    if (running) ESP_LOGE(TAG, "player task did not finish");
}

}  // namespace player
