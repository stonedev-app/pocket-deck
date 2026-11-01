#include <string.h>

#include <string>
#include <vector>

#include "esp_log.h"
#include <M5Unified.hpp>

#include "keyboard.hpp"
#include "player.hpp"
#include "sdcard.hpp"
#include "ui.hpp"
#include "volume.hpp"
#include "wav.hpp"

static const char *TAG = "pocket-deck";

static bool showingResult = false;

// 待ちのタイムアウト。どちらのキューにも届かない間も、volume::tick() の保存判定のために起きる
static constexpr TickType_t WAIT_TICKS = pdMS_TO_TICKS(100);

static void showKey(const keyboard::KeyEvent &ev)
{
    char mods[24] = "";
    if (ev.mods & keyboard::MOD_FN)    strcat(mods, "Fn ");
    if (ev.mods & keyboard::MOD_SHIFT) strcat(mods, "Shift ");
    if (ev.mods & keyboard::MOD_CTRL)  strcat(mods, "Ctrl ");
    if (ev.mods & keyboard::MOD_OPT)   strcat(mods, "Opt ");
    if (ev.mods & keyboard::MOD_ALT)   strcat(mods, "Alt ");

    // 画面は WAV 一覧に使うので、キーはログにだけ出す
    ESP_LOGI(TAG, "key: %s (mods: %s)", ev.name, mods[0] ? mods : "-");
}

// 選んだ WAV のヘッダを解析して、結果かエラーの理由をログに出す。画面は ui::drawResult() で描く
static void parseSelected()
{
    ui::State &st = ui::state;
    const std::string &name = st.files[st.selected];
    st.resultOk = wav::parse(sdcard::wavPath(name).c_str(), st.resultInfo, st.resultErr,
                             sizeof(st.resultErr));
    if (!st.resultOk) {
        ESP_LOGE(TAG, "wav parse failed: %s: %s", name.c_str(), st.resultErr);
        return;
    }
    ESP_LOGI(TAG, "wav: %s rate=%u ch=%u bits=%u data_offset=%u data_bytes=%u", name.c_str(),
             (unsigned)st.resultInfo.sampleRate, (unsigned)st.resultInfo.channels,
             (unsigned)st.resultInfo.bitsPerSample, (unsigned)st.resultInfo.dataOffset,
             (unsigned)st.resultInfo.dataBytes);
}

// 再生を止めて、画面に出す状態を停止中にする（描き直しは呼び出し側）
static void stopPlayback()
{
    player::stop();
    // stop() で出た Stopped の通知はここで捨てる。残すと、すぐ再生し直したときに
    // 古い通知で新しい再生が止まったことになってしまう
    player::EndReason reason;
    while (player::receive(reason, 0)) {}
    ui::state.resultPlaying = false;
}

// 再生が終わった通知を受けたら、停止中の表示に戻す（描画はこのタスクだけで行う）
static void handleEnd(player::EndReason reason)
{
    ESP_LOGI(TAG, "playback end: %d", (int)reason);
    if (showingResult && ui::state.resultPlaying) {
        ui::state.resultPlaying = false;
        ui::drawResult();
    }
}

// 一覧の操作（j / Down で下、k / Up で上、Enter で決定）と、結果表示の操作
// （Enter で再生 / 停止、Esc で一覧に戻る。再生中なら止めてから戻る）
static void handleKey(const keyboard::KeyEvent &ev)
{
    ui::State &st = ui::state;

    // Fn + A / S / D は、Key::Char の大文字 'A' / 'S' / 'D' に MOD_FN が付いたイベントになる。
    // Shift + A などは MOD_FN が付かないので区別できる
    if (ev.key == keyboard::Key::Char && (ev.mods & keyboard::MOD_FN) &&
        (ev.ch == 'A' || ev.ch == 'S' || ev.ch == 'D')) {
        if (ev.ch == 'A') volume::up();
        else if (ev.ch == 'S') volume::down();
        else volume::toggleMute();
        if (showingResult) ui::drawResult(); else ui::drawList();
        return;
    }
    if (showingResult) {
        if (ev.key == keyboard::Key::Esc) {
            stopPlayback();
            showingResult = false;
            ui::drawList();
        } else if (ev.key == keyboard::Key::Enter && st.resultOk) {
            if (st.resultPlaying) {
                stopPlayback();
            } else {
                st.resultPlaying =
                    player::start(sdcard::wavPath(st.files[st.selected]).c_str(), st.resultInfo);
            }
            ui::drawResult();
        }
        return;
    }
    if (st.files.empty()) return;

    bool down = ev.key == keyboard::Key::Down || (ev.key == keyboard::Key::Char && ev.ch == 'j');
    bool up   = ev.key == keyboard::Key::Up || (ev.key == keyboard::Key::Char && ev.ch == 'k');
    if (down && st.selected < (int)st.files.size() - 1) {
        st.selected++;
        ui::drawList();
    } else if (up && st.selected > 0) {
        st.selected--;
        ui::drawList();
    } else if (ev.key == keyboard::Key::Enter) {
        showingResult = true;
        st.resultPlaying = false;
        parseSelected();
        ui::drawResult();
    }
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
    if (!sdcard::begin()) return;
    if (!sdcard::listWav(ui::state.files)) {
        M5.Display.println("opendir failed");
        return;
    }
    ui::drawList();

    if (!keyboard::begin() || !player::begin()) {
        ESP_LOGE(TAG, "keyboard or player init failed");
        return;
    }

    // キー入力と再生終了の 2 つのキューを QueueSet にまとめ、どちらかに届くまで 1 か所で待つ。
    // セットの大きさは、入れるキューの長さの合計にする
    QueueSetHandle_t set = xQueueCreateSet(keyboard::QUEUE_SIZE + player::EVENT_QUEUE_LEN);
    if (!set || xQueueAddToSet(keyboard::queue(), set) != pdPASS ||
        xQueueAddToSet(player::eventQueue(), set) != pdPASS) {
        ESP_LOGE(TAG, "queue set init failed");
        return;
    }

    while (1) {
        QueueSetMemberHandle_t member = xQueueSelectFromSet(set, WAIT_TICKS);
        // 届いたキューから 1 件だけ取る。0 tick で取るので、stopPlayback() が先に取った分で
        // 空だったときは何もしない
        if (member == keyboard::queue()) {
            keyboard::KeyEvent ev;
            if (keyboard::poll(ev, 0)) {
                showKey(ev);
                handleKey(ev);
            }
        } else if (member == player::eventQueue()) {
            player::EndReason reason;
            if (player::receive(reason, 0)) handleEnd(reason);
        }
        volume::tick();
    }
}
