/**
 * @file    key.c
 * @brief   按键驱动实现 (4 个按键, 消抖处理)
 *
 * ===================== 按键硬件说明 =====================
 *
 * 4 个轻触按键, 一端接 GPIO, 另一端接 GND。
 * GPIO 配置为内部上拉输入, 按下时读到低电平。
 *
 * 按键分配:
 *   KEY1 (PC13): 手动开/关风扇
 *   KEY2 (PA8):  切换自动/手动模式
 *   KEY3 (PB11): 温度阈值 +1°C
 *   KEY4 (PB10): 温度阈值 -1°C
 *
 * 消抖算法:
 *   每次检测到电平变化时, 记录时间戳。
 *   只有当电平稳定超过 KEY_DEBOUNCE_MS (20ms) 后才确认状态。
 *   检测按下事件 (从释放到按下的跳变), 不检测重复触发。
 *
 * 注意:
 *   PC13 在 STM32F103 上没有内部上拉电阻, 建议外加 10kΩ 上拉。
 *   PA8、PB10、PB11 有内部上拉, 可直接使用。
 */

#include "key.h"

/* ======================== 按键引脚表 ======================== */

typedef struct {
    GPIO_TypeDef *port;     /**< GPIO 端口 */
    uint16_t      pin;      /**< GPIO 引脚 */
} KeyPin_t;

/**
 * @brief 按键引脚映射表 — 索引对应 KEY_1~KEY_4
 */
static const KeyPin_t key_pins[] = {
    {KEY1_PORT, KEY1_PIN},   /* KEY_1 = 0: PC13 */
    {KEY2_PORT, KEY2_PIN},   /* KEY_2 = 1: PA8  */
    {KEY3_PORT, KEY3_PIN},   /* KEY_3 = 2: PB11 */
    {KEY4_PORT, KEY4_PIN},   /* KEY_4 = 3: PB10 */
};

/** @brief 按键数量 */
#define KEY_COUNT   (sizeof(key_pins) / sizeof(key_pins[0]))

/* ======================== 消抖状态 ======================== */

/** @brief 按键当前确认状态: 1=按下, 0=释放 */
static uint8_t _key_state[4] = {0};

/** @brief 按键上一次采样状态 (用于检测变化) */
static uint8_t _key_prev[4] = {0};

/** @brief 按键状态变化时的时间戳 (用于消抖计时) */
static uint32_t _key_debounce[4] = {0};

/* ======================== 辅助函数 ======================== */

/**
 * @brief  复用 app_master 中的 SysTick 计数器
 * @note   app_master.c 中定义了 g_systick_ms, 由 SysTick_Handler 递增
 */
extern volatile uint32_t g_systick_ms;

static uint32_t GetTick(void)
{
    return g_systick_ms;
}

/**
 * @brief  读取单个按键的原始电平
 * @param  idx: 按键索引 (0~3)
 * @retval 1=按下(低电平), 0=释放(高电平)
 *
 * GPIO 内部上拉, 按键另一端接 GND:
 *   未按下: GPIO 读到高电平 (上拉)
 *   按下:   GPIO 读到低电平 (接地)
 */
static uint8_t Key_ReadRaw(uint8_t idx)
{
    return (GPIO_ReadInputDataBit(key_pins[idx].port, key_pins[idx].pin) == Bit_RESET) ? 1 : 0;
}

/* ======================== 公共函数 ======================== */

void Key_Init(void)
{
    GPIO_InitTypeDef gpio;

    /* 开启 GPIOA/GPIOB/GPIOC 时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA |
                           RCC_APB2Periph_GPIOB |
                           RCC_APB2Periph_GPIOC, ENABLE);

    /* 所有按键: 内部上拉输入, 低电平有效 */
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;  /* 输入模式, 速率不重要 */

    for (uint8_t i = 0; i < KEY_COUNT; i++) {
        gpio.GPIO_Pin = key_pins[i].pin;
        GPIO_Init(key_pins[i].port, &gpio);
    }
}

uint8_t Key_Scan(void)
{
    /* 简单扫描 — 返回第一个按下的按键 */
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

        /**
         * 消抖逻辑:
         *   1. 检测到电平与上一次不同 → 记录时间戳, 更新采样值
         *   2. 如果电平与上次相同 → 检查是否已稳定超过消抖时间
         *   3. 稳定后: 如果从释放变为按下 → 返回按键事件
         */
        if (raw != _key_prev[i]) {
            _key_debounce[i] = now;    /* 重新计时 */
            _key_prev[i] = raw;        /* 更新采样 */
        }

        if ((now - _key_debounce[i]) > KEY_DEBOUNCE_MS) {
            /* 电平已稳定超过消抖时间 */
            if (raw && !_key_state[i]) {
                /* 按下事件: 之前释放, 现在按下 */
                _key_state[i] = 1;
                return i;
            } else if (!raw && _key_state[i]) {
                /* 释放事件: 之前按下, 现在释放 (仅更新状态, 不返回事件) */
                _key_state[i] = 0;
            }
        }
    }

    return KEY_NONE;  /* 无按键事件 */
}
