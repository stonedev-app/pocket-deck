#include "ui/ui.hpp"

#include <M5Unified.hpp>

namespace ui {

void showError(const char *msg)
{
    M5.Display.printf("\nInit failed:\n%s\n", msg);
}

}  // namespace ui
