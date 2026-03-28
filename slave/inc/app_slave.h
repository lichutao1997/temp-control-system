/**
 * @file    app_slave.h
 * @brief   从机应用层逻辑
 *
 * 从机 (设备房) 负责:
 *   - 读取 DS18B20 温度
 *   - 读取 MQ-2 烟雾传感器状态
 *   - 自动温控: 温度超过阈值自动启动风扇 (带回差控制)
 *   - 接收主机控制指令 (风扇/模式/阈值/烟感开关)
 *   - 每 3 秒上报一次传感器数据
 *
 * v3.0 更新: 删除 DHT22, 增加 MQ-2 烟雾传感器
 */

#ifndef APP_SLAVE_H
#define APP_SLAVE_H

#include "stm32f10x.h"

/* ======================== 运行模式定义 ======================== */

/**
 * 运行模式 — 主机和从机必须使用相同的值:
 *   MODE_AUTO:   从机根据温度阈值自动控制风扇
 *   MODE_MANUAL: 由主机按键远程控制风扇开关
 */
#define MODE_AUTO   0x01
#define MODE_MANUAL 0x00

/* ======================== 从机状态结构体 ======================== */

/**
 * @brief 从机运行状态 — 存储传感器数据和控制状态
 *
 * 该结构体在 App_Slave_Loop() 中周期性更新,
 * 并通过 LoRa 上报给主机。
 *
 * v3.0 更新: 删除湿度字段, 增加烟感相关字段
 */
typedef struct {
    float    temperature;       /**< 当前温度 (°C), 来自 DS18B20 */
    uint8_t  smoke_enable;     /**< 烟感功能开关: 1=开启, 0=关闭 */
    uint8_t  smoke_status;     /**< 烟感状态: 1=检测到烟雾, 0=正常 */
    uint8_t  fan_on;            /**< 风扇状态: 1=开, 0=关 */
    uint8_t  mode;              /**< 运行模式: MODE_AUTO / MODE_MANUAL */
    uint8_t  threshold;         /**< 温度阈值 (°C), 范围 20~50, 默认 30 */
    uint32_t last_report_tick;  /**< 上次数据上报的系统时间 (ms) */
} SlaveState_t;

/* ======================== 公共函数 ======================== */

/**
 * @brief  从机应用初始化
 *
 * 初始化顺序:
 *   1. SysTick 1ms 定时器
 *   2. DS18B20 温度传感器 (PA0)
 *   3. MQ-2 烟雾传感器 (PA1)
 *   4. 继电器 (PB1, 默认关闭)
 *   5. LoRa SX1278 (SPI1 + DIO0 中断)
 *   6. 启动首次 DS18B20 温度转换
 *
 * v3.0 更新: DHT22 → MQ-2 烟雾传感器
 */
void App_Slave_Init(void);

/**
 * @brief  从机主循环处理 — 在 while(1) 中反复调用
 *
 * 循环内部按时间间隔分发任务:
 *   - 每 2000ms: 启动 DS18B20 转换, 750ms 后读取温度
 *   - 每 500ms: 读取 MQ-2 烟雾传感器状态
 *   - 每 3000ms: LoRa 上报传感器数据
 *   - 实时:      自动温控逻辑 (带回差)
 *   - 实时:      LoRa 接收中断自动处理控制指令
 *
 * v3.0 更新: 增加烟雾检测逻辑
 */
void App_Slave_Loop(void);

/**
 * @brief  获取当前从机状态 (用于调试)
 * @retval 指向 SlaveState_t 的只读指针
 */
const SlaveState_t* App_Slave_GetState(void);

#endif /* APP_SLAVE_H */
