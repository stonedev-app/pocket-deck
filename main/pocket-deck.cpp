#include <string.h>

#include <string>
#include <vector>

#include "esp_log.h"
#include <M5Unified.hpp>

#include "keyboard.hpp"
#include "player.hpp"
#include "sdcard.hpp"
#include "ui.hpp"
#include "volume.hpp"
#include "wav.hpp"

static const char *TAG = "pocket-deck";

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

extern "C" void app_main(void)
{
    M5.begin();
    ESP_LOGI(TAG, "display: %d x %d", (int)M5.Display.width(), (int)M5.Display.height());

    // スピーカーの初期化と音量の設定は、起動時に 1 回だけ行う
    M5.Speaker.begin();
    volume::begin();

    // SPI バスの初期化は M5.begin() の後でなければならない（ディスプレイ用 SPI との衝突を避ける）
    M5.Display.setTextSize(2);
    if (!sdcard::begin()) return;
    if (!sdcard::listWav(ui::state.files)) {
        M5.Display.println("opendir failed");
        return;
    }
    ui::drawList();

    if (!keyboard::begin() || !player::begin()) {
        ESP_LOGE(TAG, "keyboard or player init failed");
        return;
    }

    // キー入力と再生終了の 2 つのキューを QueueSet にまとめ、どちらかに届くまで 1 か所で待つ。
    // セットの大きさは、入れるキューの長さの合計にする
    QueueSetHandle_t set = xQueueCreateSet(keyboard::QUEUE_SIZE + player::EVENT_QUEUE_LEN);
    if (!set || xQueueAddToSet(keyboard::queue(), set) != pdPASS ||
        xQueueAddToSet(player::eventQueue(), set) != pdPASS) {
        ESP_LOGE(TAG, "queue set init failed");
        return;
    }

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
