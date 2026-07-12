# ESP32 AirQualityMonitor

> 基于 ESP32 的多传感器室内空气质量监测仪。集成颗粒物、VOC/甲醛/CO₂/温湿度、光照传感器，OLED 实时显示，BLE 手机端监控与报警。

![PlatformIO](https://img.shields.io/badge/PlatformIO-ESP32-orange)
![Framework](https://img.shields.io/badge/framework-Arduino-00979D)
![BLE](https://img.shields.io/badge/BLE-4.2-blue)
![License](https://img.shields.io/badge/license-MIT-green)

---

## 功能概览

- **多传感器融合** — PMS7003（颗粒物）、21VOC 五合一（VOC/甲醛/eCO₂/温湿度）、BH1750（光照）
- **OLED 显示** — 128x64 SSD1306，空气质量等级、各参数值、报警状态一目了然
- **BLE 监控** — Nordic UART Service，手机 App 实时接收数据，支持远程静音/刷新
- **智能报警** — PM2.5 > 75 或 VOC > 500 触发蜂鸣器，支持板载按键或蓝牙静音

---

## 快速开始

```bash
# 编译
pio run -e esp32dev

# 上传固件（COM5）
pio run -e esp32dev -t upload --upload-port COM5

# 串口监视器
pio device monitor -p COM5 -b 115200
```

环境要求：[PlatformIO](https://platformio.org/)（VS Code 扩展或 CLI），Python 3.6+。

---

## 硬件连接

| 模块 | 接口 | 引脚 |
|------|------|------|
| OLED SSD1306 | I2C (0x3C) | SDA=21, SCL=22 |
| BH1750 光照 | I2C (0x23) | 共用 I2C 总线 |
| PMS7003 颗粒物 | UART (9600) | RX=16, TX=17 |
| 21VOC 五合一 | UART (9600) | RX=18, TX=19 |
| 蜂鸣器 | GPIO 直驱 | GPIO 25（低电平触发，VCC 接 3.3V） |
| 静音按键 | 数字输入 | GPIO 0（BOOT 按钮，长按 1s） |

> 详细原理图、BOM、Layout 建议见 [Hardware.md](Hardware.md)。

---

## BLE 协议

设备名 **AirQualityMonitor**，使用 Nordic UART Service：

```
Service:   6E400001-B5A3-F393-E0A9-E50E24DCCA9E
TX (Notify): 6E400003-B5A3-F393-E0A9-E50E24DCCA9E  (ESP → 手机)
RX (Write):  6E400002-B5A3-F393-E0A9-E50E24DCCA9E  (手机 → ESP)
```

**上行数据**（每秒推送）：
```
PM2.5:35 PM10:48 T:25.3 H:62.5 VOC:298 HCHO:31 eCO2:300 LUX:1200 Grade:1 Alarm:ON
```

**下行指令**：

| 指令 | 功能 |
|------|------|
| `'1'` | 启用蜂鸣器 |
| `'0'` | 静音 10 分钟 |
| `'R'` | 强制刷新数据 |

---

## 项目结构

```
src/
├── main.cpp               # 主程序：初始化、主循环、阈值、全局变量
├── pms70xx.h / .c         # PMS7003 帧解析器（C 状态机）
├── VOC21Driver.h / .cpp   # 21VOC 五合一传感器驱动
├── BH1750Driver.h / .cpp  # BH1750 光照传感器驱动
├── OLEDDriver.h / .cpp    # SSD1306 OLED 显示驱动
├── AlarmManager.h / .cpp  # 蜂鸣器报警 + 静音按键管理
└── BluetoothManager.h/.cpp # BLE NUS 通信管理
```

---

## 报警策略

| 条件 | 动作 |
|------|------|
| PM2.5 > 75 μg/m³ **或** VOC > 500 μg/m³ | 蜂鸣器报警 |
| 首次触发 | 短促 200ms × 2 提示 |
| 持续报警 | 500ms 响 / 500ms 停 间歇循环 |
| 条件解除 | 自动停止 |
| 静音方式 | 长按 BOOT 按钮 1s 或蓝牙发送 `'0'` |
| 静音超时 | 10 分钟自动恢复 |

---

## 空气质量分级

| 等级 | PM2.5 (μg/m³) | 编码 |
|------|---------------|------|
| Good | ≤ 35 | 0 |
| Fair | 36~75 | 1 |
| Light Pollution | 76~115 | 2 |
| Moderate Pollution | 116~150 | 3 |
| Heavy Pollution | > 150 | 4 |

---

## License

MIT
