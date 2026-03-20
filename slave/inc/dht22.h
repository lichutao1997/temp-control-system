/**
 * @file    dht22.h
 * @brief   DHT22 (AM2302) 温湿度传感器驱动
 *
 * 硬件接线:
 *   PA1  → DHT22 DATA
 *   3.3V → DHT22 VCC
 *   GND  → DHT22 GND
 *   PA1 与 3.3V 之间接 10kΩ 上拉电阻
 *
 * 传感器规格:
 *   温度范围: -40°C ~ +80°C, 精度 ±0.5°C
 *   湿度范围: 0~99.9% RH, 精度 ±2% RH
 *   采样间隔: ≥ 2 秒
 *   供电: 3.3~5.5V
 */

#ifndef DHT22_H
#define DHT22_H

#include "stm32f10x.h"

/* ======================== 引脚定义 ======================== */
#define DHT22_PORT  GPIOA       /**< DATA 引脚所在端口 */
#define DHT22_PIN   GPIO_Pin_1  /**< DATA 引脚号 */

/* ======================== 数据结构 ======================== */

/**
 * @brief DHT22 读取结果
 */
typedef struct {
    float   temperature;    /**< 温度 (°C), 支持负温度 */
    float   humidity;       /**< 湿度 (%RH) */
    uint8_t valid;          /**< 数据有效性: 1=有效, 0=读取失败 */
} DHT22_Data_t;

/* ======================== 公共函数 ======================== */

/**
 * @brief  初始化 DHT22 — 配置 GPIO 为输入 (外部 10kΩ 上拉)
 */
void DHT22_Init(void);

/**
 * @brief  读取温湿度数据
 *
 * 完整读取流程:
 *   1. 主机发送起始信号 (拉低 1.5ms)
 *   2. 检测 DHT22 响应 (80μs 低 + 80μs 高)
 *   3. 接收 40 bit 数据
 *   4. 校验 (前 4 字节之和 = 第 5 字节)
 *
 * @param  data: 输出数据结构体
 * @retval 1=读取成功, 0=失败 (无响应或校验错误)
 *
 * @note   两次调用间隔至少 2 秒
 */
uint8_t DHT22_Read(DHT22_Data_t *data);

#endif /* DHT22_H */
