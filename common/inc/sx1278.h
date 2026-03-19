/**
 * @file    sx1278.h
 * @brief   SX1278 LoRa 无线模块驱动 (SPI 接口)
 */

#ifndef SX1278_H
#define SX1278_H

#include "stm32f10x.h"
#include "sx1278_regs.h"

/* ==================== 配置结构体 ==================== */
typedef struct {
    uint32_t frequency;     /* 频率 (Hz), 如 434000000 */
    uint8_t  spreading_factor;  /* 扩频因子 6~12 */
    uint8_t  bandwidth;         /* 带宽枚举值 */
    uint8_t  coding_rate;       /* 编码率 1~4 (对应 4/5~4/8) */
    int8_t   tx_power;          /* 发射功率 (dBm), 2~17 */
    uint8_t  sync_word;         /* 同步字 */
    uint8_t  preamble_len;      /* 前导码长度 */
} SX1278_Config_t;

/* ==================== 状态枚举 ==================== */
typedef enum {
    SX1278_OK = 0,
    SX1278_ERROR,
    SX1278_TIMEOUT,
    SX1278_NOT_FOUND,
} SX1278_Status_t;

/* ==================== 回调类型 ==================== */
typedef void (*SX1278_RxCallback_t)(uint8_t *data, uint8_t len, int16_t rssi);
typedef void (*SX1278_TxDoneCallback_t)(void);

/* ==================== 函数声明 ==================== */

/**
 * @brief  初始化 SX1278 模块
 * @param  config: 配置参数
 * @retval SX1278_Status_t
 */
SX1278_Status_t SX1278_Init(const SX1278_Config_t *config);

/**
 * @brief  发送数据
 * @param  data: 数据指针
 * @param  len:  数据长度 (1~64)
 * @retval SX1278_Status_t
 */
SX1278_Status_t SX1278_Send(const uint8_t *data, uint8_t len);

/**
 * @brief  进入连续接收模式
 * @retval SX1278_Status_t
 */
SX1278_Status_t SX1278_StartRx(void);

/**
 * @brief  DIO0 中断处理函数 (需在 EXTI0 ISR 中调用)
 */
void SX1278_IRQHandler(void);

/**
 * @brief  注册接收完成回调
 */
void SX1278_SetRxCallback(SX1278_RxCallback_t cb);

/**
 * @brief  注册发送完成回调
 */
void SX1278_SetTxDoneCallback(SX1278_TxDoneCallback_t cb);

/**
 * @brief  检查模块是否正在发送
 */
uint8_t SX1278_IsBusy(void);

/**
 * @brief  读取芯片版本号 (用于验证连接)
 */
uint8_t SX1278_ReadVersion(void);

#endif /* SX1278_H */
