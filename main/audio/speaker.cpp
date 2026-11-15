#include "audio/speaker.hpp"

#include <M5Unified.hpp>

#include "audio/volume.hpp"
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
    volume::begin();
}

}  // namespace speaker
