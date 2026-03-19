/**
 * @file    relay.h
 * @brief   继电器控制驱动
 */

#ifndef RELAY_H
#define RELAY_H

#include "stm32f10x.h"

/* 引脚定义: PB1 */
#define RELAY_PORT  GPIOB
#define RELAY_PIN   GPIO_Pin_1

void Relay_Init(void);
void Relay_On(void);
void Relay_Off(void);
uint8_t Relay_GetStatus(void);  /* 1=开, 0=关 */

#endif /* RELAY_H */
