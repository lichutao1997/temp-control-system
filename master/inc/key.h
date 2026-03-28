/**
 * @file    key.h
 * @brief   5 路按键扫描驱动 (内部上拉, 低电平有效)
 *
 * 按键硬件:
 *   6×6 轻触开关, 一端接 GPIO, 另一端接 GND。
 *   GPIO 配置为内部上拉输入, 按下时读到低电平。
 *
 * 按键功能分配:
 *   KEY1 (PC13): 手动开/关风扇 — 先切到手动模式, 再翻转风扇
 *   KEY2 (PA8):  切换自动/手动模式
 *   KEY3 (PB11): 温度阈值 +1°C (上限 50°C)
 *   KEY4 (PB10): 温度阈值 -1°C (下限 20°C)
 *   KEY5 (PB9):  烟感功能开关 — 切换烟感检测开启/关闭
 *
 * 注意: PC13 在 STM32F103C8T6 上没有内部上拉,
 *       建议外加 10kΩ 上拉电阻到 3.3V。
 *
 * v3.0 更新: 增加 KEY5 用于控制烟感功能开关
 */

#ifndef KEY_H
#define KEY_H

#include "stm32f10x.h"

/* ======================== 按键引脚定义 ======================== */

#define KEY1_PORT   GPIOC       /**< KEY1 端口 */
#define KEY1_PIN    GPIO_Pin_13 /**< KEY1 引脚 — 手动开/关风扇 */
#define KEY2_PORT   GPIOA       /**< KEY2 端口 */
#define KEY2_PIN    GPIO_Pin_8  /**< KEY2 引脚 — 切换自动/手动 */
#define KEY3_PORT   GPIOB       /**< KEY3 端口 */
#define KEY3_PIN    GPIO_Pin_11 /**< KEY3 引脚 — 阈值 +1°C */
#define KEY4_PORT   GPIOB       /**< KEY4 端口 */
#define KEY4_PIN    GPIO_Pin_10 /**< KEY4 引脚 — 阈值 -1°C */
#define KEY5_PORT   GPIOB       /**< KEY5 端口 */
#define KEY5_PIN    GPIO_Pin_9  /**< KEY5 引脚 — 烟感开关 */

/* ======================== 按键编号 ======================== */

#define KEY_1       0   /**< KEY1 编号 */
#define KEY_2       1   /**< KEY2 编号 */
#define KEY_3       2   /**< KEY3 编号 */
#define KEY_4       3   /**< KEY4 编号 */
#define KEY_5       4   /**< KEY5 编号 */
#define KEY_NONE    0xFF /**< 无按键 */

/* ======================== 时间参数 ======================== */

/** @brief 消抖时间 (ms) — 电平需稳定超过此时间才确认状态 */
#define KEY_DEBOUNCE_MS     20

/** @brief 长按时间 (ms) — 预留, 当前未使用 */
#define KEY_LONG_PRESS_MS   1000

/* ======================== 公共函数 ======================== */

/**
 * @brief  初始化所有按键 GPIO — 内部上拉输入
 *
 * 配置 PC13, PA8, PB11, PB10, PB9 为输入上拉模式。
 *
 * v3.0 更新: 增加 KEY5 (PB9)
 */
void Key_Init(void);

/**
 * @brief  快速扫描按键 — 返回当前按下的按键编号
 * @retval KEY_1~KEY_5, 无按键返回 KEY_NONE
 * @note   不带消抖, 适合需要快速响应的场景
 */
uint8_t Key_Scan(void);

/**
 * @brief  检测按键按下事件 (带消抖)
 *
 * 消抖逻辑:
 *   - 每 20ms (由主循环控制调用频率) 调用一次
 *   - 电平变化时记录时间, 稳定 20ms 后确认
 *   - 只返回按下事件 (一次按键只返回一次)
 *   - 释放事件仅更新内部状态, 不返回
 *
 * @retval 按下的按键编号 KEY_1~KEY_5, 无事件返回 KEY_NONE
 */
uint8_t Key_GetEvent(void);

#endif /* KEY_H */
