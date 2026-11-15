#include "app/input.hpp"

#include "audio/volume.hpp"
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

bool poll(Action &out)
{
    keyboard::KeyEvent ev;
    if (!keyboard::poll(ev, 0)) return false;
    if (handleVolumeKey(ev)) return false;
    return toAction(ev, out);
}

}  // namespace input
