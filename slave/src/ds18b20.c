/**
 * @file    ds18b20.c
 * @brief   DS18B20 单总线温度传感器驱动实现
 */

#include "ds18b20.h"

/* 微秒级延时 (基于 72MHz 主频, 粗略) */
static void delay_us(uint32_t us)
{
    us *= 72;
    while (us--) {
        __NOP();
    }
}

static void delay_ms(uint32_t ms)
{
    while (ms--) {
        delay_us(1000);
    }
}

/* 单总线引脚操作 */
static void DS18B20_SetOutput(void)
{
    GPIO_InitTypeDef gpio;
    gpio.GPIO_Pin = DS18B20_PIN;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(DS18B20_PORT, &gpio);
}

static void DS18B20_SetInput(void)
{
    GPIO_InitTypeDef gpio;
    gpio.GPIO_Pin = DS18B20_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IPU;  /* 外部已有 4.7kΩ 上拉 */
    GPIO_Init(DS18B20_PORT, &gpio);
}

static void DS18B20_DQ_Low(void)  { GPIO_ResetBits(DS18B20_PORT, DS18B20_PIN); }
static void DS18B20_DQ_High(void) { GPIO_SetBits(DS18B20_PORT, DS18B20_PIN); }
static uint8_t DS18B20_DQ_Read(void) { return GPIO_ReadInputDataBit(DS18B20_PORT, DS18B20_PIN); }

/* 复位并检测存在脉冲 */
static uint8_t DS18B20_Reset(void)
{
    uint8_t presence;

    DS18B20_SetOutput();
    DS18B20_DQ_Low();
    delay_us(480);     /* 拉低 480μs */
    DS18B20_SetInput();
    delay_us(60);      /* 等待 60μs */
    presence = !DS18B20_DQ_Read();  /* 检测存在脉冲 */
    delay_us(420);     /* 等待复位完成 */

    return presence;
}

/* 写一个字节 (LSB first) */
static void DS18B20_WriteByte(uint8_t byte)
{
    for (uint8_t i = 0; i < 8; i++) {
        DS18B20_SetOutput();
        if (byte & 0x01) {
            /* 写 1: 拉低 1~15μs, 释放 */
            DS18B20_DQ_Low();
            delay_us(2);
            DS18B20_DQ_High();
            delay_us(60);
        } else {
            /* 写 0: 拉低 60~120μs */
            DS18B20_DQ_Low();
            delay_us(60);
            DS18B20_DQ_High();
            delay_us(2);
        }
        byte >>= 1;
    }
}

/* 读一个字节 (LSB first) */
static uint8_t DS18B20_ReadByte(void)
{
    uint8_t byte = 0;

    for (uint8_t i = 0; i < 8; i++) {
        byte >>= 1;
        DS18B20_SetOutput();
        DS18B20_DQ_Low();
        delay_us(2);
        DS18B20_SetInput();
        delay_us(10);
        if (DS18B20_DQ_Read()) {
            byte |= 0x80;
        }
        delay_us(50);
    }
    return byte;
}

void DS18B20_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    DS18B20_Reset();
}

void DS18B20_StartConvert(void)
{
    DS18B20_Reset();
    DS18B20_WriteByte(0xCC);  /* Skip ROM */
    DS18B20_WriteByte(0x44);  /* Convert T */
}

float DS18B20_ReadTemp(void)
{
    if (!DS18B20_Reset()) {
        return -100.0f;  /* 传感器无响应 */
    }

    DS18B20_WriteByte(0xCC);  /* Skip ROM */
    DS18B20_WriteByte(0xBE);  /* Read Scratchpad */

    uint8_t lsb = DS18B20_ReadByte();
    uint8_t msb = DS18B20_ReadByte();

    int16_t raw = (int16_t)((msb << 8) | lsb);
    return (float)raw / 16.0f;
}

float DS18B20_ReadTempBlocking(void)
{
    DS18B20_StartConvert();
    delay_ms(750);  /* 12位精度需 750ms */
    return DS18B20_ReadTemp();
}
