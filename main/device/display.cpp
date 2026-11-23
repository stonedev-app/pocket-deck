#include "device/display.hpp"

#include <M5Unified.hpp>

namespace display {

int32_t width()
{
    return M5.Display.width();
}

int32_t height()
{
    return M5.Display.height();
}

void setBacklight(Backlight level)
{
    // 元の明るさは、最初に切り替えるときに読み取って覚えておく
    static uint8_t full = 0;
    if (full == 0) {
        full = M5.Display.getBrightness();
        if (full == 0) full = 128;
    }
    switch (level) {
    case Backlight::Full: M5.Display.setBrightness(full);     break;
    case Backlight::Dim:  M5.Display.setBrightness(full / 8); break;
    case Backlight::Off:  M5.Display.setBrightness(0);        break;
    }
}

void flush(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t *pixels)
{
    M5.Display.startWrite();
    M5.Display.pushImage(x, y, w, h, reinterpret_cast<const lgfx::rgb565_t *>(pixels));
    M5.Display.endWrite();
}

}  // namespace display
