/**
 * @file    ds18b20.h
 * @brief   DS18B20 单总线温度传感器驱动
 *
 * 硬件接线:
 *   PA0  → DS18B20 DQ (数据线)
 *   3.3V → DS18B20 VCC
 *   GND  → DS18B20 GND
 *   PA0 与 3.3V 之间接 4.7kΩ 上拉电阻
 *
 * 传感器规格:
 *   测量范围: -55°C ~ +125°C
 *   精度: ±0.5°C (-10°C ~ +85°C)
 *   分辨率: 9~12 位可选 (本驱动默认 12 位, 0.0625°C)
 *   转换时间: 12 位精度约 750ms
 */

#ifndef DS18B20_H
#define DS18B20_H

#include "stm32f10x.h"

/* ======================== 引脚定义 ======================== */
#define DS18B20_PORT    GPIOA       /**< DQ 引脚所在端口 */
#define DS18B20_PIN     GPIO_Pin_0  /**< DQ 引脚号 */

/* ======================== 公共函数 ======================== */

/**
 * @brief  初始化 DS18B20 — 配置 GPIO 并检测传感器存在
 * @note   调用前需确保 4.7kΩ 上拉电阻已接好
 */
void DS18B20_Init(void);

/**
 * @brief  启动温度转换 (非阻塞)
 *
 * 发送 Skip ROM (0xCC) + Convert T (0x44) 命令,
 * DS18B20 在后台执行温度转换, 约 750ms 后完成。
 * 之后调用 DS18B20_ReadTemp() 读取结果。
 */
void DS18B20_StartConvert(void);

/**
 * @brief  读取温度值
 *
 * 发送 Skip ROM (0xCC) + Read Scratchpad (0xBE),
 * 然后读取 2 字节温度数据并转换为 °C。
 *
 * @retval 温度值 (°C), 精度 0.0625°C
 * @retval -100.0f — 传感器无响应 (检查接线)
 *
 * @note   必须在 StartConvert 后等待至少 750ms 再调用
 */
float DS18B20_ReadTemp(void);

/**
 * @brief  快速读取温度 (阻塞方式)
 *
 * 内部依次调用: StartConvert → 等待 750ms → ReadTemp
 * 适合简单场景, 但会阻塞主循环约 750ms。
 *
 * @retval 温度值 (°C)
 */
float DS18B20_ReadTempBlocking(void);

#endif /* DS18B20_H */
