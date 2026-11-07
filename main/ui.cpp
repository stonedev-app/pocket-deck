#include "ui.hpp"

#include <string.h>

#include <cstdint>
#include <string>
#include <vector>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"  // IWYU pragma: keep
#include "freertos/queue.h"
#include "freertos/task.h"
#include <M5Unified.hpp>

#include "keyboard.hpp"
#include "player.hpp"
#include "sdcard.hpp"
#include "volume.hpp"
#include "wav.hpp"

namespace ui {

static const char *TAG = "ui";

static const uint32_t TASK_STACK = 8192;
static const UBaseType_t TASK_PRIORITY = 5;

// 画面モード
enum class Mode : uint8_t {
    List,    // WAV 一覧
    Result,  // 結果表示（再生 / 停止）
};

// 画面の状態。画面のタスクだけが更新する（起動前の start() の中を除く）
struct State {
    Mode mode = Mode::List;          // 今の画面モード
    std::vector<std::string> files;  // 一覧に出す WAV のファイル名
    int selected = 0;                // 一覧で選んでいる位置

    // 結果画面に出している WAV の解析結果
    wav::Info resultInfo;
    bool resultOk = false;
    char resultErr[40] = "";
    bool resultPlaying = false;  // 画面に出している状態。player の終了通知を受けて下ろす
};

static State state;

// 画面は 240x135。文字サイズ 2（12x16）で 1 行 20 文字、見出し 1 行を除いて 7 行入る。
// 長い名前は折り返して行数が増えるので、行頭の印（2 文字）を除いた長さで切る
static constexpr int NAME_CHARS = 18;

// 音量を「mute」または「level/max」で出して改行する。prefix は音量の前に出す文字列
static void printVolume(const char *prefix)
{
    if (volume::muted()) {
        M5.Display.printf("%smute\n", prefix);
    } else {
        M5.Display.printf("%s%d/%d\n", prefix, volume::level(), volume::MAX_LEVEL);
    }
}

static void drawList()
{
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setCursor(0, 0);
    M5.Display.setTextSize(2);

    printVolume("WAV files:  Vol ");
    if (state.files.empty()) {
        M5.Display.println("(none)");
        return;
    }
    for (int i = 0; i < (int)state.files.size(); i++) {
        M5.Display.printf("%c %.*s\n", i == state.selected ? '>' : ' ', NAME_CHARS,
                          state.files[i].c_str());
    }
}

// 結果画面。形式と、再生中か停止中かを出す（文字サイズ 2 で 8 行まで入る）
static void drawResult()
{
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setCursor(0, 0);
    M5.Display.setTextSize(2);
    M5.Display.printf("%.20s\n", state.files[state.selected].c_str());

    if (!state.resultOk) {
        M5.Display.printf("Error:\n%s\n", state.resultErr);
        return;
    }
    M5.Display.printf("Rate: %u\nCh: %u\nBits: %u\nData: %u\n", (unsigned)state.resultInfo.sampleRate,
                      (unsigned)state.resultInfo.channels, (unsigned)state.resultInfo.bitsPerSample,
                      (unsigned)state.resultInfo.dataBytes);
    M5.Display.printf("%s\n", state.resultPlaying ? "Playing" : "Stopped");
    printVolume("Vol: ");
}

// 選んだ WAV のヘッダを解析して、結果かエラーの理由を state に入れ、ログにも出す
static void parseSelected()
{
    State &st = state;
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

static void redraw()
{
    if (state.mode == Mode::Result) drawResult(); else drawList();
}

// 再生を始める。成功したら表示を再生中にする
static void startPlayback()
{
    state.resultPlaying =
        player::start(sdcard::wavPath(state.files[state.selected]).c_str(), state.resultInfo);
}

// 再生を止めて、表示を停止中にする。stop() で出た Stopped の通知はここで捨てる。
// 残すと、すぐ再生し直したときに古い通知で新しい再生が止まったことになってしまう
static void stopPlayback()
{
    player::stop();
    player::EndReason reason;
    while (player::receive(reason, 0)) {}
    state.resultPlaying = false;
}

static void handleEnd(player::EndReason reason)
{
    ESP_LOGI(TAG, "playback end: %d", (int)reason);
    if (state.mode == Mode::Result && state.resultPlaying) {
        state.resultPlaying = false;
        drawResult();
    }
}

static void handleKey(const keyboard::KeyEvent &ev)
{
    State &st = state;

    // Fn + A / S / D は、Key::Char の大文字 'A' / 'S' / 'D' に MOD_FN が付いたイベントになる。
    // Shift + A などは MOD_FN が付かないので区別できる
    if (ev.key == keyboard::Key::Char && (ev.mods & keyboard::MOD_FN) &&
        (ev.ch == 'A' || ev.ch == 'S' || ev.ch == 'D')) {
        if (ev.ch == 'A') volume::up();
        else if (ev.ch == 'S') volume::down();
        else volume::toggleMute();
        redraw();
        return;
    }
    if (st.mode == Mode::Result) {
        if (ev.key == keyboard::Key::Esc) {
            stopPlayback();
            st.mode = Mode::List;
            drawList();
        } else if (ev.key == keyboard::Key::Enter && st.resultOk) {
            if (st.resultPlaying) stopPlayback(); else startPlayback();
            drawResult();
        }
        return;
    }
    if (st.files.empty()) return;

    bool down = ev.key == keyboard::Key::Down || (ev.key == keyboard::Key::Char && ev.ch == 'j');
    bool up   = ev.key == keyboard::Key::Up || (ev.key == keyboard::Key::Char && ev.ch == 'k');
    if (down && st.selected < (int)st.files.size() - 1) {
        st.selected++;
        drawList();
    } else if (up && st.selected > 0) {
        st.selected--;
        drawList();
    } else if (ev.key == keyboard::Key::Enter) {
        st.mode = Mode::Result;
        st.resultPlaying = false;
        parseSelected();
        drawResult();
    }
}

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

static void screenTask(void *arg)
{
    QueueSetHandle_t set = static_cast<QueueSetHandle_t>(arg);
    while (1) {
        QueueSetMemberHandle_t member = xQueueSelectFromSet(set, portMAX_DELAY);
        // 届いたキューから 1 件だけ取る。0 tick で取るので、先に取られて
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
    }
}

bool start()
{
    if (!sdcard::listWav(state.files)) {
        ESP_LOGE(TAG, "listWav failed");
        return false;
    }
    drawList();

    // セットの大きさは、入れるキューの長さの合計にする
    QueueSetHandle_t set = xQueueCreateSet(keyboard::QUEUE_SIZE + player::EVENT_QUEUE_LEN);
    if (!set || xQueueAddToSet(keyboard::queue(), set) != pdPASS ||
        xQueueAddToSet(player::eventQueue(), set) != pdPASS) {
        ESP_LOGE(TAG, "queue set init failed");
        return false;
    }
    if (xTaskCreate(screenTask, "ui", TASK_STACK, set, TASK_PRIORITY, nullptr) != pdPASS) {
        ESP_LOGE(TAG, "task create failed");
        return false;
    }
    return true;
}

void showError(const char *msg)
{
    M5.Display.printf("\nInit failed:\n%s\n", msg);
}

}  // namespace ui
