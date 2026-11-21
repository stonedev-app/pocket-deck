#include "device/speaker.hpp"

#include <M5Unified.hpp>

#include "task_config.hpp"

namespace speaker {

void begin()
{
    M5.Speaker.end();
    auto spk = M5.Speaker.config();
    spk.task_priority = task_config::SPEAKER.priority;
    spk.task_pinned_core = task_config::SPEAKER.core;
    M5.Speaker.config(spk);
    M5.Speaker.begin();
}

void setVolume(uint8_t v)
{
    M5.Speaker.setVolume(v);
}

void play(const int16_t *data, size_t count, uint32_t sampleRate, bool stereo, uint8_t channel)
{
    M5.Speaker.playRaw(data, count, sampleRate, stereo, 1, channel);
}

bool isPlaying(uint8_t channel)
{
    return M5.Speaker.isPlaying(channel);
}

void stop(uint8_t channel)
{
    M5.Speaker.stop(channel);
}

}  // namespace speaker
