/**
 * @file    dht22.c
 * @brief   DHT22 (AM2302) 温湿度传感器驱动实现
 *
 * ===================== DHT22 工作原理 =====================
 *
 * DHT22 使用单总线协议, 一次通信包含 40 bit 数据:
 *   [湿度高8位] [湿度低8位] [温度高8位] [温度低8位] [校验和]
 *
 * 通信流程:
 *   1. 主机发送起始信号: 拉低 DATA 至少 1ms, 然后拉高 20~40μs
 *   2. DHT22 响应: 拉低 80μs + 拉高 80μs
 *   3. DHT22 发送 40 bit 数据:
 *      - 每 bit 以 50μs 低电平开始
 *      - '0': 高电平持续 26~28μs
 *      - '1': 高电平持续 70μs
 *   4. 校验: buf[0]+buf[1]+buf[2]+buf[3] 应等于 buf[4]
 *
 * 数据解析:
 *   湿度 = (buf[0]<<8 | buf[1]) / 10.0  (%RH)
 *   温度 = (buf[2]<<8 | buf[3]) / 10.0  (°C)
 *   如果 buf[2] 的 bit7 = 1, 表示负温度
 *
 * 注意事项:
 *   - DATA 引脚需外接 10kΩ 上拉电阻
 *   - 两次读取间隔至少 2 秒 (DHT22 采样率限制)
 *   - 时序对延时精度要求较高, 72MHz 下用 NOP 循环近似
 */

#include "dht22.h"

/* ======================== 微秒延时 ======================== */

/**
 * @brief  粗略微秒延时 (72MHz 主频)
 * @note   每次循环约 13.9ns, 乘以 72 近似 1μs
 */
static void delay_us(uint32_t us)
{
    us *= 72;
    while (us--) __NOP();
}

/* ======================== 引脚方向切换 ======================== */

/**
 * @brief  将 DATA 引脚配置为推挽输出 (主机发送起始信号)
 */
static void DHT22_SetOutput(void)
{
    GPIO_InitTypeDef gpio;
    gpio.GPIO_Pin = DHT22_PIN;           /* PA1 */
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(DHT22_PORT, &gpio);
}

/**
 * @brief  将 DATA 引脚配置为上拉输入 (接收 DHT22 数据)
 * @note   外部 10kΩ 上拉确保空闲时 DATA 为高电平
 */
static void DHT22_SetInput(void)
{
    GPIO_InitTypeDef gpio;
    gpio.GPIO_Pin = DHT22_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(DHT22_PORT, &gpio);
}

/* ======================== 读取单个 bit ======================== */

/**
 * @brief  从 DHT22 读取 1 bit 数据
 *
 * 时序:
 *   1. 等待 50μs 低电平结束 (DHT22 每 bit 以低电平开始)
 *   2. 延时 30μs 后采样:
 *      - 如果此时 DATA 仍为高 → bit = '1' (高电平持续 70μs)
 *      - 如果此时 DATA 已低 → bit = '0' (高电平持续 26~28μs)
 *   3. 等待高电平结束
 *
 * @retval 0 或 1
 */
static uint8_t DHT22_ReadBit(void)
{
    uint32_t timeout;

    /* 等待低电平结束 (最多 100μs) */
    timeout = 100;
    while (!GPIO_ReadInputDataBit(DHT22_PORT, DHT22_PIN) && timeout--) {
        delay_us(1);
    }

    /* 延时 30μs 后采样 — 这是区分 '0' 和 '1' 的关键时间点 */
    delay_us(30);
    uint8_t bit = GPIO_ReadInputDataBit(DHT22_PORT, DHT22_PIN);

    /* 等待高电平结束 */
    timeout = 100;
    while (GPIO_ReadInputDataBit(DHT22_PORT, DHT22_PIN) && timeout--) {
        delay_us(1);
    }

    return bit;
}

/* ======================== 公共函数 ======================== */

void DHT22_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    DHT22_SetInput();  /* 默认输入状态 (等待主机发起通信) */
}

uint8_t DHT22_Read(DHT22_Data_t *data)
{
    uint8_t buf[5];  /* 40 bit = 5 字节 */
    uint32_t timeout;

    data->valid = 0;

    /* ===== 步骤 1: 主机发送起始信号 ===== */
    DHT22_SetOutput();
    GPIO_ResetBits(DHT22_PORT, DHT22_PIN);
    delay_us(1500);         /* 拉低至少 1ms (这里用 1.5ms) */
    GPIO_SetBits(DHT22_PORT, DHT22_PIN);
    delay_us(30);           /* 拉高 20~40μs */
    DHT22_SetInput();       /* 切换为输入, 等待 DHT22 响应 */

    /* ===== 步骤 2: 检测 DHT22 响应 ===== */
    /* DHT22 先拉低 80μs, 再拉高 80μs */

    /* 等待 DHT22 拉低 (超时 100μs) */
    timeout = 100;
    while (GPIO_ReadInputDataBit(DHT22_PORT, DHT22_PIN) && timeout--) {
        delay_us(1);
    }
    if (timeout == 0) return 0;  /* 超时 — DHT22 无响应 */

    /* 等待低电平结束 (~80μs) */
    timeout = 100;
    while (!GPIO_ReadInputDataBit(DHT22_PORT, DHT22_PIN) && timeout--) {
        delay_us(1);
    }

    /* 等待高电平结束 (~80μs) */
    timeout = 100;
    while (GPIO_ReadInputDataBit(DHT22_PORT, DHT22_PIN) && timeout--) {
        delay_us(1);
    }

    /* ===== 步骤 3: 读取 40 bit 数据 ===== */
    for (uint8_t i = 0; i < 5; i++) {
        buf[i] = 0;
        for (uint8_t j = 0; j < 8; j++) {
            buf[i] <<= 1;           /* 左移, 为下一 bit 腾出位置 */
            buf[i] |= DHT22_ReadBit();  /* 读取 1 bit 并放入最低位 */
        }
    }

    /* ===== 步骤 4: 校验 ===== */
    uint8_t checksum = buf[0] + buf[1] + buf[2] + buf[3];
    if (checksum != buf[4]) {
        return 0;  /* 校验失败 */
    }

    /* ===== 步骤 5: 解析数据 ===== */

    /* 湿度: buf[0] = 高字节, buf[1] = 低字节 */
    uint16_t raw_humid = ((uint16_t)buf[0] << 8) | buf[1];
    data->humidity = (float)raw_humid / 10.0f;

    /* 温度: buf[2] = 高字节, buf[3] = 低字节 */
    uint16_t raw_temp = ((uint16_t)buf[2] << 8) | buf[3];
    if (raw_temp & 0x8000) {
        /* bit15 = 1 → 负温度 */
        raw_temp &= 0x7FFF;
        data->temperature = -(float)raw_temp / 10.0f;
    } else {
        data->temperature = (float)raw_temp / 10.0f;
    }

    data->valid = 1;
    return 1;
}
