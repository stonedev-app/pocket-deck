#include <string.h>

#include <string>
#include <vector>

#include "esp_log.h"
#include <M5Unified.hpp>

#include "keyboard.hpp"
#include "player.hpp"
#include "sdcard.hpp"
#include "volume.hpp"
#include "wav.hpp"

static const char *TAG = "pocket-deck";

// 画面は 240x135。文字サイズ 2（12x16）で 1 行 20 文字、見出し 1 行を除いて 7 行入る。
// 長い名前は折り返して行数が増えるので、行頭の印（2 文字）を除いた長さで切る
static constexpr int NAME_CHARS = 18;

static std::vector<std::string> files;
static int selected = 0;
static bool showingResult = false;

// キー入力待ちの時間。再生が終わったかどうかを、この間隔で確認する
static constexpr TickType_t POLL_TICKS = pdMS_TO_TICKS(100);

// 結果画面に出している WAV の解析結果
static wav::Info resultInfo;
static bool resultOk = false;
static char resultErr[40];
static bool resultPlaying = false;  // 画面に出している状態。player::isPlaying() との差で終了を知る

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

static void drawList()
{
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setCursor(0, 0);
    M5.Display.setTextSize(2);

    if (volume::muted()) {
        M5.Display.println("WAV files:  Vol mute");
    } else {
        M5.Display.printf("WAV files:  Vol %d/%d\n", volume::level(), volume::MAX_LEVEL);
    }
    if (files.empty()) {
        M5.Display.println("(none)");
        return;
    }
    for (int i = 0; i < (int)files.size(); i++) {
        M5.Display.printf("%c %.*s\n", i == selected ? '>' : ' ', NAME_CHARS, files[i].c_str());
    }
}

// 選んだ WAV のヘッダを解析して、結果かエラーの理由をログに出す。画面は drawResult() で描く
static void parseSelected()
{
    const std::string &name = files[selected];
    resultOk = wav::parse(sdcard::wavPath(name).c_str(), resultInfo, resultErr, sizeof(resultErr));
    if (!resultOk) {
        ESP_LOGE(TAG, "wav parse failed: %s: %s", name.c_str(), resultErr);
        return;
    }
    ESP_LOGI(TAG, "wav: %s rate=%u ch=%u bits=%u data_offset=%u data_bytes=%u", name.c_str(),
             (unsigned)resultInfo.sampleRate, (unsigned)resultInfo.channels,
             (unsigned)resultInfo.bitsPerSample, (unsigned)resultInfo.dataOffset,
             (unsigned)resultInfo.dataBytes);
}

// 結果画面。形式と、再生中か停止中かを出す（文字サイズ 2 で 8 行まで入る）
static void drawResult()
{
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setCursor(0, 0);
    M5.Display.setTextSize(2);
    M5.Display.printf("%.20s\n", files[selected].c_str());

    if (!resultOk) {
        M5.Display.printf("Error:\n%s\n", resultErr);
        return;
    }
    M5.Display.printf("Rate: %u\nCh: %u\nBits: %u\nData: %u\n", (unsigned)resultInfo.sampleRate,
                      (unsigned)resultInfo.channels, (unsigned)resultInfo.bitsPerSample,
                      (unsigned)resultInfo.dataBytes);
    M5.Display.printf("%s\n", resultPlaying ? "Playing" : "Stopped");
    if (volume::muted()) {
        M5.Display.println("Vol: mute");
    } else {
        M5.Display.printf("Vol: %d/%d\n", volume::level(), volume::MAX_LEVEL);
    }
}

// 再生を止めて、画面に出す状態を停止中にする（描き直しは呼び出し側）
static void stopPlayback()
{
    player::stop();
    resultPlaying = false;
}

// 一覧の操作（j / Down で下、k / Up で上、Enter で決定）と、結果表示の操作
// （Enter で再生 / 停止、Esc で一覧に戻る。再生中なら止めてから戻る）
static void handleKey(const keyboard::KeyEvent &ev)
{
    // Fn + A / S / D は、Key::Char の大文字 'A' / 'S' / 'D' に MOD_FN が付いたイベントになる。
    // Shift + A などは MOD_FN が付かないので区別できる
    if (ev.key == keyboard::Key::Char && (ev.mods & keyboard::MOD_FN) &&
        (ev.ch == 'A' || ev.ch == 'S' || ev.ch == 'D')) {
        if (ev.ch == 'A') volume::up();
        else if (ev.ch == 'S') volume::down();
        else volume::toggleMute();
        if (showingResult) drawResult(); else drawList();
        return;
    }
    if (showingResult) {
        if (ev.key == keyboard::Key::Esc) {
            stopPlayback();
            showingResult = false;
            drawList();
        } else if (ev.key == keyboard::Key::Enter && resultOk) {
            if (resultPlaying) {
                stopPlayback();
            } else {
                resultPlaying = player::start(sdcard::wavPath(files[selected]).c_str(), resultInfo);
            }
            drawResult();
        }
        return;
    }
    if (files.empty()) return;

    bool down = ev.key == keyboard::Key::Down || (ev.key == keyboard::Key::Char && ev.ch == 'j');
    bool up   = ev.key == keyboard::Key::Up || (ev.key == keyboard::Key::Char && ev.ch == 'k');
    if (down && selected < (int)files.size() - 1) {
        selected++;
        drawList();
    } else if (up && selected > 0) {
        selected--;
        drawList();
    } else if (ev.key == keyboard::Key::Enter) {
        showingResult = true;
        resultPlaying = false;
        parseSelected();
        drawResult();
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
    if (!sdcard::listWav(files)) {
        M5.Display.println("opendir failed");
        return;
    }
    drawList();

    if (!keyboard::begin()) {
        ESP_LOGE(TAG, "keyboard init failed");
        return;
    }

    keyboard::KeyEvent ev;
    while (1) {
        if (keyboard::poll(ev, POLL_TICKS)) {
            showKey(ev);
            handleKey(ev);
        }
        volume::tick();
        // 最後まで再生が終わったら、停止中の表示に戻す（描画はこのタスクだけで行う）
        if (showingResult && resultPlaying && !player::isPlaying()) {
            resultPlaying = false;
            drawResult();
        }
    }
}
