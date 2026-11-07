#include "esp_log.h"
#include <M5Unified.hpp>

#include "keyboard.hpp"
#include "player.hpp"
#include "screen.hpp"
#include "sdcard.hpp"
#include "ui.hpp"
#include "volume.hpp"

static const char *TAG = "pocket-deck";

// 初期化の失敗は、ログと画面に出して終わる（タスクは起動しない）
static void fail(const char *msg)
{
    ESP_LOGE(TAG, "init failed: %s", msg);
    M5.Display.printf("\nInit failed:\n%s\n", msg);
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
    if (!sdcard::begin()) return fail("sdcard");
    if (!sdcard::listWav(ui::state.files)) return fail("opendir");
    if (!keyboard::begin()) return fail("keyboard");
    if (!player::begin()) return fail("player");

    // 初期化がすべて成功してから描画を始め、画面のタスクを起動する
    ui::drawList();
    if (!screen::start()) return fail("screen task");
}
