/**
 * @file    key.h
 * @brief   按键驱动 (4 个按键)
 */

#ifndef KEY_H
#define KEY_H

#include "stm32f10x.h"

/* 按键引脚定义 */
#define KEY1_PORT   GPIOC
#define KEY1_PIN    GPIO_Pin_13
#define KEY2_PORT   GPIOA
#define KEY2_PIN    GPIO_Pin_8
#define KEY3_PORT   GPIOB
#define KEY3_PIN    GPIO_Pin_11
#define KEY4_PORT   GPIOB
#define KEY4_PIN    GPIO_Pin_10

/* 按键编号 */
#define KEY_1       0
#define KEY_2       1
#define KEY_3       2
#define KEY_4       3
#define KEY_NONE    0xFF

/* 消抖时间 (ms) */
#define KEY_DEBOUNCE_MS     20
#define KEY_LONG_PRESS_MS   1000

/**
 * @brief  初始化按键 GPIO (内部上拉, 低电平有效)
 */
void Key_Init(void);

/**
 * @brief  扫描按键 (需周期性调用, 推荐 10ms)
 * @retval 按键编号 KEY_1~KEY_4, 无按键返回 KEY_NONE
 */
uint8_t Key_Scan(void);

/**
 * @brief  检测按键按下事件 (带消抖)
 * @retval 按下的按键编号, 无事件返回 KEY_NONE
 */
uint8_t Key_GetEvent(void);

#endif /* KEY_H */
