#include "app/input.hpp"

#include <esp_timer.h>

#include "audio/volume.hpp"
#include "device/display.hpp"
#include "device/keyboard.hpp"

namespace input {

/**
 * @brief Fn + A / S / D の音量操作を処理する
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
 * @brief keyboard のイベントを操作に変換する。変換できるキーでなければ false
 *
 * 選択の移動は、上下キーと vim 風の j / k。
 */
static bool toAction(const keyboard::KeyEvent &ev, Action &out)
{
    using keyboard::Key;
    switch (ev.key) {
    case Key::Down:  out = Action::Next;  return true;
    case Key::Up:    out = Action::Prev;  return true;
    case Key::Enter: out = Action::Enter; return true;
    case Key::Esc:   out = Action::Esc;   return true;
    case Key::Space: out = Action::PlayPause; return true;
    case Key::Right: out = Action::NextTrack; return true;
    case Key::Left:  out = Action::PrevTrack; return true;
    case Key::Char:
        if (ev.ch == 'j') { out = Action::Next; return true; }
        if (ev.ch == 'k') { out = Action::Prev; return true; }
        if (ev.ch == 'l') { out = Action::NextTrack; return true; }
        if (ev.ch == 'h') { out = Action::PrevTrack; return true; }
        if (ev.ch == 'r') { out = Action::Repeat; return true; }
        return false;
    default:
        return false;
    }
}

// 操作がなくなってから、暗くするまで・消すまでの時間（どちらも直前の段階からの経過）
static constexpr int64_t kDimAfterUs = 30LL * 1000 * 1000;
static constexpr int64_t kOffAfterUs = 60LL * 1000 * 1000;

/**
 * @brief 操作がない時間に応じて、バックライトを暗くする・消す
 *
 * @param[in] pressed キーが押されたか
 * @return 画面が暗い・消えていて、このキーを復帰だけに使うなら true
 */
static bool updateBacklight(bool pressed)
{
    static display::Backlight level = display::Backlight::Full;
    static int64_t lastActive = esp_timer_get_time();

    int64_t now = esp_timer_get_time();
    if (pressed) {
        lastActive = now;
        if (level != display::Backlight::Full) {
            level = display::Backlight::Full;
            display::setBacklight(level);
            return true;
        }
        return false;
    }

    int64_t idle = now - lastActive;
    display::Backlight want = idle >= kOffAfterUs ? display::Backlight::Off
                            : idle >= kDimAfterUs ? display::Backlight::Dim
                                                  : display::Backlight::Full;
    if (want != level) {
        level = want;
        display::setBacklight(level);
    }
    return false;
}

bool poll(Action &out)
{
    keyboard::KeyEvent ev;
    bool pressed = keyboard::poll(ev, 0);
    // 暗い・消えているときのキーは、画面を戻すだけに使う（音量も変えない）
    if (updateBacklight(pressed)) return false;
    if (!pressed) return false;
    if (handleVolumeKey(ev)) return false;
    return toAction(ev, out);
}

}  // namespace input
