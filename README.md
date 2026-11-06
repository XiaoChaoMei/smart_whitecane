# ESP32 智能导盲杖固件说明

本文档说明当前固件的硬件组成、接线、GPIO 分配、通信协议、编译方法和常见
故障处理。引脚以 `include/config.h` 为准；本文与代码不一致时，应先检查
`config.h` 是否被修改。

## 1. 工程概览

- 主控：ESP32 DevKit V1，ESP32-D0WDQ6，双核 240 MHz
- Flash：4 MB
- 开发环境：VS Code + PlatformIO
- Wi-Fi：2.4 GHz
- 云平台：巴法云 MQTT
- 定位：GPS/北斗等多模 GNSS 模块，TTL UART NMEA
- 主要功能：三向避障、振动提醒、积水检测、GPS 定位、自动补光、SOS 和
  DFPlayer 语音提示

当前板卡实测芯片为 ESP32-D0WDQ6，Flash 容量为 4 MB。工程使用
`board = esp32dev`。

## 2. 当前工程结构

```text
ESP32-DaoMangZhang/
├─ include/
│  ├─ config.h                 # 引脚、阈值、主题和业务参数
│  ├─ secrets.h                # 本机 Wi-Fi/私钥配置，不提交
│  ├─ secrets.example.h        # 配置模板
│  ├─ control/                 # 控制类头文件
│  ├─ network/                 # MQTT 头文件
│  └─ sensors/                 # 传感器头文件
├─ src/
│  ├─ main.cpp                 # 初始化和主循环
│  ├─ control/                 # 按钮、灯光、振动、DFPlayer
│  ├─ network/                 # 巴法云 MQTT
│  └─ sensors/                 # 超声波、光照、水浸、GPS
├─ audio/01/                   # DFPlayer 语音文件
├─ lib/                        # 预留本地库目录
├─ test/                       # 预留测试目录
└─ platformio.ini
```

组件头文件统一放在 `include/`，实现文件放在 `src/`。

## 3. 硬件模块清单

| 模块 | 工程中的用途 | 当前型号/说明 |
| --- | --- | --- |
| ESP32 开发板 | 主控、Wi-Fi、MQTT、传感器处理 | ESP32 DevKit V1 / ESP32-D0WDQ6 / 4 MB |
| HC-SR04 超声波模块 ×3 | 前方、左侧、右侧测距 | 5V 供电，ECHO 必须确认电平匹配 |
| BH1750 光照传感器 | 测量环境照度，自动控制补光灯 | I2C，地址 `0x23` 或 `0x5C` |
| 水浸传感器模块 | 检测探头是否接触积水 | 模拟输出接 ADC |
| GPS/GNSS 模块 | 获取经纬度、卫星数和定位状态 | BOM 记录为 GY-NEO-6M；实际串口为多模 GNSS |
| 按键模块 | 短按切换补光灯，长按 SOS | 内部上拉，低电平按下 |
| DFPlayer Mini | 播放障碍、积水、灯光和 SOS 提示音 | UART1，9600 8N1，microSD 卡 |
| 扬声器 | 语音输出 | BOM 记录为 4 Ω 3 W |
| 扁平振动马达 ×2 | 提示左、右和前方危险 | 3V 马达，必须经过驱动 |
| MOSFET 模块 ×2 | 左右马达 PWM 驱动 | 由 GPIO13/14 控制 |
| MOSFET 模块 ×1 | 补光灯控制 | GPIO25，高电平开启 |
| 低压 LED 灯带 | 暗光照明 | 实际供电电压按灯带规格 |
| 5V 2000 mAh 电池、自锁开关 | 总供电和电源开关 | 充电、保护和升压电路以实物为准 |

> GPS 型号注意：BOM 写的是 `GY-NEO-6M`，但现有串口日志同时包含
> `$GNGGA`、`$GPGSV`、`$BDGSV`，说明实物更像是 GPS/北斗多模模块。
> 固件只要求输出标准 TTL NMEA，不能仅根据 BOM 型号判断。

## 4. GPIO 与接线

下表方向均以 ESP32 为视角：

| GPIO | 功能 | ESP32 方向 | 说明 |
| ---: | --- | --- | --- |
| 13 | 左侧振动马达 PWM | 输出 | 接 MOSFET 驱动 |
| 14 | 右侧振动马达 PWM | 输出 | 接 MOSFET 驱动 |
| 16 | 前超声波 TRIG | 输出 | HC-SR04 |
| 17 | 左超声波 TRIG | 输出 | HC-SR04 |
| 18 | 右超声波 TRIG | 输出 | HC-SR04 |
| 21 | BH1750 SDA | 双向 | I2C 数据 |
| 22 | BH1750 SCL | 输出 | I2C 时钟 |
| 23 | 按键信号 | 输入 | 内部上拉，低电平按下 |
| 25 | 补光灯 PWM | 输出 | 高电平开灯 |
| 26 | GPS UART RX | 输入 | 接 GPS TX |
| 27 | GPS UART TX | 输出 | 接 GPS RX，可留空 |
| 32 | DFPlayer UART RX | 输入 | 接 DFPlayer TX |
| 33 | DFPlayer UART TX | 输出 | 接 DFPlayer RX |
| 34 | 前超声波 ECHO | 输入 | 输入专用，需电平匹配 |
| 35 | 左超声波 ECHO | 输入 | 输入专用，需电平匹配 |
| 36 | 右超声波 ECHO | 输入 | 输入专用，需电平匹配 |
| 39 | 水浸模拟量 AO | 输入 | ADC1，输入专用 |

### 4.1 三路 HC-SR04

| 方向 | TRIG | ECHO | 供电 | 接地 |
| --- | ---: | ---: | --- | --- |
| 前方 `超声波_FRONT` | GPIO16 | GPIO34 | 5V | 共 GND |
| 左侧 `超声波_LEFT` | GPIO17 | GPIO35 | 5V | 共 GND |
| 右侧 `超声波_RIGHT` | GPIO18 | GPIO36 | 5V | 共 GND |

接线：

```text
HC-SR04 VCC   -> 5V
HC-SR04 GND   -> GND
HC-SR04 TRIG  -> 对应 ESP32 TRIG
HC-SR04 ECHO  -> 对应 ESP32 ECHO
```

GPIO34、GPIO35、GPIO36 是输入专用引脚，没有内部上下拉。HC-SR04 通常输出
5V ECHO，不能把 5V 直接送入 ESP32；需要使用分压电阻，目前 PCB 已集成可靠
分压电路。

### 4.2 BH1750

| BH1750 | ESP32 |
| --- | --- |
| VCC | 3.3V |
| GND | GND |
| SDA | GPIO21 |
| SCL | GPIO22 |
| ADDR | 按模块地址设计，悬空通常为 `0x23` |

固件会自动探测 `0x23` 和 `0x5C`。I2C 总线上只能保留一个 BH1750 地址。

### 4.3 水浸传感器

| 水浸模块 | ESP32 |
| --- | --- |
| VCC | 3.3V |
| GND | GND |
| AO/模拟输出 | GPIO39 |

代码使用 12 位 ADC 和 `ADC_11db` 衰减。GPIO39 是输入专用 ADC1 引脚，Wi-Fi
工作时仍可使用。判湿、判干原始值分别为 1800 和 1200，并具有 500 ms 去抖。

### 4.4 GPS/GNSS

| GPS 模块 | 板上 J3/ESP32 | 说明 |
| --- | --- | --- |
| VCC | J3 第 1 脚 | 5V |
| GPS RX | J3 第 2 脚 / GPIO27 | 非必接，GPS模块上电后自动向 ESP32 发送数据 |
| GPS TX | J3 第 3 脚 / GPIO26 | 必须接，GPS 发往 ESP32 |
| GND | J3 第 4 脚 | 必须与 ESP32 共地 |


模块必须输出 TTL UART NMEA，不能使用 RS232 电平。`$GNGGA`、`$GNRMC`、
`$GPGSV`、`$BDGSV` 都属于正常语句。

### 4.5 按键

| 按键模块 | ESP32 |
| --- | --- |
| GND | GND |
| OUT/SIGNAL | GPIO23 |

固件启用 `INPUT_PULLUP`，按键按下时读取低电平。短按切换补光灯并进入手动
模式；持续按下 5 秒触发 SOS。

### 4.6 DFPlayer Mini 和扬声器

| DFPlayer Mini | ESP32/外设 |
| --- | --- |
| VCC | 5V |
| GND | GND |
| RX | GPIO33（ESP32 发往 DFPlayer） |
| TX | GPIO32（DFPlayer 发往 ESP32） |
| SPK1/SPK2 | 扬声器两端 |

DFPlayer 使用 UART1、9600 8N1。模块自备 microSD 卡，将 `audio/01/` 整个目录放到 FAT32 卡根目录。

```text
DFPlayer TX -> ESP32 GPIO32
DFPlayer RX -> ESP32 GPIO33
```

### 4.7 左右振动马达

| 执行器 | ESP32 控制脚 | 要求 |
| --- | ---: | --- |
| 左侧振动马达驱动 | GPIO13 | PWM 输出，经过 MOSFET 管控制|
| 右侧振动马达驱动 | GPIO14 | PWM 输出，经过 MOSFET 管控制|

固件使用 1 kHz、8 位 PWM，高电平有效。马达不能直接连接 GPIO；马达电源应与
ESP32 共地，并使用适合马达电流的驱动和续流保护。

### 4.8 补光灯

| 补光灯驱动 | ESP32 |
| --- | --- |
| PWM/IN | GPIO25 |
| GND | GND |

GPIO25 高电平开灯、低电平关灯。固件在 80 lx 以下自动开灯，在 150 lx 以上
自动关灯。短按按键或小程序云端命令会切换到手动模式。

### 4.9 供电

BOM 记录使用 5V 2000 mAh 可充电锂电池和自锁电源开关。接线时必须保证：

- ESP32、传感器、DFPlayer 和驱动模块共地。
- 3.3V 传感器不要接 5V 电源脚

## 5. 软件功能与参数

| 功能 | 当前规则 |
| --- | --- |
| 超声波采样 | 三个方向轮询，每路间隔 60 ms |
| 振动预警 | 任一路 ≤120 cm，距离越近 PWM 越强 |
| 语音预警 | 对应方向 ≤25 cm，有 3 秒冷却 |
| 水浸 | 500 ms 去抖，判湿值 1800，判干值 1200 |
| 光照控制 | <80 lx 开灯，>150 lx 关灯 |
| SOS | 按键持续 3 秒，播放 006 并发送 GPS |
| GPS 定位有效期 | 20 秒 |
| 云端遥测 | 每 5 秒发布一次 |
| 音量 | DFPlayer 0～30，可通过小程序滑块调整 |

主循环每秒输出一次状态：

```text
[12000] F:23 L:84 R:-1 Lux:52.3 Water:0 Lamp:1(auto) MOS:HIGH GPS:fix/7 MQTT:1
```

## 6. 巴法云 MQTT

ESP32 使用私钥作为 MQTT Client ID，服务器为 `bemfa.com:9501`。

| 主题 | 方向 | 内容 |
| --- | --- | --- |
| `bz004` | ESP32 → 云/小程序 | 三路测距和最近障碍 |
| `bs004` | ESP32 → 云/小程序 | 水浸状态和 ADC 原始值 |
| `yd004` | ESP32 → 云/小程序 | 照度、补光灯模式、当前音量 |
| `dw004` | ESP32 → 云/小程序 | GPS 经纬度、卫星数和定位状态 |
| `sos006` | ESP32 → 云/小程序 | SOS 求救事件 |
| `lamp002` | 小程序 → ESP32 | `on/off` 控灯，`vol:0`～`vol:30` 调音量 |


## 7. 配置与编译

1. 将 `include/secrets.example.h` 复制为 `include/secrets.h`。
2. 填写 2.4 GHz Wi-Fi、巴法云私钥。
3. 用 VS Code 打开 `ESP32-DaoMangZhang` 文件夹。
4. 等待 PlatformIO 安装依赖，执行 Build/Upload。
5. 串口监视器使用 115200。

`platformio.ini` 当前依赖：

```ini
lib_deps =
    knolleary/PubSubClient
    mikalhart/TinyGPSPlus
```

不要把真实的 `include/secrets.h` 上传到公开仓库。

## 8. 故障排查

### GPS

- `GPS: no UART bytes`：优先检查 GPS TX 是否接 GPIO26、是否共地、模块是否供电。
- `GPS: ... no valid NMEA`：收到字节但无法识别，检查串口电平和接线。
- `GPS baud detected`：波特率识别成功。
- `GPS fix`：定位和解析成功。
- 室内、天线未接或遮蔽严重时可能长期无法定位，需要室外或靠近室外位置使用

### DFPlayer

- 没有声音：检查 5V 供电、共地、TX/RX 交叉、microSD 的 FAT32 格式和文件目录。
- 音量过小：在小程序“数据”页将音量调到 0～30；若仍小，检查扬声器功率和供电。

### 补光灯

- 始终亮或始终灭：检查 GPIO25 到 MOSFET 的信号、MOSFET 供电和灯带公共地。
- 自动模式不工作：查看串口是否提示 BH1750 未找到，检查 GPIO21/22 和模块地址。

### 超声波

- 某一路始终为 `-1`：检查该路 TRIG/ECHO、5V 供电和 ECHO 分压。
- 全部无回波：检查公共地或传感器朝向。

## 9. 相关文件

- 烧录与串口操作：`烧录说明.md`
- 语音文件编号：`audio/README.md`
- 引脚和阈值：`include/config.h`
- 本地敏感配置：`include/secrets.h`
- PCB 源文件：`../PCB设计/`
