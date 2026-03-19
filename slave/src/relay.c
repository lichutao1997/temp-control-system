/**
 * @file    relay.c
 * @brief   继电器控制驱动实现
 */

#include "relay.h"

static uint8_t _relay_state = 0;

void Relay_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitTypeDef gpio;
    gpio.GPIO_Pin = RELAY_PIN;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(RELAY_PORT, &gpio);

    Relay_Off();  /* 默认关闭 */
}

void Relay_On(void)
{
    GPIO_SetBits(RELAY_PORT, RELAY_PIN);
    _relay_state = 1;
}

void Relay_Off(void)
{
    GPIO_ResetBits(RELAY_PORT, RELAY_PIN);
    _relay_state = 0;
}

uint8_t Relay_GetStatus(void)
{
    return _relay_state;
}
