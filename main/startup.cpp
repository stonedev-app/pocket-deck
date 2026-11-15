#include "startup.hpp"

#include "esp_log.h"
#include <M5Unified.hpp>

#include "app/playlist.hpp"
#include "audio/player.hpp"
#include "audio/speaker.hpp"
#include "audio/spectrum.hpp"
#include "device/display.hpp"
#include "device/keyboard.hpp"
#include "device/sdcard.hpp"
#include "ui/ui.hpp"

namespace startup {

static const char *TAG = "pocket-deck";

/**
 * @brief 初期化の失敗を、ログと画面に出して終わる（タスクは起動しない）
 *
 * この時点では画面のタスクがなく、M5.Display に直接出して終えるのが安全。
 */
static void fail(const char *msg)
{
    ESP_LOGE(TAG, "init failed: %s", msg);
    ui::showError(msg);
}

/**
 * 初期化の順序: M5.begin() → スピーカーと音量 → SD カード → キーボード → イコライザーの計算 →
 * 再生 → 曲の一覧 → 画面。
 * 後ろのものは前のものに依存する（SPI は M5.begin() の後、音量はスピーカーの後、
 * 曲の一覧は SD カードの後、画面は keyboard / player のキューと曲の一覧ができた後）。
 * どれかが失敗したら fail() で画面とログに出して return する（以降のタスクは起動しない）。
 */
void run()
{
    M5.begin();
    ESP_LOGI(TAG, "display: %d x %d", (int)display::width(), (int)display::height());

    // スピーカーの初期化は、起動時に 1 回だけ行う（音量の復元も含む）
    speaker::begin();

    // SPI バスの初期化は M5.begin() の後でなければならない（ディスプレイ用 SPI との衝突を避ける）
    char err[64];
    if (!sdcard::begin(err, sizeof(err))) return fail(err);
    if (!keyboard::begin()) return fail("keyboard");
    // イコライザーの計算のタスク。失敗しても棒が動かないだけなので、再生は続ける
    if (!spectrum::begin()) ESP_LOGW(TAG, "spectrum unavailable");
    if (!player::begin()) return fail("player");

    // 曲の一覧を取る。失敗しても 0 件になるだけ（一覧に「(none)」が出る）
    playlist::load();

    // 初期化がすべて成功してから、画面を起動する（画面を作って LVGL のタスクを動かす）
    if (!ui::start()) return fail("lvgl");
}

}  // namespace startup
