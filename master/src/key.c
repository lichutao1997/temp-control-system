/**
 * @file    key.c
 * @brief   按键驱动实现
 */

#include "key.h"

/* 按键引脚表 */
typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
} KeyPin_t;

static const KeyPin_t key_pins[] = {
    {KEY1_PORT, KEY1_PIN},
    {KEY2_PORT, KEY2_PIN},
    {KEY3_PORT, KEY3_PIN},
    {KEY4_PORT, KEY4_PIN},
};
#define KEY_COUNT   (sizeof(key_pins) / sizeof(key_pins[0]))

static uint8_t _key_state[4] = {0};
static uint8_t _key_prev[4] = {0};
static uint32_t _key_debounce[4] = {0};
static uint32_t _systick_ms = 0;

/* SysTick 已在 app_master 中配置, 这里复用 */
extern volatile uint32_t g_systick_ms;

static uint32_t GetTick(void)
{
    return g_systick_ms;
}

void Key_Init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA |
                           RCC_APB2Periph_GPIOB |
                           RCC_APB2Periph_GPIOC, ENABLE);

    /* 所有按键: Input Pull-Up, 低电平有效 */
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;

    for (uint8_t i = 0; i < KEY_COUNT; i++) {
        gpio.GPIO_Pin = key_pins[i].pin;
        GPIO_Init(key_pins[i].port, &gpio);
    }
}

static uint8_t Key_ReadRaw(uint8_t idx)
{
    return (GPIO_ReadInputDataBit(key_pins[idx].port, key_pins[idx].pin) == Bit_RESET) ? 1 : 0;
}

uint8_t Key_Scan(void)
{
    for (uint8_t i = 0; i < KEY_COUNT; i++) {
        if (Key_ReadRaw(i)) {
            return i;
        }
    }
    return KEY_NONE;
}

uint8_t Key_GetEvent(void)
{
    uint32_t now = GetTick();

    for (uint8_t i = 0; i < KEY_COUNT; i++) {
        uint8_t raw = Key_ReadRaw(i);

        if (raw != _key_prev[i]) {
            _key_debounce[i] = now;
            _key_prev[i] = raw;
        }

        if ((now - _key_debounce[i]) > KEY_DEBOUNCE_MS) {
            if (raw && !_key_state[i]) {
                /* 按下事件 */
                _key_state[i] = 1;
                return i;
            } else if (!raw && _key_state[i]) {
                /* 释放事件 */
                _key_state[i] = 0;
            }
        }
    }

    return KEY_NONE;
}
