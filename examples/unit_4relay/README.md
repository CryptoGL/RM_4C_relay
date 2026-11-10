# Unit 4Relay 四路开关

基于 ESP RainMaker 的四路开关固件，驱动 M5Stack Unit 4Relay（SKU U097，见 `Unit_4Relay.pdf`）。手机 App 里是一个节点上的四个 Switch 设备（开关1–开关4），可分别开关、定时和加入场景。

## 接线

Grove HY2.0-4P：

| 线色 | 信号 | 说明 |
| --- | --- | --- |
| 黑 | GND | 共地 |
| 红 | 5V | 继电器线圈供电，接到板子的 5V，不要接 3V3 |
| 黄 | SDA | ESP32-C3 默认 GPIO4 |
| 白 | SCL | ESP32-C3 默认 GPIO5 |

I2C 地址 `0x26`。寄存器 `0x10` bit0 = 1 时，板上 LED 跟随继电器。寄存器 `0x11`：bit3–bit0 分别是继电器 1–4，1 为吸合。

ESP32-C3 上 GPIO9 是 BOOT 键，所以 I2C 不用它。换板子时在 `idf.py menuconfig` → `Unit 4Relay` 里改 SDA/SCL。

## 编译烧录

目标芯片是 ESP32-C3。IDF 使用 `/Users/LZJ/esp/esp-idf-6.0.3`。

```bash
. /Users/LZJ/esp/esp-idf-6.0.3/export.sh
cd examples/unit_4relay
idf.py set-target esp32c3
idf.py build flash monitor
```

配网二维码打印在串口日志 `Provisioning QR`。用 ESP RainMaker App 扫码添加。BOOT 键按住 3 秒清除 Wi-Fi，按住 10 秒恢复出厂。短按 BOOT 不会改变继电器。

四路本地按键默认关闭（GPIO 设为 -1）。需要时在 menuconfig 里给每一路指定一个低电平有效的 GPIO。

断电后默认恢复上次的通断状态。若希望上电全关，关掉 `Restore relay state after reboot`。
