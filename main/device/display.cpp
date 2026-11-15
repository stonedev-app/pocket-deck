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

void flush(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t *pixels)
{
    M5.Display.startWrite();
    M5.Display.pushImage(x, y, w, h, reinterpret_cast<const lgfx::rgb565_t *>(pixels));
    M5.Display.endWrite();
}

}  // namespace display
