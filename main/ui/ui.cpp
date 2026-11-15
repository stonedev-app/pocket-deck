#include "ui/ui.hpp"

#include <M5Unified.hpp>

#include "app/playback.hpp"
#include "ui/file_list.hpp"
#include "ui/now_playing.hpp"
#include "ui/status_bar.hpp"

namespace ui {

void showError(const char *msg)
{
    M5.Display.printf("\nInit failed:\n%s\n", msg);
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

void build()
{
    status_bar::create();
    file_list::create(onSelect);
    now_playing::create(onClose);
}

}  // namespace ui
