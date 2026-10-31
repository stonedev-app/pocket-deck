#include "ui.hpp"

#include <M5Unified.hpp>

#include "volume.hpp"

namespace ui {

State state;

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

void drawList()
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
void drawResult()
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

}  // namespace ui
