/*
   Unit 4Relay RainMaker switch

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#pragma once

#include <stdbool.h>
#include <esp_err.h>
#include <esp_rmaker_core.h>

#define UNIT_4RELAY_CHANNEL_COUNT 4

extern esp_rmaker_device_t *switch_devices[UNIT_4RELAY_CHANNEL_COUNT];

void app_driver_init(void);
void app_driver_register_buttons(void);
esp_err_t app_driver_set_state(int channel, bool on);
bool app_driver_get_state(int channel);
