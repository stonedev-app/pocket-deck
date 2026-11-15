#include "app/playback.hpp"

#include <string>

#include "app/playlist.hpp"
#include "audio/player.hpp"
#include "audio/wav.hpp"
#include "esp_log.h"

namespace playback {

static const char *TAG = "playback";

static State s_state = State::Stopped;  // 終了通知を受けたら Stopped にする
static bool s_finished = false;         // 最後まで再生して終わった
static bool s_repeat = false;           // 1 曲リピート
static int s_index = -1;                // 今の曲の、一覧での位置
static std::string s_path;              // 今の曲のパス（停止後の再生・リピートで使う）
static wav::Info s_info;
static uint32_t s_durationMs = 0;

/** @brief 溜まっている終了通知を捨てる（自分で止めた・始め直したときの通知を、終了と取り違えないため） */
static void drainEvents()
{
    player::EndReason reason;
    while (player::receive(reason, 0)) {}
}

/** @brief 今の曲を最初から再生する（停止後の Space とリピートで使う）。成功で true */
static bool restart()
{
    player::stop();
    drainEvents();
    if (!player::start(s_path.c_str(), s_info)) {
        ESP_LOGE(TAG, "player restart failed");
        s_state = State::Stopped;
        return false;
    }
    s_state = State::Playing;
    s_finished = false;
    return true;
}

bool play(int index)
{
    const char *name = playlist::nameAt(index);
    if (!name) return false;
    const std::string path = playlist::pathAt(index);
    wav::Info info;
    char err[40] = "";
    if (!wav::parse(path.c_str(), info, err, sizeof(err))) {
        ESP_LOGE(TAG, "wav parse failed: %s: %s", name, err);
        return false;
    }
    ESP_LOGI(TAG, "wav: %s rate=%u ch=%u bits=%u data_offset=%u data_bytes=%u", name,
             (unsigned)info.sampleRate, (unsigned)info.channels, (unsigned)info.bitsPerSample,
             (unsigned)info.dataOffset, (unsigned)info.dataBytes);

    // 前の曲が再生中・一時停止中でも止める。古い通知が残っていると、新しい再生が終わったことになる
    player::stop();
    drainEvents();
    if (!player::start(path.c_str(), info)) {
        ESP_LOGE(TAG, "player start failed: %s", name);
        s_state = State::Stopped;
        return false;
    }
    s_index = index;
    s_path = path;
    s_info = info;
    s_durationMs = player::durationMs(info);
    s_state = State::Playing;
    s_finished = false;
    return true;
}

void togglePause()
{
    switch (s_state) {
    case State::Playing:
        if (player::pause()) {
            drainEvents();  // 一時停止で出る Stopped の通知は終了ではない
            s_state = State::Paused;
        }
        break;
    case State::Paused:
        if (player::resume()) s_state = State::Playing;
        else ESP_LOGE(TAG, "player resume failed");
        break;
    case State::Stopped:
        restart();
        break;
    }
}

void next()
{
    if (s_index >= 0 && s_index + 1 < playlist::count()) play(s_index + 1);
}

void prev()
{
    if (s_index > 0) play(s_index - 1);
}

void toggleRepeat()
{
    s_repeat = !s_repeat;
}

void stop()
{
    player::stop();
    drainEvents();
    s_state = State::Stopped;
}

void update()
{
    player::EndReason reason;
    if (!player::receive(reason, 0)) return;
    ESP_LOGI(TAG, "playback end: %d", (int)reason);
    s_state = State::Stopped;
    // 最後まで再生したときは、位置が端数でずれないよう満了の表示にする
    s_finished = reason == player::EndReason::Finished;
    // リピートがオンなら、同じ曲を最初から始める
    if (s_finished && s_repeat) restart();
}

State state()
{
    return s_state;
}

bool repeat()
{
    return s_repeat;
}

int index()
{
    return s_index;
}

uint32_t positionMs()
{
    uint32_t pos = s_finished ? s_durationMs : player::positionMs();
    if (pos > s_durationMs) pos = s_durationMs;
    return pos;
}

uint32_t durationMs()
{
    return s_durationMs;
}

}  // namespace playback
