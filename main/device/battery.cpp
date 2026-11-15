#include "device/battery.hpp"

#include <M5Unified.hpp>

namespace battery {

int percent()
{
    const int32_t raw = M5.Power.getBatteryLevel();
    return (raw >= 0 && raw <= 100) ? (int)raw : -1;
}

}  // namespace battery
