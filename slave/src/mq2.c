/**
 * @file    mq2.c
 * @brief   MQ-2 烟雾传感器驱动实现
 *
 * MQ-2 传感器使用比较器输出 (DOUT):
 *   - DOUT 低电平: 检测到烟雾 (浓度超过电位器设定的阈值)
 *   - DOUT 高电平: 正常
 *
 * 预热说明:
 *   MQ-2 传感器需要预热 20~60 秒才能达到稳定工作状态。
 *   首次上电时建议等待足够时间再进行检测。
 *
 * v3.0 新增: 用于替代 DHT22 温湿度传感器
 */

#include "mq2.h"

/**
 * @brief  初始化 MQ-2 传感器
 *
 * 配置 PA1 为输入模式。
 * DOUT 默认高电平 (上拉), 检测到烟雾时拉低。
 */
void MQ2_Init(void)
{
    GPIO_InitTypeDef gpio;

    /* 开启 GPIOA 时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    /* 配置 PA1 为输入上拉模式 */
    gpio.GPIO_Pin  = MQ2_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IPU;  /* 内部上拉 */
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(MQ2_PORT, &gpio);
}

/**
 * @brief  读取烟雾状态
 * @retval SMOKE_DETECTED (1) = 检测到烟雾
 * @retval SMOKE_NORMAL   (0) = 正常
 */
uint8_t MQ2_ReadStatus(void)
{
    /* DOUT 低电平 = 检测到烟雾 */
    if (GPIO_ReadInputDataBit(MQ2_PORT, MQ2_PIN) == Bit_RESET) {
        return SMOKE_DETECTED;
    }
    return SMOKE_NORMAL;
}
