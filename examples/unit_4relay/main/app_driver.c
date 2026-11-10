/*
   M5Stack Unit 4Relay (SKU U097) driver for ESP RainMaker.

   Protocol from Unit_4Relay.pdf (updated 2026-09-29):
     I2C address 0x26
     Reg 0x10 bit0: LED sync, 0 = manual, 1 = auto (LED follows relay)
     Reg 0x11:
       bit7 LED1, bit6 LED2, bit5 LED3, bit4 LED4
       bit3 RELAY1, bit2 RELAY2, bit1 RELAY3, bit0 RELAY4
       1 = ON, 0 = OFF

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <sdkconfig.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <esp_log.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <driver/i2c_master.h>

#include <iot_button.h>
#include <button_gpio.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_params.h>
#include <app_reset.h>

#include "app_priv.h"

static const char *TAG = "unit_4relay";

#define REG_MODE            0x10
#define REG_CTRL            0x11
#define MODE_LED_AUTO       0x01
#define MODE_LED_MANUAL     0x00
#define RELAY_BIT_MASK      0x0F
#define LED_BIT_MASK        0xF0
#define I2C_TIMEOUT_MS      100
#define NVS_NAMESPACE       "u4relay"
#define WIFI_RESET_SECONDS  3
#define FACTORY_RESET_SECONDS 10

static i2c_master_bus_handle_t s_bus;
static i2c_master_dev_handle_t s_dev;
static SemaphoreHandle_t s_lock;
static bool s_state[UNIT_4RELAY_CHANNEL_COUNT];
static bool s_hw_synced;

static uint8_t relay_bit(int channel)
{
    /* RELAY1 is bit3, RELAY4 is bit0. */
    return (uint8_t)(1u << (3 - channel));
}

static uint8_t led_bit(int channel)
{
    /* LED1 is bit7, LED4 is bit4. */
    return (uint8_t)(1u << (7 - channel));
}

static esp_err_t write_reg_locked(uint8_t reg, uint8_t value)
{
    uint8_t buf[2] = { reg, value };
    return i2c_master_transmit(s_dev, buf, sizeof(buf), I2C_TIMEOUT_MS);
}

static esp_err_t read_reg_locked(uint8_t reg, uint8_t *value)
{
    return i2c_master_transmit_receive(s_dev, &reg, 1, value, 1, I2C_TIMEOUT_MS);
}

static void load_state_locked(void)
{
#if CONFIG_UNIT_4RELAY_RESTORE_STATE
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
        return;
    }
    for (int i = 0; i < UNIT_4RELAY_CHANNEL_COUNT; i++) {
        char key[4];
        snprintf(key, sizeof(key), "c%d", i);
        uint8_t value = 0;
        if (nvs_get_u8(handle, key, &value) == ESP_OK) {
            s_state[i] = value != 0;
        }
    }
    nvs_close(handle);
#else
    memset(s_state, 0, sizeof(s_state));
#endif
}

static void save_state_locked(void)
{
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) {
        ESP_LOGW(TAG, "Could not open NVS to store relay state");
        return;
    }
    for (int i = 0; i < UNIT_4RELAY_CHANNEL_COUNT; i++) {
        char key[4];
        snprintf(key, sizeof(key), "c%d", i);
        nvs_set_u8(handle, key, s_state[i] ? 1 : 0);
    }
    nvs_commit(handle);
    nvs_close(handle);
}

static uint8_t build_ctrl_locked(uint8_t current)
{
    uint8_t ctrl = 0;
    for (int i = 0; i < UNIT_4RELAY_CHANNEL_COUNT; i++) {
        if (!s_state[i]) {
            continue;
        }
        ctrl |= relay_bit(i);
#if !CONFIG_UNIT_4RELAY_LED_SYNC
        ctrl |= led_bit(i);
#endif
    }
#if CONFIG_UNIT_4RELAY_LED_SYNC
    /* Auto mode: leave LED bits for the Unit's STM32 to update. */
    ctrl |= (uint8_t)(current & LED_BIT_MASK);
#else
    (void)current;
#endif
    return ctrl;
}

static esp_err_t flush_locked(void)
{
    if (!s_dev) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t mode = CONFIG_UNIT_4RELAY_LED_SYNC ? MODE_LED_AUTO : MODE_LED_MANUAL;
    esp_err_t err = write_reg_locked(REG_MODE, mode);
    if (err != ESP_OK) {
        i2c_master_bus_reset(s_bus);
        return err;
    }

    uint8_t current = 0;
    err = read_reg_locked(REG_CTRL, &current);
    if (err != ESP_OK) {
        current = 0;
    }

    uint8_t ctrl = build_ctrl_locked(current);
    err = write_reg_locked(REG_CTRL, ctrl);
    if (err != ESP_OK) {
        i2c_master_bus_reset(s_bus);
        return err;
    }

    s_hw_synced = true;
    ESP_LOGI(TAG, "Relays %d%d%d%d (reg 0x11 = 0x%02x)",
             s_state[0], s_state[1], s_state[2], s_state[3], ctrl);
    return ESP_OK;
}

static void sync_task(void *arg)
{
    bool announced = false;
    while (true) {
        if (!s_hw_synced && s_lock && xSemaphoreTake(s_lock, pdMS_TO_TICKS(200)) == pdTRUE) {
            esp_err_t err = flush_locked();
            xSemaphoreGive(s_lock);
            if (err != ESP_OK && !announced) {
                ESP_LOGW(TAG, "Unit 4Relay not responding at 0x%02x (%s). Retrying.",
                         CONFIG_UNIT_4RELAY_I2C_ADDR, esp_err_to_name(err));
                announced = true;
            } else if (err == ESP_OK && announced) {
                ESP_LOGI(TAG, "Unit 4Relay is back");
                announced = false;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void app_driver_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    if (!s_lock) {
        ESP_LOGE(TAG, "Failed to create relay mutex");
        return;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    load_state_locked();

    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = CONFIG_UNIT_4RELAY_I2C_PORT,
        .sda_io_num = CONFIG_UNIT_4RELAY_I2C_SDA_GPIO,
        .scl_io_num = CONFIG_UNIT_4RELAY_I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t err = i2c_new_master_bus(&bus_cfg, &s_bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C bus init failed: %s", esp_err_to_name(err));
        xSemaphoreGive(s_lock);
        return;
    }

    err = i2c_master_probe(s_bus, CONFIG_UNIT_4RELAY_I2C_ADDR, I2C_TIMEOUT_MS);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "No ACK from Unit 4Relay at 0x%02x (%s). Check 5V, GND, SDA GPIO%d, SCL GPIO%d.",
                 CONFIG_UNIT_4RELAY_I2C_ADDR, esp_err_to_name(err),
                 CONFIG_UNIT_4RELAY_I2C_SDA_GPIO, CONFIG_UNIT_4RELAY_I2C_SCL_GPIO);
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = CONFIG_UNIT_4RELAY_I2C_ADDR,
        .scl_speed_hz = CONFIG_UNIT_4RELAY_I2C_FREQ_HZ,
    };
    err = i2c_master_bus_add_device(s_bus, &dev_cfg, &s_dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C device add failed: %s", esp_err_to_name(err));
        xSemaphoreGive(s_lock);
        return;
    }

    err = flush_locked();
    xSemaphoreGive(s_lock);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Initial relay write failed: %s", esp_err_to_name(err));
    }

    if (xTaskCreate(sync_task, "u4relay_sync", 3072, NULL, 5, NULL) != pdPASS) {
        ESP_LOGW(TAG, "Could not start relay sync task");
    }
}

esp_err_t app_driver_set_state(int channel, bool on)
{
    if (channel < 0 || channel >= UNIT_4RELAY_CHANNEL_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_lock || xSemaphoreTake(s_lock, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    s_state[channel] = on;
    s_hw_synced = false;
    save_state_locked();
    esp_err_t err = flush_locked();
    xSemaphoreGive(s_lock);

    if (err != ESP_OK) {
        /* Desired state is stored. The sync task retries until the Unit ACKs. */
        ESP_LOGW(TAG, "Switch %d -> %s not applied yet: %s", channel + 1, on ? "ON" : "OFF", esp_err_to_name(err));
    }
    return ESP_OK;
}

bool app_driver_get_state(int channel)
{
    if (channel < 0 || channel >= UNIT_4RELAY_CHANNEL_COUNT || !s_lock) {
        return false;
    }
    if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return false;
    }
    bool on = s_state[channel];
    xSemaphoreGive(s_lock);
    return on;
}

static void local_button_cb(void *arg, void *data)
{
    int channel = (int)(intptr_t)data;
    bool next = !app_driver_get_state(channel);
    if (app_driver_set_state(channel, next) != ESP_OK) {
        return;
    }
    esp_rmaker_device_t *device = switch_devices[channel];
    if (!device) {
        return;
    }
    esp_rmaker_param_update_and_report(
            esp_rmaker_device_get_param_by_name(device, ESP_RMAKER_DEF_POWER_NAME),
            esp_rmaker_bool(next));
}

static esp_err_t add_toggle_button(int gpio, int channel)
{
    if (gpio < 0) {
        return ESP_OK;
    }
    button_config_t btn_cfg = {
        .long_press_time = 0,
        .short_press_time = 0,
    };
    button_gpio_config_t gpio_cfg = {
        .gpio_num = gpio,
        .active_level = 0,
        .enable_power_save = false,
    };
    button_handle_t handle = NULL;
    esp_err_t err = iot_button_new_gpio_device(&btn_cfg, &gpio_cfg, &handle);
    if (err != ESP_OK || !handle) {
        ESP_LOGW(TAG, "Button GPIO %d init failed: %s", gpio, esp_err_to_name(err));
        return err;
    }
    return iot_button_register_cb(handle, BUTTON_SINGLE_CLICK, NULL, local_button_cb, (void *)(intptr_t)channel);
}

void app_driver_register_buttons(void)
{
    const int channel_gpio[UNIT_4RELAY_CHANNEL_COUNT] = {
        CONFIG_UNIT_4RELAY_BTN1_GPIO,
        CONFIG_UNIT_4RELAY_BTN2_GPIO,
        CONFIG_UNIT_4RELAY_BTN3_GPIO,
        CONFIG_UNIT_4RELAY_BTN4_GPIO,
    };

    button_config_t btn_cfg = {
        .long_press_time = 0,
        .short_press_time = 0,
    };
    button_gpio_config_t gpio_cfg = {
        .gpio_num = CONFIG_UNIT_4RELAY_BOOT_GPIO,
        .active_level = 0,
        .enable_power_save = false,
    };
    button_handle_t boot = NULL;
    if (iot_button_new_gpio_device(&btn_cfg, &gpio_cfg, &boot) == ESP_OK && boot) {
        app_reset_button_register(boot, WIFI_RESET_SECONDS, FACTORY_RESET_SECONDS);
    } else {
        ESP_LOGW(TAG, "BOOT button GPIO %d is unavailable", CONFIG_UNIT_4RELAY_BOOT_GPIO);
    }

    for (int i = 0; i < UNIT_4RELAY_CHANNEL_COUNT; i++) {
        if (channel_gpio[i] == CONFIG_UNIT_4RELAY_BOOT_GPIO) {
            ESP_LOGW(TAG, "Switch %d button GPIO %d clashes with BOOT. Skipped.", i + 1, channel_gpio[i]);
            continue;
        }
        add_toggle_button(channel_gpio[i], i);
    }
}
