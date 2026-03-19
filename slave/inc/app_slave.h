/**
 * @file    app_slave.h
 * @brief   从机应用层逻辑
 */

#ifndef APP_SLAVE_H
#define APP_SLAVE_H

#include "stm32f10x.h"

/* 运行模式 */
#define MODE_AUTO   0x01
#define MODE_MANUAL 0x00

typedef struct {
    float    temperature;       /* 当前温度 */
    float    humidity;          /* 当前湿度 */
    uint8_t  fan_on;            /* 风扇状态 */
    uint8_t  mode;              /* 运行模式 */
    uint8_t  threshold;         /* 温度阈值 */
    uint32_t last_report_tick;  /* 上次上报时间 */
} SlaveState_t;

/**
 * @brief  从机应用初始化
 */
void App_Slave_Init(void);

/**
 * @brief  从机主循环处理 (在 while(1) 中调用)
 */
void App_Slave_Loop(void);

/**
 * @brief  获取当前状态 (用于调试)
 */
const SlaveState_t* App_Slave_GetState(void);

#endif /* APP_SLAVE_H */
