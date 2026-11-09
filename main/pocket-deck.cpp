#include "esp_log.h"
#include <M5Unified.hpp>

#include "audio/player.hpp"
#include "audio/volume.hpp"
#include "device/keyboard.hpp"
#include "device/sdcard.hpp"
#include "ui/lvgl_port.hpp"
#include "ui/status_bar.hpp"
#include "ui/ui.hpp"

static const char *TAG = "pocket-deck";

/** @brief 初期化の失敗を、ログと画面に出して終わる（タスクは起動しない） */
static void fail(const char *msg)
{
    ESP_LOGE(TAG, "init failed: %s", msg);
    ui::showError(msg);
}

/**
 * @brief LVGL が描いた領域を M5.Display に転送する（lvgl_port に渡す関数）
 *
 * M5GFX への依存は、ここだけに置く。画素は RGB565 のまま渡す。
 */
static void displayFlush(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t *pixels)
{
    M5.Display.startWrite();
    M5.Display.pushImage(x, y, w, h, reinterpret_cast<const lgfx::rgb565_t *>(pixels));
    M5.Display.endWrite();
}

/**
 * @brief keyboard のイベントを lvgl_port の操作に変換して 1 件取り出す（lvgl_port に渡す関数）
 *
 * Fn + A / S / D の音量操作は、ここで処理する（帯は volume:: の状態を見て描き直す）。
 * 選択の移動は、上下キーと vim 風の j / k。待たずに戻る。
 */
static bool pollKey(lvgl_port::Key &out)
{
    using keyboard::Key;
    keyboard::KeyEvent ev;
    if (!keyboard::poll(ev, 0)) return false;

    // Fn + A / S / D は、Key::Char の大文字 'A' / 'S' / 'D' に MOD_FN が付いたイベントになる。
    // Shift + A などは MOD_FN が付かないので区別できる
    if (ev.key == Key::Char && (ev.mods & keyboard::MOD_FN) &&
        (ev.ch == 'A' || ev.ch == 'S' || ev.ch == 'D')) {
        if (ev.ch == 'A') volume::up();
        else if (ev.ch == 'S') volume::down();
        else volume::toggleMute();
        return false;
    }

    switch (ev.key) {
    case Key::Down:  out = lvgl_port::Key::Next;  return true;
    case Key::Up:    out = lvgl_port::Key::Prev;  return true;
    case Key::Enter: out = lvgl_port::Key::Enter; return true;
    case Key::Esc:   out = lvgl_port::Key::Esc;   return true;
    case Key::Char:
        if (ev.ch == 'j') { out = lvgl_port::Key::Next; return true; }
        if (ev.ch == 'k') { out = lvgl_port::Key::Prev; return true; }
        return false;
    default:
        return false;
    }
}

/**
 * @brief 起動時の入口。初期化とタスクの起動だけを行い、あとは画面のタスクに任せる
 *
 * 初期化の順序: M5.begin() → スピーカーと音量 → SD カード → キーボード → 再生 → 画面。
 * 後ろのものは前のものに依存する（SPI は M5.begin() の後、音量はスピーカーの後、
 * 画面は keyboard / player のキューができた後）。
 * どれかが失敗したら fail() で画面とログに出して return する（以降のタスクは起動しない）。
 */
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

    // 初期化がすべて成功してから、LVGL を起動する（帯を作って LVGL のタスクを動かす）。
    // ui::start() は残してあるが、どちらも M5.Display に描くので同時には動かせない
    if (!lvgl_port::start(M5.Display.width(), M5.Display.height(), displayFlush, pollKey,
                          status_bar::create)) {
        return fail("lvgl");
    }
}
