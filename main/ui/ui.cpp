#include "ui/ui.hpp"

#include <M5Unified.hpp>

#include "app/input.hpp"
#include "app/playback.hpp"
#include "device/display.hpp"
#include "ui/keys.hpp"
#include "ui/lvgl_port.hpp"
#include "ui/screens/file_list.hpp"
#include "ui/screens/now_playing.hpp"
#include "ui/widgets/status_bar.hpp"

namespace ui {

void showError(const char *msg)
{
    M5.Display.setTextSize(2);
    M5.Display.printf("\nInit failed:\n%s\n", msg);
}

/**
 * @brief 操作を LVGL のキーコード（LV_KEY_* や文字）に変換する
 *
 * 項目間の選択の移動は LV_KEY_NEXT / LV_KEY_PREV で行う。曲の操作は keys.hpp の値。
 */
static uint32_t toLvKey(input::Action action)
{
    using input::Action;
    switch (action) {
    case Action::Next:      return LV_KEY_NEXT;
    case Action::Prev:      return LV_KEY_PREV;
    case Action::Enter:     return LV_KEY_ENTER;
    case Action::Esc:       return LV_KEY_ESC;
    case Action::PlayPause: return keys::PLAY_PAUSE;
    case Action::NextTrack: return keys::NEXT_TRACK;
    case Action::PrevTrack: return keys::PREV_TRACK;
    case Action::Repeat:    return keys::REPEAT;
    }
    return 0;
}

/** @brief キーを 1 件取り出す（lvgl_port に渡す関数）。待たずに戻る */
static bool pollKey(uint32_t &key)
{
    input::Action action;
    if (!input::poll(action)) return false;
    key = toLvKey(action);
    return true;
}

/** @brief 一覧で曲が選ばれた。再生を始められたら、再生中の画面に切り替える */
static void onSelect(int index)
{
    if (!playback::play(index)) return;
    file_list::hide();
    now_playing::open();
}

/** @brief 再生中の画面が閉じた。一覧に戻り、再生していた曲の行を選ぶ */
static void onClose()
{
    file_list::show(playback::index());
}

static void build()
{
    status_bar::create();
    file_list::create(onSelect);
    now_playing::create(onClose);
}

bool start()
{
    return lvgl_port::start(display::width(), display::height(), display::flush, pollKey, build);
}

}  // namespace ui
