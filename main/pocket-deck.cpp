#include "startup.hpp"

extern "C" void app_main(void)
{
    startup::run();
}
