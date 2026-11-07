#include "screen.hpp"

#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "keyboard.hpp"
#include "player.hpp"
#include "ui.hpp"

static const char *TAG = "screen";

static const uint32_t TASK_STACK = 8192;
static const UBaseType_t TASK_PRIORITY = 5;

static void showKey(const keyboard::KeyEvent &ev)
{
    char mods[24] = "";
    if (ev.mods & keyboard::MOD_FN)    strcat(mods, "Fn ");
    if (ev.mods & keyboard::MOD_SHIFT) strcat(mods, "Shift ");
    if (ev.mods & keyboard::MOD_CTRL)  strcat(mods, "Ctrl ");
    if (ev.mods & keyboard::MOD_OPT)   strcat(mods, "Opt ");
    if (ev.mods & keyboard::MOD_ALT)   strcat(mods, "Alt ");

    // 画面は WAV 一覧に使うので、キーはログにだけ出す
    ESP_LOGI(TAG, "key: %s (mods: %s)", ev.name, mods[0] ? mods : "-");
}

static void screenTask(void *arg)
{
    QueueSetHandle_t set = static_cast<QueueSetHandle_t>(arg);
    while (1) {
        QueueSetMemberHandle_t member = xQueueSelectFromSet(set, portMAX_DELAY);
        // 届いたキューから 1 件だけ取る。0 tick で取るので、ui が先に取った分で
        // 空だったときは何もしない
        if (member == keyboard::queue()) {
            keyboard::KeyEvent ev;
            if (keyboard::poll(ev, 0)) {
                showKey(ev);
                ui::handleKey(ev);
            }
        } else if (member == player::eventQueue()) {
            player::EndReason reason;
            if (player::receive(reason, 0)) ui::handleEnd(reason);
        }
    }
}

bool screen::start()
{
    // セットの大きさは、入れるキューの長さの合計にする
    QueueSetHandle_t set = xQueueCreateSet(keyboard::QUEUE_SIZE + player::EVENT_QUEUE_LEN);
    if (!set || xQueueAddToSet(keyboard::queue(), set) != pdPASS ||
        xQueueAddToSet(player::eventQueue(), set) != pdPASS) {
        ESP_LOGE(TAG, "queue set init failed");
        return false;
    }
    if (xTaskCreate(screenTask, "screen", TASK_STACK, set, TASK_PRIORITY, nullptr) != pdPASS) {
        ESP_LOGE(TAG, "task create failed");
        return false;
    }
    return true;
}
