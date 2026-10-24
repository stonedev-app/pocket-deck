#include <string.h>

#include "freertos/FreeRTOS.h"  // IWYU pragma: keep
#include "freertos/task.h"
#include "esp_log.h"
#include <M5Unified.hpp>

#include "keyboard.hpp"

static const char *TAG = "pocket-deck";

static void showKey(const keyboard::KeyEvent &ev)
{
    char mods[24] = "";
    if (ev.mods & keyboard::MOD_FN)    strcat(mods, "Fn ");
    if (ev.mods & keyboard::MOD_SHIFT) strcat(mods, "Shift ");
    if (ev.mods & keyboard::MOD_CTRL)  strcat(mods, "Ctrl ");
    if (ev.mods & keyboard::MOD_OPT)   strcat(mods, "Opt ");
    if (ev.mods & keyboard::MOD_ALT)   strcat(mods, "Alt ");

    ESP_LOGI(TAG, "key: %s (mods: %s)", ev.name, mods[0] ? mods : "-");

    M5.Display.fillRect(0, 32, M5.Display.width(), 64, TFT_BLACK);
    M5.Display.setCursor(0, 32);
    M5.Display.setTextSize(3);
    M5.Display.println(ev.name);
    M5.Display.setTextSize(2);
    M5.Display.printf("mods: %s\n", mods[0] ? mods : "-");
}

extern "C" void app_main(void)
{
    M5.begin();
    ESP_LOGI(TAG, "display: %d x %d", (int)M5.Display.width(), (int)M5.Display.height());

    M5.Display.setTextSize(2);
    M5.Display.println("pocket-deck");

    if (!keyboard::begin()) {
        M5.Display.println("keyboard init failed");
    }

    while (1) {
        M5.update();
        keyboard::update();

        keyboard::KeyEvent ev;
        while (keyboard::poll(ev)) {
            showKey(ev);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
