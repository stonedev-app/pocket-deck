#include "esp_log.h"
#include <M5Unified.hpp>

#include "audio/player.hpp"
#include "audio/spectrum.hpp"
#include "audio/volume.hpp"
#include "device/keyboard.hpp"
#include "device/sdcard.hpp"
#include "ui/file_list.hpp"
#include "ui/lvgl_port.hpp"
#include "ui/now_playing.hpp"
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
 * @brief Fn + A / S / D の音量操作を処理する（帯は volume:: の状態を見て描き直す）
 *
 * @return 音量のキーで、処理したら true
 */
static bool handleVolumeKey(const keyboard::KeyEvent &ev)
{
    // Fn + A / S / D は、Key::Char の大文字 'A' / 'S' / 'D' に MOD_FN が付いたイベントになる。
    // Shift + A などは MOD_FN が付かないので区別できる
    if (ev.key != keyboard::Key::Char || !(ev.mods & keyboard::MOD_FN)) return false;
    switch (ev.ch) {
    case 'A': volume::up();         return true;
    case 'S': volume::down();       return true;
    case 'D': volume::toggleMute(); return true;
    default:                        return false;
    }
}

/**
 * @brief keyboard のイベントを lvgl_port の操作に変換する。変換できるキーでなければ false
 *
 * 選択の移動は、上下キーと vim 風の j / k。
 */
static bool toLvglKey(const keyboard::KeyEvent &ev, lvgl_port::Key &out)
{
    using keyboard::Key;
    switch (ev.key) {
    case Key::Down:  out = lvgl_port::Key::Next;  return true;
    case Key::Up:    out = lvgl_port::Key::Prev;  return true;
    case Key::Enter: out = lvgl_port::Key::Enter; return true;
    case Key::Esc:   out = lvgl_port::Key::Esc;   return true;
    case Key::Space: out = lvgl_port::Key::PlayPause; return true;
    case Key::Right: out = lvgl_port::Key::NextTrack; return true;
    case Key::Left:  out = lvgl_port::Key::PrevTrack; return true;
    case Key::Char:
        if (ev.ch == 'j') { out = lvgl_port::Key::Next; return true; }
        if (ev.ch == 'k') { out = lvgl_port::Key::Prev; return true; }
        if (ev.ch == 'l') { out = lvgl_port::Key::NextTrack; return true; }
        if (ev.ch == 'h') { out = lvgl_port::Key::PrevTrack; return true; }
        if (ev.ch == 'r') { out = lvgl_port::Key::Repeat; return true; }
        return false;
    default:
        return false;
    }
}

/**
 * @brief キーを 1 件取り出す（lvgl_port に渡す関数）。待たずに戻る
 *
 * 音量のキーは lvgl_port には渡さず、ここで処理する。残りは toLvglKey() で変換するだけ。
 */
static bool pollKey(lvgl_port::Key &out)
{
    keyboard::KeyEvent ev;
    if (!keyboard::poll(ev, 0)) return false;
    if (handleVolumeKey(ev)) return false;
    return toLvglKey(ev, out);
}

/** @brief 画面の部品を作る（帯と、その下の一覧、再生中の画面） */
static void buildUi()
{
    status_bar::create();
    file_list::create();
    now_playing::create();
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
    // スピーカーのタスクを、LVGL・再生用のタスク（優先度 5）より高い 6 にする。
    // 既定の 2 では、イコライザーの描画が忙しいときに CPU を取れず、音が途切れた（実機で確認）。
    // begin() は開始済みだと何もしないので、end() で止めてから設定し直す
    M5.Speaker.end();
    auto spk = M5.Speaker.config();
    spk.task_priority = 6;
    // 音の経路はコア 0 に置く（描画はコア 1。player.cpp の TASK_CORE を参照）
    spk.task_pinned_core = 0;
    M5.Speaker.config(spk);
    M5.Speaker.begin();
    volume::begin();

    // SPI バスの初期化は M5.begin() の後でなければならない（ディスプレイ用 SPI との衝突を避ける）
    M5.Display.setTextSize(2);
    // 失敗しても app_main を return するだけ。この時点では画面のタスクがなく、
    // M5.Display に直接出して終えるのが安全（fail() 参照）
    if (!sdcard::begin()) return fail("sdcard");
    if (!keyboard::begin()) return fail("keyboard");
    // イコライザーの計算のタスク。失敗しても棒が動かないだけなので、再生は続ける
    if (!spectrum::begin()) ESP_LOGW(TAG, "spectrum unavailable");
    if (!player::begin()) return fail("player");

    // 初期化がすべて成功してから、LVGL を起動する（画面を作って LVGL のタスクを動かす）
    if (!lvgl_port::start(M5.Display.width(), M5.Display.height(), displayFlush, pollKey, buildUi)) {
        return fail("lvgl");
    }
}
