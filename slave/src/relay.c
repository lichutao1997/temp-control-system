/**
 * @file    relay.c
 * @brief   继电器控制驱动实现
 *
 * 继电器模块接线:
 *   PB1 → 继电器 IN 引脚 (高电平触发)
 *   继电器 COM → 市电火线
 *   继电器 NO  → 风扇插座火线
 *   (零线和地线直连风扇插座, 不经过继电器)
 *
 * 注意事项:
 *   - 3.3V GPIO 直接驱动 5V 继电器模块可能不可靠
 *   - 建议加 NPN 三极管驱动电路 (见项目技术文档 4.5 节)
 *   - 220V 接线必须由专业电工操作
 */

#include "relay.h"

/** @brief 继电器当前状态记录 */
static uint8_t _relay_state = 0;

void Relay_Init(void)
{
    /* 开启 GPIOB 时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    /* PB1 配置为推挽输出 */
    GPIO_InitTypeDef gpio;
    gpio.GPIO_Pin = RELAY_PIN;       /* PB1 */
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(RELAY_PORT, &gpio);

    /* 默认关闭继电器 (安全状态) */
    Relay_Off();
}

void Relay_On(void)
{
    GPIO_SetBits(RELAY_PORT, RELAY_PIN);  /* PB1 = HIGH → 继电器闭合 */
    _relay_state = 1;
}

void Relay_Off(void)
{
    GPIO_ResetBits(RELAY_PORT, RELAY_PIN);  /* PB1 = LOW → 继电器断开 */
    _relay_state = 0;
}

uint8_t Relay_GetStatus(void)
{
    return _relay_state;
}
