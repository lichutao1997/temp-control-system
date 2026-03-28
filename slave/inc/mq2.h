/**
 * @file    mq2.h
 * @brief   MQ-2 烟雾传感器驱动
 *
 * MQ-2 传感器特性:
 *   - 检测烟雾、液化气、丙烷、氢气、甲烷等可燃气体
 *   - 输出方式: 数字信号 (DO) + 模拟信号 (AO)
 *   - 灵敏度可通过电位器调节
 *   - 工作电压: 5V
 *
 * 硬件连接:
 *   - VCC: 5V 供电
 *   - GND: 地
 *   - AOUT: 模拟输出 (本驱动使用 DO 数字输出)
 *   - DOUT: 数字输出 (比较器输出, 可通过电位器调节阈值)
 *
 * 本驱动使用 DO 数字输出方式:
 *   - DOUT 输出低电平: 检测到烟雾 (超过阈值)
 *   - DOUT 输出高电平: 正常 (低于阈值)
 *
 * v3.0 新增: 用于替代 DHT22 温湿度传感器
 */

#ifndef MQ2_H
#define MQ2_H

#include "stm32f10x.h"

/* ======================== MQ-2 引脚定义 ======================== */

/** @brief MQ-2 数字输出引脚 — PA1 */
#define MQ2_PORT    GPIOA
#define MQ2_PIN     GPIO_Pin_1

/* ======================== 烟感状态 ======================== */

#define SMOKE_DETECTED     1   /**< 检测到烟雾 */
#define SMOKE_NORMAL       0   /**< 正常, 无烟雾 */

/* ======================== 公共函数 ======================== */

/**
 * @brief  初始化 MQ-2 传感器
 *
 * 配置 PA1 为输入模式, 用于读取 DOUT 信号。
 * DOUT 默认高电平, 检测到烟雾时拉低。
 */
void MQ2_Init(void);

/**
 * @brief  读取烟雾状态
 * @retval SMOKE_DETECTED (1) = 检测到烟雾
 * @retval SMOKE_NORMAL   (0) = 正常
 *
 * 读取 PA1 引脚电平:
 *   - 低电平 (Bit_RESET): 检测到烟雾
 *   - 高电平 (Bit_SET):   正常
 */
uint8_t MQ2_ReadStatus(void);

#endif /* MQ2_H */
