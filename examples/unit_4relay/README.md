# Unit 4Relay for ESP RainMaker

[中文](README_CN.md)

This example is a small ESP RainMaker firmware for the [M5Stack Unit 4Relay](https://docs.m5stack.com/en/unit/Unit_4Relay) (SKU U097). Use the ESP RainMaker Home app, on the public Classic deployment, to try the four relays: turn each channel on or off, add schedules, and use them in scenes.

The firmware runs on ESP32-C3. The product code is in this directory.

| File | What it does |
| --- | --- |
| `main/app_main.c` | RainMaker node「四路开关」, four switch devices, provisioning |
| `main/app_driver.c` | I2C driver for the Unit 4Relay, saved on/off state, BOOT-button reset |
| `main/Kconfig.projbuild` | I2C pins and options (`idf.py menuconfig` → Unit 4Relay) |

## Hardware

Connect the Grove cable packed with the unit:

| Wire | Signal | ESP32-C3 |
| --- | --- | --- |
| Black | GND | GND |
| Red | 5V | 5V, for the relay coils. Do not use 3V3 |
| Yellow | SDA | GPIO4 |
| White | SCL | GPIO5 |

The module's I2C address is `0x26`. Each relay is rated AC 250V / 10A or DC 28V / 10A. GPIO9 is the BOOT button, so it is not used for I2C.

## Provisioning

A new board has no Wi-Fi credentials. It advertises BLE provisioning and prints a QR payload on the serial log, on the line `Provisioning QR`. The proof of possession (PoP) is random and is included in that payload.

1. Build, flash, and open the serial monitor (commands below).
2. Install ESP RainMaker Home: [Android](https://play.google.com/store/apps/details?id=com.espressif.novahome), [iOS](https://apps.apple.com/app/esp-rainmaker-home/id1563728960).
3. On the landing screen, choose **ESP RainMaker Classic**. That is the public Classic deployment. Do not choose Neo or Private deployment. Sign in.
4. Add a device and scan the QR code from the monitor. The phone connects to the board over BLE.
5. Choose a 2.4 GHz Wi-Fi network and enter its password. ESP32-C3 cannot join 5 GHz networks.
6. Wait until the board joins Wi-Fi and shows up in the app as the node「四路开关」.

The QR line is printed only while the board is waiting to be provisioned. To see it again, reset Wi-Fi (below) and reboot.

## Using the relays

The node has four switch devices,「开关1」to「开关4」. Each has a Power control.

- In the app, turn a switch on or off. The matching relay on the Unit 4Relay follows.
- Schedules and scenes are enabled, so a switch can be timed or grouped with other RainMaker devices.
- After the board is on the same LAN as the phone, the app can also control it locally. Local control uses security version 0.
- Power-on restores the last on/off state of each channel.

Hold BOOT (GPIO9) for about 3 seconds, then release, to erase the Wi-Fi password. Provision the board again; the RainMaker node stays on your account. Hold BOOT for about 10 seconds, then release, to factory-reset the node. Add the device in the app again after a factory reset. A short press of BOOT does not change the relays.

## Build and flash

ESP-IDF 5.1 or newer. This firmware was built with ESP-IDF 6.0.3.

```bash
cd examples/unit_4relay
idf.py set-target esp32c3
idf.py build flash monitor
```
