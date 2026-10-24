#include "keyboard.hpp"

#include <string.h>

#include "freertos/FreeRTOS.h"  // IWYU pragma: keep
#include "freertos/queue.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_log.h"
#include <M5Unified.hpp>

// TCA8418 のレジスタ配置とキー配列は docs/cardputer-adv-peripherals.md の 4 章に基づく。
// ライセンスの都合（4.8）で、GPLv3 表記のある keymap.h は使わず、配列は自前で定義している。

namespace keyboard {

static const char *TAG = "keyboard";

static constexpr uint8_t  TCA8418_ADDR    = 0x34;
static constexpr uint32_t I2C_FREQ        = 400000;
static constexpr uint8_t  REG_CFG         = 0x01;
static constexpr uint8_t  REG_INT_STAT    = 0x02;
static constexpr uint8_t  REG_KEY_LCK_EC  = 0x03;
static constexpr uint8_t  REG_KEY_EVENT_A = 0x04;
static constexpr uint8_t  REG_KP_GPIO1    = 0x1D;
static constexpr uint8_t  REG_KP_GPIO2    = 0x1E;
static constexpr uint8_t  REG_KP_GPIO3    = 0x1F;
static constexpr uint8_t  CFG_KE_IEN      = 0x01;
static constexpr uint8_t  INT_STAT_K_INT  = 0x01;

static constexpr int ROWS = 4;
static constexpr int COLS = 14;

// 配列中の特殊キー（ASCII 文字と衝突しない 0x80 以上）
enum : uint8_t {
    S_FN = 0x80, S_SHIFT, S_CTRL, S_OPT, S_ALT, S_DEL, S_TAB, S_ENTER, S_SPACE,
};

// 通常時 [row][col]
static const uint8_t kBase[ROWS][COLS] = {
    {'`', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', S_DEL},
    {S_TAB, 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\\'},
    {S_FN, S_SHIFT, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', S_ENTER},
    {S_CTRL, S_OPT, S_ALT, 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', S_SPACE},
};

// Shift 時。文字キー以外は通常時と同じ
static const uint8_t kShift[ROWS][COLS] = {
    {'~', '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', S_DEL},
    {S_TAB, 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '|'},
    {S_FN, S_SHIFT, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', S_ENTER},
    {S_CTRL, S_OPT, S_ALT, 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', S_SPACE},
};

static constexpr int QUEUE_SIZE = 16;
static constexpr uint32_t TASK_STACK_SIZE = 4096;
static constexpr UBaseType_t TASK_PRIORITY = 5;
static constexpr gpio_num_t INT_PIN = GPIO_NUM_11;  // TCA8418 の INT（Low アクティブ）
static constexpr TickType_t FALLBACK_PERIOD = pdMS_TO_TICKS(100);  // 割り込みを取りこぼした場合の保険
static constexpr int MAX_RESCAN = 4;
static QueueHandle_t s_queue = nullptr;
static TaskHandle_t s_task = nullptr;
static uint8_t s_mods = 0;  // 入力タスクだけが扱うので排他制御は不要

static void push(const KeyEvent &ev)
{
    if (s_queue) {
        xQueueSend(s_queue, &ev, 0);  // 満杯なら新しいイベントを捨てる
    }
}

static void setName(KeyEvent &ev, const char *name)
{
    strncpy(ev.name, name, sizeof(ev.name) - 1);
    ev.name[sizeof(ev.name) - 1] = '\0';
}

static void setChar(KeyEvent &ev, char c)
{
    ev.key = Key::Char;
    ev.ch = c;
    ev.name[0] = c;
    ev.name[1] = '\0';
}

static uint8_t modBit(uint8_t sp)
{
    switch (sp) {
    case S_FN:    return MOD_FN;
    case S_SHIFT: return MOD_SHIFT;
    case S_CTRL:  return MOD_CTRL;
    case S_OPT:   return MOD_OPT;
    case S_ALT:   return MOD_ALT;
    default:      return 0;
    }
}

// Fn 層（4.5）。定義があれば true
static bool fnLayer(uint8_t base, KeyEvent &ev)
{
    switch (base) {
    case ';':   ev.key = Key::Up;     setName(ev, "Up");     return true;
    case ',':   ev.key = Key::Left;   setName(ev, "Left");   return true;
    case '.':   ev.key = Key::Down;   setName(ev, "Down");   return true;
    case '/':   ev.key = Key::Right;  setName(ev, "Right");  return true;
    case '`':   ev.key = Key::Esc;    setName(ev, "Esc");    return true;
    case S_DEL: ev.key = Key::Delete; setName(ev, "Delete"); return true;
    default:
        if (base >= 'a' && base <= 'z') {  // Fn + 英字は大文字
            setChar(ev, (char)(base - 'a' + 'A'));
            return true;
        }
        return false;
    }
}

static void handleEvent(uint8_t raw)
{
    const bool pressed = raw & 0x80;
    const int v = (raw & 0x7F) - 1;
    if (v < 0) {
        return;
    }
    // TCA8418 の座標から物理配列へ変換（4.3）
    const int rawRow = v / 10;
    const int rawCol = v % 10;
    const int col = rawRow * 2 + (rawCol > 3 ? 1 : 0);
    const int row = (rawCol + 4) % 4;
    if (row >= ROWS || col >= COLS) {
        return;
    }

    const uint8_t base = kBase[row][col];
    const uint8_t bit = modBit(base);
    if (bit) {
        const bool wasHeld = s_mods & bit;
        if (pressed) {
            s_mods |= bit;
        } else {
            s_mods &= ~bit;
        }
        if (!pressed || wasHeld) {
            return;
        }
    } else if (!pressed) {
        return;  // 離上は通知しない
    }

    KeyEvent ev = {};
    ev.mods = s_mods;

    if (bit) {
        switch (base) {
        case S_FN:    ev.key = Key::Fn;    setName(ev, "Fn");    break;
        case S_SHIFT: ev.key = Key::Shift; setName(ev, "Shift"); break;
        case S_CTRL:  ev.key = Key::Ctrl;  setName(ev, "Ctrl");  break;
        case S_OPT:   ev.key = Key::Opt;   setName(ev, "Opt");   break;
        default:      ev.key = Key::Alt;   setName(ev, "Alt");   break;
        }
        push(ev);
        return;
    }

    if (!((s_mods & MOD_FN) && fnLayer(base, ev))) {
        const uint8_t c = (s_mods & MOD_SHIFT) ? kShift[row][col] : base;
        switch (c) {
        case S_DEL:   ev.key = Key::Backspace; setName(ev, "Backspace"); break;
        case S_TAB:   ev.key = Key::Tab;       setName(ev, "Tab");       break;
        case S_ENTER: ev.key = Key::Enter;     setName(ev, "Enter");     break;
        case S_SPACE: ev.key = Key::Space;     setName(ev, "Space");     break;
        default:      setChar(ev, (char)c);                              break;
        }
    }
    push(ev);
}

// イベント FIFO を読み切って INT を解除する
static void scan()
{
    int n = M5.In_I2C.readRegister8(TCA8418_ADDR, REG_KEY_LCK_EC, I2C_FREQ) & 0x0F;
    while (n-- > 0) {
        const uint8_t raw = M5.In_I2C.readRegister8(TCA8418_ADDR, REG_KEY_EVENT_A, I2C_FREQ);
        if (raw == 0) {
            break;
        }
        handleEvent(raw);
    }
    M5.In_I2C.writeRegister8(TCA8418_ADDR, REG_INT_STAT, INT_STAT_K_INT, I2C_FREQ);
}

// I2C は ISR から呼べないので、入力タスクを起こすだけにする
static void IRAM_ATTR intHandler(void *)
{
    BaseType_t woken = pdFALSE;
    vTaskNotifyGiveFromISR(s_task, &woken);
    portYIELD_FROM_ISR(woken);
}

static void inputTask(void *)
{
    while (1) {
        ulTaskNotifyTake(pdTRUE, FALLBACK_PERIOD);
        // 読み出し中に新しいイベントが来て INT が Low のままなら続けて読む
        for (int i = 0; i < MAX_RESCAN; i++) {
            scan();
            if (gpio_get_level(INT_PIN) != 0) {
                break;
            }
        }
    }
}

static bool setupInterrupt()
{
    gpio_config_t cfg = {};
    cfg.pin_bit_mask = 1ULL << INT_PIN;
    cfg.mode = GPIO_MODE_INPUT;
    cfg.pull_up_en = GPIO_PULLUP_ENABLE;
    cfg.intr_type = GPIO_INTR_NEGEDGE;
    esp_err_t err = gpio_config(&cfg);
    if (err == ESP_OK) {
        err = gpio_install_isr_service(0);
        if (err == ESP_ERR_INVALID_STATE) {  // 他で導入済み
            err = ESP_OK;
        }
    }
    if (err == ESP_OK) {
        err = gpio_isr_handler_add(INT_PIN, intHandler, nullptr);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "interrupt setup failed: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}

bool begin()
{
    // 7 行 × 8 列のキーマトリクスに設定し、イベントバッファを空にする（4.1）
    bool ok = M5.In_I2C.writeRegister8(TCA8418_ADDR, REG_KP_GPIO1, 0x7F, I2C_FREQ);
    ok &= M5.In_I2C.writeRegister8(TCA8418_ADDR, REG_KP_GPIO2, 0xFF, I2C_FREQ);
    ok &= M5.In_I2C.writeRegister8(TCA8418_ADDR, REG_KP_GPIO3, 0x00, I2C_FREQ);
    ok &= M5.In_I2C.writeRegister8(TCA8418_ADDR, REG_CFG, CFG_KE_IEN, I2C_FREQ);
    if (!ok) {
        ESP_LOGE(TAG, "TCA8418 init failed");
        return false;
    }
    for (int i = 0; i < 16; i++) {  // FIFO は最大 10 件
        if (M5.In_I2C.readRegister8(TCA8418_ADDR, REG_KEY_EVENT_A, I2C_FREQ) == 0) {
            break;
        }
    }
    M5.In_I2C.writeRegister8(TCA8418_ADDR, REG_INT_STAT, 0xFF, I2C_FREQ);
    s_mods = 0;

    if (!s_queue) {
        s_queue = xQueueCreate(QUEUE_SIZE, sizeof(KeyEvent));
        if (!s_queue) {
            ESP_LOGE(TAG, "queue create failed");
            return false;
        }
        if (xTaskCreate(inputTask, "keyboard", TASK_STACK_SIZE, nullptr, TASK_PRIORITY, &s_task) != pdPASS) {
            ESP_LOGE(TAG, "task create failed");
            vQueueDelete(s_queue);
            s_queue = nullptr;
            return false;
        }
        if (!setupInterrupt()) {
            vTaskDelete(s_task);
            s_task = nullptr;
            vQueueDelete(s_queue);
            s_queue = nullptr;
            return false;
        }
        xTaskNotifyGive(s_task);  // 登録前に押されていたキーを拾う
    }
    return true;
}

bool poll(KeyEvent &out, TickType_t timeout)
{
    if (!s_queue) {
        return false;
    }
    return xQueueReceive(s_queue, &out, timeout) == pdTRUE;
}

}  // namespace keyboard
