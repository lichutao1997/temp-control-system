/**
 * @file    app_master.h
 * @brief   主机应用层逻辑
 */

#ifndef APP_MASTER_H
#define APP_MASTER_H

#include "stm32f10x.h"

/* 主机显示状态 */
typedef struct {
    float    temperature;       /* 从机上报的温度 */
    float    humidity;          /* 从机上报的湿度 */
    uint8_t  fan_on;            /* 风扇状态 */
    uint8_t  mode;              /* 运行模式 */
    uint8_t  threshold;         /* 温度阈值 */
    uint8_t  slave_online;      /* 从机在线状态 */
    uint32_t last_rx_tick;      /* 上次收到数据的时间 */
} MasterState_t;

/**
 * @brief  主机应用初始化
 */
void App_Master_Init(void);

/**
 * @brief  主机主循环处理
 */
void App_Master_Loop(void);

#endif /* APP_MASTER_H */
