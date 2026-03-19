/**
 * @file    ds18b20.h
 * @brief   DS18B20 单总线温度传感器驱动
 */

#ifndef DS18B20_H
#define DS18B20_H

#include "stm32f10x.h"

/* 引脚定义: PA0 */
#define DS18B20_PORT    GPIOA
#define DS18B20_PIN     GPIO_Pin_0

/**
 * @brief  初始化 DS18B20 GPIO
 */
void DS18B20_Init(void);

/**
 * @brief  启动温度转换 (非阻塞)
 */
void DS18B20_StartConvert(void);

/**
 * @brief  读取温度值
 * @retval 温度值 (°C), 精度 0.0625°C。读取失败返回 -100.0
 * @note   需在 StartConvert 后等待至少 750ms 再调用
 */
float DS18B20_ReadTemp(void);

/**
 * @brief  快速读取: 启动转换+等待+读取 (阻塞约 750ms)
 * @retval 温度值 (°C)
 */
float DS18B20_ReadTempBlocking(void);

#endif /* DS18B20_H */
