/**
 * @file    dht22.c
 * @brief   DHT22 (AM2302) 温湿度传感器驱动实现
 */

#include "dht22.h"

static void delay_us(uint32_t us)
{
    us *= 72;
    while (us--) __NOP();
}

static void DHT22_SetOutput(void)
{
    GPIO_InitTypeDef gpio;
    gpio.GPIO_Pin = DHT22_PIN;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(DHT22_PORT, &gpio);
}

static void DHT22_SetInput(void)
{
    GPIO_InitTypeDef gpio;
    gpio.GPIO_Pin = DHT22_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IPU;  /* 外部 10kΩ 上拉 */
    GPIO_Init(DHT22_PORT, &gpio);
}

static uint8_t DHT22_ReadBit(void)
{
    uint32_t timeout;

    /* 等待低电平结束 (50μs) */
    timeout = 100;
    while (!GPIO_ReadInputDataBit(DHT22_PORT, DHT22_PIN) && timeout--) {
        delay_us(1);
    }

    delay_us(30);  /* 延时 30μs 后采样 */

    uint8_t bit = GPIO_ReadInputDataBit(DHT22_PORT, DHT22_PIN);

    /* 等待高电平结束 */
    timeout = 100;
    while (GPIO_ReadInputDataBit(DHT22_PORT, DHT22_PIN) && timeout--) {
        delay_us(1);
    }

    return bit;
}

void DHT22_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    DHT22_SetInput();
}

uint8_t DHT22_Read(DHT22_Data_t *data)
{
    uint8_t buf[5];
    uint32_t timeout;

    data->valid = 0;

    /* 起始信号: 拉低至少 1ms */
    DHT22_SetOutput();
    GPIO_ResetBits(DHT22_PORT, DHT22_PIN);
    delay_us(1500);  /* 拉低 1.5ms */
    GPIO_SetBits(DHT22_PORT, DHT22_PIN);
    delay_us(30);    /* 拉高 20~40μs */
    DHT22_SetInput();

    /* 检测响应: DHT22 拉低 80μs + 拉高 80μs */
    timeout = 100;
    while (GPIO_ReadInputDataBit(DHT22_PORT, DHT22_PIN) && timeout--) {
        delay_us(1);
    }
    if (timeout == 0) return 0;  /* 无响应 */

    /* 等待低电平结束 */
    timeout = 100;
    while (!GPIO_ReadInputDataBit(DHT22_PORT, DHT22_PIN) && timeout--) {
        delay_us(1);
    }

    /* 等待高电平结束 */
    timeout = 100;
    while (GPIO_ReadInputDataBit(DHT22_PORT, DHT22_PIN) && timeout--) {
        delay_us(1);
    }

    /* 读取 40 bit 数据 */
    for (uint8_t i = 0; i < 5; i++) {
        buf[i] = 0;
        for (uint8_t j = 0; j < 8; j++) {
            buf[i] <<= 1;
            buf[i] |= DHT22_ReadBit();
        }
    }

    /* 校验 */
    uint8_t checksum = buf[0] + buf[1] + buf[2] + buf[3];
    if (checksum != buf[4]) {
        return 0;
    }

    /* 解析湿度 (buf[0..1]) */
    uint16_t raw_humid = ((uint16_t)buf[0] << 8) | buf[1];
    data->humidity = (float)raw_humid / 10.0f;

    /* 解析温度 (buf[2..3], bit15 为符号位) */
    uint16_t raw_temp = ((uint16_t)buf[2] << 8) | buf[3];
    if (raw_temp & 0x8000) {
        raw_temp &= 0x7FFF;
        data->temperature = -(float)raw_temp / 10.0f;
    } else {
        data->temperature = (float)raw_temp / 10.0f;
    }

    data->valid = 1;
    return 1;
}
