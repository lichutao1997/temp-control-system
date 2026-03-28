/**
 * @file    app_master.h
 * @brief   主机应用层逻辑
 *
 * 主机 (值班室) 负责:
 *   - 接收从机上报的温度和烟感数据
 *   - 通过 5 个按键发送控制指令 (风扇/模式/阈值/烟感开关)
 *   - 在 HUB12 点阵屏上显示系统状态 (64×16 级联双屏)
 *   - 检测从机离线 (10 秒超时)
 *
 * v3.0 更新: 删除湿度, 增加烟感状态, 显示布局重新设计
 */

#ifndef APP_MASTER_H
#define APP_MASTER_H

#include "stm32f10x.h"

/* ======================== 运行模式定义 ======================== */
/**
 * 与从机 (app_slave.h) 保持一致, 两个工程的模式值必须相同。
 * 如果 protocol.h 中已有定义, 可以删除此处重复定义。
 */
#ifndef MODE_AUTO
#define MODE_AUTO   0x01   /**< 自动模式 — 从机根据温度阈值自动控制风扇 */
#endif
#ifndef MODE_MANUAL
#define MODE_MANUAL 0x00   /**< 手动模式 — 通过主机按键远程控制风扇 */
#endif

/* ======================== 主机状态结构体 ======================== */

/**
 * @brief 主机显示状态 — 存储从机上报的数据和本地控制状态
 *
 * 该结构体由 LoRa 接收中断更新, 由主循环读取和显示。
 * 所有字段在 OnRxFrame 回调中刷新。
 *
 * v3.0 更新: 删除湿度字段, 增加烟感状态字段
 */
typedef struct {
    float    temperature;       /**< 从机上报的当前温度 (°C), 如 28.5 */
    uint8_t  smoke_enable;   /**< 烟感功能开关: 1=开启, 0=关闭 */
    uint8_t  smoke_status;   /**< 烟感状态: 1=检测到烟雾, 0=正常 */
    uint8_t  fan_on;            /**< 风扇状态: 1=开, 0=关 */
    uint8_t  mode;              /**< 运行模式: MODE_AUTO(0x01) / MODE_MANUAL(0x00) */
    uint8_t  threshold;         /**< 温度阈值 (°C), 范围 20~50 */
    uint8_t  slave_online;      /**< 从机在线标志: 1=在线, 0=离线 */
    uint32_t last_rx_tick;      /**< 上次收到有效数据的系统时间 (ms) */
} MasterState_t;

/* ======================== 公共函数 ======================== */

/**
 * @brief  主机应用初始化
 *
 * 初始化顺序:
 *   1. SysTick 1ms 定时器
 *   2. HUB12 点阵屏 (级联双屏, 64×16)
 *   3. 按键 GPIO (5个按键)
 *   4. LoRa SX1278 (SPI1 + DIO0 中断)
 *   5. 初始显示 "1# 温控 V3.0"
 *
 * v3.0 更新: 显示内容和布局调整
 */
void App_Master_Init(void);

/**
 * @brief  主机主循环处理 — 在 while(1) 中反复调用
 *
 * 循环内部按时间间隔分发任务:
 *   - 每 20ms:   按键扫描 + 消抖
 *   - 每 500ms:  更新点阵屏显示
 *   - 实时:      从机在线检测 (10s 超时)
 *   - 实时:      LoRa 接收中断自动处理
 *
 * v3.0 更新: 增加 KEY5 处理 (烟感开关)
 */
void App_Master_Loop(void);

#endif /* APP_MASTER_H */
