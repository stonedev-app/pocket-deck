#include "esp_log.h"
#include <M5Unified.hpp>

#include "audio/player.hpp"
#include "audio/volume.hpp"
#include "device/keyboard.hpp"
#include "device/sdcard.hpp"
#include "ui/ui.hpp"

static const char *TAG = "pocket-deck";

/** @brief 初期化の失敗を、ログと画面に出して終わる（タスクは起動しない） */
static void fail(const char *msg)
{
    ESP_LOGE(TAG, "init failed: %s", msg);
    ui::showError(msg);
}

extern "C" void app_main(void)
{
    M5.begin();
    ESP_LOGI(TAG, "display: %d x %d", (int)M5.Display.width(), (int)M5.Display.height());

    // スピーカー（ES8311 コーデック。I2S は M5Unified が設定する）の初期化は、起動時に 1 回だけ行う。
    // volume::begin() は M5.Speaker.begin() の後でないと音量が反映されない
    M5.Speaker.begin();
    volume::begin();

    // SPI バスの初期化は M5.begin() の後でなければならない（ディスプレイ用 SPI との衝突を避ける）
    M5.Display.setTextSize(2);
    // 失敗しても app_main を return するだけ。この時点では画面のタスクがなく、
    // M5.Display に直接出して終えるのが安全（fail() 参照）
    if (!sdcard::begin()) return fail("sdcard");
    if (!keyboard::begin()) return fail("keyboard");
    if (!player::begin()) return fail("player");

    // 初期化がすべて成功してから、一覧を描いて画面のタスクを起動する
    if (!ui::start()) return fail("ui");
}
