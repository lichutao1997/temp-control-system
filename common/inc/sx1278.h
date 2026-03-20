/**
 * @file    sx1278.h
 * @brief   SX1278 LoRa 无线模块驱动 (SPI 接口)
 *
 * ===================== SX1278 模块说明 =====================
 *
 * SX1278 是 Semtech 公司的 LoRa 调制射频收发器芯片。
 * 工作频率 137~1020 MHz, 本项目使用 434 MHz。
 *
 * SPI 接口 (STM32F103C8T6):
 *   PA4 → NSS  (片选, 低电平有效, 软件控制)
 *   PA5 → SCK  (SPI 时钟, 9 MHz)
 *   PA6 → MISO (主入从出)
 *   PA7 → MOSI (主出从入)
 *   PB0 → DIO0 (中断输出, 上升沿触发, EXTI0)
 *
 * 推荐配置 (本项目):
 *   频率:       434 MHz
 *   扩频因子:   SF9 (距离 vs 速率平衡点)
 *   带宽:       125 kHz
 *   编码率:     4/5
 *   发射功率:   17 dBm
 *   同步字:     0x12 (LoRaWAN 公共同步字)
 *   前导码:     8 符号
 *
 * ⚠️ 重要: 发射时必须连接 433MHz 天线, 否则可能损坏射频前端!
 */

#ifndef SX1278_H
#define SX1278_H

#include "stm32f10x.h"
#include "sx1278_regs.h"

/* ======================== 配置结构体 ======================== */

/**
 * @brief  SX1278 初始化配置参数
 *
 * 使用方法:
 *   SX1278_Config_t cfg = {
 *       .frequency = 434000000,
 *       .spreading_factor = 9,
 *       .bandwidth = 7,        // 125 kHz
 *       .coding_rate = 1,      // 4/5
 *       .tx_power = 17,
 *       .sync_word = 0x12,
 *       .preamble_len = 8,
 *   };
 *   SX1278_Init(&cfg);
 */
typedef struct {
    uint32_t frequency;             /**< 频率 (Hz), 如 434000000 */
    uint8_t  spreading_factor;      /**< 扩频因子: 6~12, 推荐 7~10 */
    uint8_t  bandwidth;             /**< 带宽枚举值 (0=7.8kHz ~ 9=500kHz, 7=125kHz) */
    uint8_t  coding_rate;           /**< 编码率: 1~4 对应 4/5~4/8, 推荐 1 (4/5) */
    int8_t   tx_power;              /**< 发射功率 (dBm): 2~17 */
    uint8_t  sync_word;             /**< 同步字: 0x12 为 LoRaWAN 公共值 */
    uint8_t  preamble_len;          /**< 前导码长度 (符号数): 推荐 6~65535 */
} SX1278_Config_t;

/* ======================== 状态枚举 ======================== */

typedef enum {
    SX1278_OK = 0,          /**< 初始化/操作成功 */
    SX1278_ERROR,           /**< 通用错误 (参数无效等) */
    SX1278_TIMEOUT,         /**< 操作超时 */
    SX1278_NOT_FOUND,       /**< 芯片未检测到 (版本号不匹配) */
} SX1278_Status_t;

/* ======================== 回调类型 ======================== */

/**
 * @brief 接收完成回调函数类型
 * @param data: 接收到的数据
 * @param len:  数据长度
 * @param rssi: 信号强度 (dBm), 如 -80
 */
typedef void (*SX1278_RxCallback_t)(uint8_t *data, uint8_t len, int16_t rssi);

/** @brief 发送完成回调函数类型 */
typedef void (*SX1278_TxDoneCallback_t)(void);

/* ======================== 公共函数 ======================== */

/**
 * @brief  初始化 SX1278 模块
 *
 * 初始化流程:
 *   1. 配置 SPI1 GPIO + EXTI0 中断
 *   2. 进入 LoRa Sleep 模式
 *   3. 验证芯片版本号 (应返回 0x12)
 *   4. 配置频率/功率/调制参数
 *   5. 设置 FIFO 基地址
 *   6. 清除 IRQ 标志
 *   7. 进入 Standby 模式
 *
 * @param  config: 配置参数
 * @retval SX1278_OK / SX1278_NOT_FOUND (版本号不对)
 */
SX1278_Status_t SX1278_Init(const SX1278_Config_t *config);

/**
 * @brief  发送数据 (非阻塞)
 *
 * 将数据写入 FIFO, 切换到 TX 模式。
 * 发送完成后通过 DIO0 中断触发, 在 ISR 中自动调用 TxDone 回调,
 * 然后自动回到 RX 模式。
 *
 * @param  data: 数据指针
 * @param  len:  数据长度 (1~64 字节)
 * @retval SX1278_OK / SX1278_ERROR (忙或参数错误)
 */
SX1278_Status_t SX1278_Send(const uint8_t *data, uint8_t len);

/**
 * @brief  进入连续接收模式
 *
 * 设置 DIO0 映射为 RxDone, 进入 RX Continuous 模式。
 * 接收到数据后通过 DIO0 中断触发, 在 ISR 中调用 RxCallback。
 *
 * @retval SX1278_Status_t
 */
SX1278_Status_t SX1278_StartRx(void);

/**
 * @brief  DIO0 中断处理函数
 *
 * 在 EXTI0 ISR 中调用。处理:
 *   - TX_DONE: 清忙标志, 调用 TxDone 回调, 重新进入接收
 *   - RX_DONE: 读 FIFO + RSSI, 调用 RxCallback
 */
void SX1278_IRQHandler(void);

/** @brief 注册接收完成回调 */
void SX1278_SetRxCallback(SX1278_RxCallback_t cb);

/** @brief 注册发送完成回调 */
void SX1278_SetTxDoneCallback(SX1278_TxDoneCallback_t cb);

/**
 * @brief  检查模块是否正在发送
 * @retval 1=发送中, 0=空闲
 */
uint8_t SX1278_IsBusy(void);

/**
 * @brief  读取芯片版本号
 * @retval 版本号 (正常应为 0x12)
 */
uint8_t SX1278_ReadVersion(void);

#endif /* SX1278_H */
