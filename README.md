# 温控风扇自启动系统 — 一主一从 LoRa 无线温控

基于 STM32F103C8T6 + LoRa SX1278 的无线温控系统，用于设备房温湿度监控和风扇远程控制。

## 目录结构

```
temp-control-system/
├── common/                  # 公共模块 (两个工程共用)
│   ├── inc/
│   │   ├── protocol.h       # 通信帧协议
│   │   ├── sx1278.h         # SX1278 LoRa 驱动头文件
│   │   └── sx1278_regs.h    # SX1278 寄存器定义
│   └── src/
│       ├── protocol.c       # 通信帧协议实现
│       └── sx1278.c         # SX1278 LoRa 驱动实现
├── slave/                   # 设备房从机工程
│   ├── inc/
│   │   ├── ds18b20.h        # DS18B20 温度传感器
│   │   ├── dht22.h          # DHT22 温湿度传感器
│   │   ├── relay.h          # 继电器控制
│   │   └── app_slave.h      # 从机应用层
│   └── src/
│       ├── main.c           # 从机主入口
│       ├── ds18b20.c        # DS18B20 驱动实现
│       ├── dht22.c          # DHT22 驱动实现
│       ├── relay.c          # 继电器驱动实现
│       └── app_slave.c      # 从机应用逻辑
├── master/                  # 值班室主机工程
│   ├── inc/
│   │   ├── hub12.h          # HUB12 点阵屏驱动 (P10 单色屏)
│   │   ├── key.h            # 按键驱动
│   │   ├── font.h           # 5x7 ASCII 字体
│   │   └── app_master.h     # 主机应用层
│   └── src/
│       ├── main.c           # 主机主入口
│       ├── hub12.c          # HUB12 驱动实现
│       ├── key.c            # 按键驱动实现
│       └── app_master.c     # 主机应用逻辑
├── README.md                # 项目说明
├── 项目技术文档.md           # 详细技术文档
└── 调试与测试指南.md         # 调试与测试流程
```

## 硬件配置

### 从机 (设备房)
| 引脚 | 连接 | 说明 |
|------|------|------|
| PA0  | DS18B20 DQ | 单总线 + 4.7kΩ 上拉 |
| PA1  | DHT22 DQ | 单总线 + 10kΩ 上拉 |
| PA4~PA7 | LoRa SPI1 | NSS/SCK/MISO/MOSI |
| PB0  | LoRa DIO0 | 中断输入 |
| PB1  | 继电器 IN | 高电平触发 |

### 主机 (值班室)
| 引脚 | 连接 | 说明 |
|------|------|------|
| PA4~PA7 | LoRa SPI1 | 与从机相同 |
| PB0  | LoRa DIO0 | 中断输入 |
| PB3~PB8 | HUB12 点阵屏 | A/B/CLK/STB/OE/R1 |
| PC13 | KEY1 | 手动开/关风扇 |
| PA8  | KEY2 | 切换自动/手动 |
| PB11 | KEY3 | 阈值 +1°C |
| PB10 | KEY4 | 阈值 -1°C |

## 通信协议

帧格式: `[HEAD][SRC][DST][TYPE][LEN][DATA...][XOR][TAIL]`

| 字段 | 长度 | 说明 |
|------|------|------|
| HEAD | 1B | 0xAA |
| SRC  | 1B | 源设备 ID |
| DST  | 1B | 目标设备 ID |
| TYPE | 1B | 指令类型 |
| LEN  | 1B | 数据长度 |
| DATA | 0~16B | 数据内容 |
| XOR  | 1B | 异或校验 |
| TAIL | 1B | 0x55 |

### 指令类型

| 类型 | 值 | 说明 |
|------|----|------|
| CMD_FAN_CONTROL | 0x01 | 风扇控制 (0x01=开, 0x00=关) |
| CMD_SET_THRESHOLD | 0x02 | 设置温度阈值 (20~50°C) |
| CMD_SET_MODE | 0x03 | 设置模式 (0x01=自动, 0x00=手动) |
| RPT_SENSOR_DATA | 0x11 | 从机上报数据 |

## 开发环境

- **IDE**: Keil MDK-ARM v5 或 STM32CubeIDE
- **芯片**: STM32F103C8T6 (72MHz, 64KB Flash, 20KB RAM)
- **外设库**: STM32F10x_StdPeriph_Lib (标准外设库)
- **晶振**: 需要 8MHz 外部晶振 (HSE)

## 编译说明

### Keil MDK 工程配置

1. **从机工程**:
   - 添加 `common/src/` + `slave/src/` 所有 .c 文件
   - Include 路径: `common/inc` + `slave/inc`
   - 宏定义: `STM32F10X_MD`, `USE_STDPERIPH_DRIVER`

2. **主机工程**:
   - 添加 `common/src/` + `master/src/` 所有 .c 文件
   - Include 路径: `common/inc` + `master/inc`
   - 宏定义: `STM32F10X_MD`, `USE_STDPERIPH_DRIVER`

### 烧录

使用 ST-Link V2，通过 SWD 接口烧录。从机和主机程序分别编译后烧录到各自的 STM32 最小系统板。

## 注意事项

1. **天线必须连接** — SX1278 未接天线时高功率发射可能损坏射频前端
2. **上拉电阻** — DS18B20 需 4.7kΩ，DHT22 需 10kΩ 上拉
3. **220V 安全** — 继电器接线必须使用带绝缘外壳的模块，由专业人员操作
4. **供电** — 每个节点使用 5V/2A DC 电源适配器独立供电
