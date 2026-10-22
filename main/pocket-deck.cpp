#include "freertos/FreeRTOS.h"  // IWYU pragma: keep
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "pocket-deck";

extern "C" void app_main(void)
{
    uint32_t count = 0;

    while (1) {
        ESP_LOGI(TAG, "count = %lu", (unsigned long)count++);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
