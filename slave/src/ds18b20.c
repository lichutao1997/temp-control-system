/**
 * @file    ds18b20.c
 * @brief   DS18B20 单总线温度传感器驱动实现
 *
 * ===================== DS18B20 工作原理 =====================
 *
 * DS18B20 使用单总线 (1-Wire) 协议通信, 仅需一根数据线 (DQ)。
 * 数据线必须外接 4.7kΩ 上拉电阻至 3.3V。
 *
 * 通信流程:
 *   1. 复位 + 检测存在脉冲 (主机拉低 480μs → 释放 → DS18B20 拉低 60~240μs)
 *   2. 发送 Skip ROM 命令 (0xCC) — 单个传感器时跳过 ROM 匹配
 *   3. 发送 Convert T 命令 (0x44) — 启动温度转换
 *   4. 等待转换完成 (12 位精度需 750ms, 或查询 DQ 线是否变高)
 *   5. 再次复位
 *   6. 发送 Skip ROM (0xCC) + Read Scratchpad (0xBE)
 *   7. 读取 2 字节温度数据 (LSB 在前)
 *
 * 温度数据格式:
 *   16 位有符号整数, 单位 1/16°C
 *   例如: 0x0191 = 401 = 401/16 = 25.0625°C
 *         0xFF5E = -162 = -162/16 = -10.125°C
 *
 * 单总线时序 (72MHz 主频下):
 *   写 1: 拉低 2μs → 释放 → 等待 60μs
 *   写 0: 拉低 60μs → 释放 → 等待 2μs
 *   读:   拉低 2μs → 释放 → 等待 10μs → 采样 → 等待 50μs
 */

#include "ds18b20.h"

/* ======================== 微秒延时 ======================== */

/**
 * @brief  粗略微秒延时 — 基于 72MHz 主频
 *
 * 计算: 每个 __NOP() 约 13.9ns (1/72MHz)
 *       us * 72 次 NOP ≈ us * 13.9ns = us (近似)
 * 注意: 这是粗略延时, 包含循环开销, 实际延时略长于标称值。
 * 对于 DS18B20 的时序要求 (±几μs 误差可接受) 足够。
 */
static void delay_us(uint32_t us)
{
    us *= 72;
    while (us--) {
        __NOP();
    }
}

static void delay_ms(uint32_t ms)
{
    while (ms--) {
        delay_us(1000);
    }
}

/* ======================== 单总线引脚操作 ======================== */

/**
 * @brief  将 DQ 引脚配置为推挽输出 (用于主机发送)
 */
static void DS18B20_SetOutput(void)
{
    GPIO_InitTypeDef gpio;
    gpio.GPIO_Pin = DS18B20_PIN;        /* PA0 */
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;   /* 推挽输出 */
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(DS18B20_PORT, &gpio);
}

/**
 * @brief  将 DQ 引脚配置为上拉输入 (用于接收 DS18B20 数据)
 * @note   外部已有 4.7kΩ 上拉电阻, 内部上拉可选
 */
static void DS18B20_SetInput(void)
{
    GPIO_InitTypeDef gpio;
    gpio.GPIO_Pin = DS18B20_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IPU;  /* 内部上拉输入 */
    GPIO_Init(DS18B20_PORT, &gpio);
}

static void DS18B20_DQ_Low(void)   { GPIO_ResetBits(DS18B20_PORT, DS18B20_PIN); }
static void DS18B20_DQ_High(void)  { GPIO_SetBits(DS18B20_PORT, DS18B20_PIN); }
static uint8_t DS18B20_DQ_Read(void) { return GPIO_ReadInputDataBit(DS18B20_PORT, DS18B20_PIN); }

/* ======================== 单总线协议 ======================== */

/**
 * @brief  复位并检测 DS18B20 存在脉冲
 *
 * 时序:
 *   主机拉低 DQ 480μs → 释放 → 等待 60μs → 采样 DQ
 *   如果 DS18B20 存在, 它会在 60~240μs 内拉低 DQ (存在脉冲)
 *
 * @retval 1=传感器存在, 0=无响应
 */
static uint8_t DS18B20_Reset(void)
{
    uint8_t presence;

    DS18B20_SetOutput();
    DS18B20_DQ_Low();
    delay_us(480);          /* 复位脉冲: 拉低 480μs */

    DS18B20_SetInput();     /* 释放总线 (外部上拉拉高) */
    delay_us(60);           /* 等待存在脉冲 */

    presence = !DS18B20_DQ_Read();  /* DQ 被拉低 = 存在 */
    delay_us(420);          /* 等待复位时序完成 */

    return presence;
}

/**
 * @brief  向 DS18B20 写入一个字节 (LSB first)
 *
 * 每 bit 的写时序:
 *   写 1: 主机拉低 2μs → 释放 → DS18B20 在 60μs 内采样
 *   写 0: 主机拉低 60μs → 释放 → 等待恢复时间
 */
static void DS18B20_WriteByte(uint8_t byte)
{
    for (uint8_t i = 0; i < 8; i++) {
        DS18B20_SetOutput();

        if (byte & 0x01) {
            /* 写 '1': 短脉冲 */
            DS18B20_DQ_Low();
            delay_us(2);
            DS18B20_DQ_High();
            delay_us(60);
        } else {
            /* 写 '0': 长脉冲 */
            DS18B20_DQ_Low();
            delay_us(60);
            DS18B20_DQ_High();
            delay_us(2);
        }
        byte >>= 1;  /* 移到下一 bit (LSB first) */
    }
}

/**
 * @brief  从 DS18B20 读取一个字节 (LSB first)
 *
 * 每 bit 的读时序:
 *   主机拉低 2μs → 释放 → 等待 10μs → 采样 DQ → 等待 50μs
 *
 * DS18B20 在主机释放后:
 *   如果要发 '1': 保持 DQ 高电平
 *   如果要发 '0': 拉低 DQ (在 15μs 内)
 */
static uint8_t DS18B20_ReadByte(void)
{
    uint8_t byte = 0;

    for (uint8_t i = 0; i < 8; i++) {
        byte >>= 1;  /* 先移位, 最后置位 MSB */

        DS18B20_SetOutput();
        DS18B20_DQ_Low();
        delay_us(2);            /* 主机拉低 2μs */

        DS18B20_SetInput();     /* 释放总线 */
        delay_us(10);           /* 等待 DS18B20 输出数据 */

        if (DS18B20_DQ_Read()) {
            byte |= 0x80;       /* DQ 高 = bit 1 */
        }

        delay_us(50);           /* 等待时序完成 */
    }
    return byte;
}

/* ======================== 公共函数 ======================== */

void DS18B20_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    DS18B20_Reset();  /* 初始化时检测传感器是否存在 */
}

void DS18B20_StartConvert(void)
{
    DS18B20_Reset();
    DS18B20_WriteByte(0xCC);  /* Skip ROM — 单个传感器跳过地址匹配 */
    DS18B20_WriteByte(0x44);  /* Convert T — 启动温度转换 */
    /* 转换在后台进行, 12 位精度需约 750ms */
}

float DS18B20_ReadTemp(void)
{
    if (!DS18B20_Reset()) {
        return -100.0f;  /* 传感器无响应 */
    }

    DS18B20_WriteByte(0xCC);  /* Skip ROM */
    DS18B20_WriteByte(0xBE);  /* Read Scratchpad — 读取暂存器 */

    uint8_t lsb = DS18B20_ReadByte();  /* 温度低字节 */
    uint8_t msb = DS18B20_ReadByte();  /* 温度高字节 */

    /**
     * 温度数据: 16 位有符号整数, 单位 1/16°C
     * 正数: 直接除以 16 得到 °C
     * 负数: 补码表示, int16_t 自动处理
     */
    int16_t raw = (int16_t)((msb << 8) | lsb);
    return (float)raw / 16.0f;
}

float DS18B20_ReadTempBlocking(void)
{
    DS18B20_StartConvert();
    delay_ms(750);  /* 阻塞等待转换完成 (12 位精度) */
    return DS18B20_ReadTemp();
}
