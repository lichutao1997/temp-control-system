/**
 * @file    hub12.h
 * @brief   P10 单色点阵 LED 屏 HUB12 接口驱动 (级联双屏)
 *
 * HUB12 接口说明:
 *   HUB12 是 P10 单色 LED 模块的标准接口, 与 HUB75 (RGB 彩色) 不同。
 *   HUB12 使用 2 条数据线 (R1 上半屏 / R2 下半屏) + 2 条地址线 (A/B)
 *   实现 1/4 扫描, 每组扫描 4 行, 共 4 组 = 16 行。
 *
 *   本驱动支持 2 块 P10 级联 (64×16 像素)。
 *   面板1 (左侧): 32列, 面板2 (右侧): 32列
 *
 * 硬件引脚 (STM32F103C8T6):
 *   PB3  -> A    (行选地址位 0, LSB)
 *   PB4  -> B    (行选地址位 1, MSB)
 *   PB5  -> CLK  (移位时钟, 上升沿移入数据)
 *   PB6  -> STB  (锁存信号, 上升沿锁存到输出)
 *   PB7  -> OE   (输出使能, 低电平点亮, 高电平熄灭)
 *   PB8  -> R1   (面板1数据, 面板1最右列对应第一个数据)
 *   (面板2数据通过面板1级联输出, 无需额外GPIO)
 *
 * 级联原理:
 *   两块 P10 面板通过级联线连接。面板1的输出连接到面板2的输入。
 *   移入 64 列数据后, 面板1自动将后半部分数据级联到面板2。
 *   这样只需 8P 排针即可驱动双屏。
 *
 * P10 模块规格 (单块):
 *   尺寸: 32×16 像素
 *   扫描方式: 1/4 扫描
 *   接口: HUB12 (8P 排针)
 *   供电: 5V
 *
 * 级联后规格:
 *   尺寸: 64×16 像素 (2 块 32×16 级联)
 *
 * 显示缓冲区布局:
 *   128 字节, 每字节 8 bit, 逐行排列。
 *   byte[0~7]   = 第 0 行 (64 像素 / 8 = 8 字节)
 *   byte[8~15]  = 第 1 行
 *   ...
 *   byte[120~127] = 第 15 行
 *
 * v3.0 更新: 支持 2 块 P10 级联 (64×16 像素)
 */

#ifndef HUB12_H
#define HUB12_H

#include "stm32f10x.h"

/* ======================== HUB12 引脚定义 ======================== */

/** @brief 行选地址线 A (LSB) — 选择扫描组的 bit0 */
#define HUB12_A_PORT    GPIOB
#define HUB12_A_PIN     GPIO_Pin_3

/** @brief 行选地址线 B (MSB) — 选择扫描组的 bit1 */
#define HUB12_B_PORT    GPIOB
#define HUB12_B_PIN     GPIO_Pin_4

/** @brief 移位时钟 — 每个上升沿将 R1 数据移入移位寄存器 */
#define HUB12_CLK_PORT  GPIOB
#define HUB12_CLK_PIN   GPIO_Pin_5

/** @brief 锁存信号 — 上升沿将移位寄存器数据锁存到输出寄存器 */
#define HUB12_STB_PORT  GPIOB
#define HUB12_STB_PIN   GPIO_Pin_6

/** @brief 输出使能 — 低电平时 LED 点亮, 高电平时熄灭 (消隐) */
#define HUB12_OE_PORT   GPIOB
#define HUB12_OE_PIN    GPIO_Pin_7

/** @brief 数据线 R1 — 串行数据输入, 1=点亮该像素 */
#define HUB12_R1_PORT   GPIOB
#define HUB12_R1_PIN    GPIO_Pin_8

/* ======================== 屏幕参数 (级联双屏) ======================== */

/** @brief 屏幕宽度 (像素列数) — 级联后 64 列 */
#define HUB12_WIDTH         64

/** @brief 屏幕高度 (像素行数) */
#define HUB12_HEIGHT        16

/** @brief 扫描组数 — 1/4 扫描分为 4 组 */
#define HUB12_SCAN_GROUPS   4

/** @brief 每组扫描的行数 */
#define HUB12_ROWS_PER_GROUP (HUB12_HEIGHT / HUB12_SCAN_GROUPS)  /* 4 */

/** @brief 显示缓冲区大小 (字节): 64×16 / 8 = 128 */
#define HUB12_BUF_SIZE  (HUB12_WIDTH * HUB12_HEIGHT / 8)

/* ======================== 公共函数 ======================== */

/**
 * @brief  初始化 HUB12 驱动
 *
 *   配置 PB3~PB8 为推挽输出, 初始化 TIM2 定时器中断实现自动扫描刷新。
 *   TIM2 参数: 预分频 72 (1MHz), 计数 2500 (400Hz 中断)
 *   4 组扫描 × 400Hz = 每组 100Hz → 总刷新率 ~50Hz, 无闪烁
 *
 * v3.0 更新: 级联双屏需要更多时钟周期
 */
void HUB12_Init(void);

/**
 * @brief  清空显示缓冲区 (全屏熄灭)
 */
void HUB12_Clear(void);

/**
 * @brief  设置/清除单个像素
 * @param  x: 列坐标 (0~63), 超出范围自动忽略
 * @param  y: 行坐标 (0~15), 超出范围自动忽略
 * @param  on: 1=点亮, 0=熄灭
 *
 * v3.0 更新: x 范围从 0~31 扩展到 0~63
 */
void HUB12_SetPixel(uint8_t x, uint8_t y, uint8_t on);

/**
 * @brief  在指定位置绘制一个 ASCII 字符 (5×7 字模)
 * @param  x: 起始列 (0~63)
 * @param  y: 起始行 (0~15)
 * @param  ch: ASCII 字符 (0x20~0x7E), 超出范围显示为空格
 *
 * v3.0 更新: x 范围从 0~31 扩展到 0~63
 */
void HUB12_DrawChar(uint8_t x, uint8_t y, char ch);

/**
 * @brief  在指定位置绘制一个字符串
 * @param  x: 起始列
 * @param  y: 起始行
 * @param  str: 以 '\0' 结尾的字符串
 * @note   每个字符占 6 列宽 (5 列字模 + 1 列间距), 超出屏幕自动截断
 */
void HUB12_DrawString(uint8_t x, uint8_t y, const char *str);

/**
 * @brief  按行号显示一行文本 (自动清行再绘制)
 * @param  line: 行号 (0=上, 1=中, 2=下), 每行 6 像素高, 共 3 行
 * @param  str: 要显示的文本
 * @note   P10 屏 16 像素高, 分为 3 行显示:
 *         line 0: y=0~5   (1#/模式)
 *         line 1: y=6~11  (温度/阈值)
 *         line 2: y=12~15 (风扇/烟感状态)
 *
 * v3.0 更新: 显示内容从 3 行改为 3 行, 内容重新定义
 */
void HUB12_PrintLine(uint8_t line, const char *str);

/**
 * @brief  扫描刷新 — 由 TIM2 中断自动调用, 用户无需手动调用
 *
 *   扫描流程 (每组):
 *     1. 关闭输出 (OE=HIGH, 消隐)
 *     2. 设置地址线 A/B 选择当前组
 *     3. 从右到左逐列移入 64 列数据 (级联双屏)
 *     4. 锁存 (STB 脉冲)
 *     5. 打开输出 (OE=LOW, 点亮)
 *     6. 组号 +1
 *
 * v3.0 更新: 从 32 列扩展到 64 列, 支持级联双屏
 */
void HUB12_Refresh(void);

#endif /* HUB12_H */
