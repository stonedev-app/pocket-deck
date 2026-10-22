#include "freertos/FreeRTOS.h"  // IWYU pragma: keep
#include "freertos/task.h"
#include "esp_log.h"
#include <M5Unified.hpp>

static const char *TAG = "pocket-deck";

extern "C" void app_main(void)
{
    M5.begin();
    ESP_LOGI(TAG, "display: %d x %d", (int)M5.Display.width(), (int)M5.Display.height());

    M5.Display.setTextSize(2);
    M5.Display.println("pocket-deck");

    uint32_t count = 0;

    while (1) {
        ESP_LOGI(TAG, "count = %lu", (unsigned long)count);
        M5.Display.setCursor(0, 32);
        M5.Display.printf("count = %lu   ", (unsigned long)count++);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
