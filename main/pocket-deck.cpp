#include <string.h>

#include "esp_log.h"
#include <M5Unified.hpp>

#include "keyboard.hpp"
#include "sdcard.hpp"

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

    // SPI バスの初期化は M5.begin() の後でなければならない（ディスプレイ用 SPI との衝突を避ける）
    sdcard::showWavList();

    if (!keyboard::begin()) {
        ESP_LOGE(TAG, "keyboard init failed");
        return;
    }

    keyboard::KeyEvent ev;
    while (1) {
        if (keyboard::poll(ev, portMAX_DELAY)) {
            showKey(ev);
        }
    }
}
