/**
 * @file    dht22.h
 * @brief   DHT22 (AM2302) 温湿度传感器驱动
 */

#ifndef DHT22_H
#define DHT22_H

#include "stm32f10x.h"

/* 引脚定义: PA1 */
#define DHT22_PORT  GPIOA
#define DHT22_PIN   GPIO_Pin_1

typedef struct {
    float temperature;  /* °C */
    float humidity;     /* %RH */
    uint8_t valid;      /* 1=数据有效, 0=读取失败 */
} DHT22_Data_t;

/**
 * @brief  初始化 DHT22 GPIO
 */
void DHT22_Init(void);

/**
 * @brief  读取温湿度数据
 * @param  data: 输出数据结构体
 * @retval 1=成功, 0=失败
 */
uint8_t DHT22_Read(DHT22_Data_t *data);

#endif /* DHT22_H */
