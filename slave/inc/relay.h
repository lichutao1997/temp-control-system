/**
 * @file    relay.h
 * @brief   单路继电器控制驱动
 *
 * 硬件接线:
 *   PB1  → 继电器模块 IN 引脚
 *   5V   → 继电器模块 VCC (线圈供电)
 *   GND  → 继电器模块 GND
 *   COM  → 市电火线
 *   NO   → 风扇插座火线端
 *
 * 控制逻辑:
 *   PB1 = HIGH → 继电器闭合 (COM 与 NO 接通) → 风扇通电
 *   PB1 = LOW  → 继电器断开 (COM 与 NO 断开) → 风扇断电
 *
 * 安全警告:
 *   ⚠️ 220V 强电部分必须由专业电工接线
 *   ⚠️ 建议使用带光耦隔离的继电器模块
 */

#ifndef RELAY_H
#define RELAY_H

#include "stm32f10x.h"

/* ======================== 引脚定义 ======================== */
#define RELAY_PORT  GPIOB       /**< 继电器控制引脚所在端口 */
#define RELAY_PIN   GPIO_Pin_1  /**< 继电器控制引脚号 */

/* ======================== 公共函数 ======================== */

/**
 * @brief  初始化继电器 GPIO — 默认关闭 (安全状态)
 */
void Relay_Init(void);

/**
 * @brief  打开继电器 — PB1 输出高电平, 风扇通电
 */
void Relay_On(void);

/**
 * @brief  关闭继电器 — PB1 输出低电平, 风扇断电
 */
void Relay_Off(void);

/**
 * @brief  获取继电器当前状态
 * @retval 1=打开, 0=关闭
 */
uint8_t Relay_GetStatus(void);

#endif /* RELAY_H */
