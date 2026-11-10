# 用于 ESP RainMaker 的 Unit 4Relay

[English](README.md)

这个示例是给 [M5Stack Unit 4Relay](https://docs.m5stack.com/en/unit/Unit_4Relay)（SKU U097）用的 ESP RainMaker 固件。用 ESP RainMaker Home，选择 Classic 公有部署，就可以试用并控制四路继电器：分别开关、定时，以及放进场景。

固件跑在 ESP32-C3 上。产品代码就在本目录。

| 文件 | 作用 |
| --- | --- |
| `main/app_main.c` | RainMaker 节点「四路开关」、四个开关设备、配网 |
| `main/app_driver.c` | Unit 4Relay 的 I2C 驱动、通断状态保存、BOOT 键重置 |
| `main/Kconfig.projbuild` | I2C 引脚和选项（`idf.py menuconfig` → Unit 4Relay） |

## 硬件

使用模块自带的 Grove 线：

| 线色 | 信号 | ESP32-C3 |
| --- | --- | --- |
| 黑 | GND | GND |
| 红 | 5V | 5V，给继电器线圈供电，不要接 3V3 |
| 黄 | SDA | GPIO4 |
| 白 | SCL | GPIO5 |

模块 I2C 地址是 `0x26`。每路触点额定交流 250V / 10A 或直流 28V / 10A。GPIO9 是 BOOT 键，不拿来做 I2C。

## 配网

新板子没有 Wi-Fi 密码。它会打开 BLE 配网，并在串口日志里打印二维码内容，对应这一行：`Provisioning QR`。所有权证明（PoP）是随机的，已经包含在这行内容里。

1. 编译、烧录并打开串口监视器（命令见下方）。
2. 安装 ESP RainMaker Home：[Android](https://play.google.com/store/apps/details?id=com.espressif.novahome)、[iOS](https://apps.apple.com/app/esp-rainmaker-home/id1563728960)。
3. 在启动页选择 **ESP RainMaker Classic**，也就是 Classic 的公有部署。不要选 Neo，也不要选 Private deployment。然后登录。
4. 添加设备，扫描监视器里的二维码。手机会通过 BLE 连上板子。
5. 选择 2.4 GHz 的 Wi-Fi 并输入密码。ESP32-C3 不能连接 5 GHz 网络。
6. 等到板子连上 Wi-Fi，App 里出现节点「四路开关」。

只有在等待配网时才会打印这行二维码。想再看到它，先重置 Wi-Fi（见下方）再重启。

## 使用

节点下面有四个开关：「开关1」到「开关4」。每个开关都有 Power 控制。

- 在 App 里打开或关闭某个开关，Unit 4Relay 上对应的继电器会跟着动作。
- 已启用定时和场景，开关可以定时，也可以和其他 RainMaker 设备编在一起。
- 板子和手机在同一个局域网后，App 也可以本地控制。本地控制使用 security version 0。
- 重新上电后，会恢复每一路上次的通断状态。

按住 BOOT（GPIO9）约 3 秒后松开，清除 Wi-Fi 密码。需要重新配网，RainMaker 节点仍留在账号里。按住约 10 秒后松开，把节点恢复出厂。出厂后要在 App 里重新添加设备。短按 BOOT 不会改变继电器。

## 编译烧录

需要 ESP-IDF 5.1 或更新版本。这份固件用 ESP-IDF 6.0.3 编译过。

```bash
cd examples/unit_4relay
idf.py set-target esp32c3
idf.py build flash monitor
```
