#include <string.h>

#include <string>
#include <vector>

#include "esp_log.h"
#include <M5Unified.hpp>

#include "keyboard.hpp"
#include "sdcard.hpp"
#include "wav.hpp"

static const char *TAG = "pocket-deck";

// 画面は 240x135。文字サイズ 2（12x16）で 1 行 20 文字、見出し 1 行を除いて 7 行入る。
// 長い名前は折り返して行数が増えるので、行頭の印（2 文字）を除いた長さで切る
static constexpr int NAME_CHARS = 18;

static std::vector<std::string> files;
static int selected = 0;
static bool showingResult = false;

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

    M5.Display.println("WAV files:");
    if (files.empty()) {
        M5.Display.println("(none)");
        return;
    }
    for (int i = 0; i < (int)files.size(); i++) {
        M5.Display.printf("%c %.*s\n", i == selected ? '>' : ' ', NAME_CHARS, files[i].c_str());
    }
}

// 選んだ WAV のヘッダを解析して、結果かエラーの理由を画面とログに出す
static void drawResult()
{
    const std::string &name = files[selected];
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setCursor(0, 0);
    M5.Display.setTextSize(2);
    M5.Display.printf("%.20s\n", name.c_str());

    wav::Info info;
    char err[40];
    if (!wav::parse(sdcard::wavPath(name).c_str(), info, err, sizeof(err))) {
        ESP_LOGE(TAG, "wav parse failed: %s: %s", name.c_str(), err);
        M5.Display.printf("Error:\n%s\n", err);
        return;
    }

    ESP_LOGI(TAG, "wav: %s rate=%u ch=%u bits=%u data_offset=%u data_bytes=%u", name.c_str(),
             (unsigned)info.sampleRate, (unsigned)info.channels, (unsigned)info.bitsPerSample,
             (unsigned)info.dataOffset, (unsigned)info.dataBytes);
    M5.Display.printf("Rate: %u\nCh: %u\nBits: %u\nData: %u\n", (unsigned)info.sampleRate,
                      (unsigned)info.channels, (unsigned)info.bitsPerSample, (unsigned)info.dataBytes);
}

// 一覧の操作（j / Down で下、k / Up で上、Enter で決定）と、結果表示からの復帰（Esc）
static void handleKey(const keyboard::KeyEvent &ev)
{
    if (showingResult) {
        if (ev.key == keyboard::Key::Esc) {
            showingResult = false;
            drawList();
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
        drawResult();
    }
}

extern "C" void app_main(void)
{
    M5.begin();
    ESP_LOGI(TAG, "display: %d x %d", (int)M5.Display.width(), (int)M5.Display.height());

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
        if (keyboard::poll(ev, portMAX_DELAY)) {
            showKey(ev);
            handleKey(ev);
        }
    }
}
