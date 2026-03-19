/**
 * @file    hub75.h
 * @brief   P10 点阵 LED 屏 HUB75 接口驱动
 */

#ifndef HUB75_H
#define HUB75_H

#include "stm32f10x.h"

/*
 * HUB75 接口引脚:
 *   PB3  -> A    (行选位 0)
 *   PB4  -> B    (行选位 1)
 *   PB5  -> CLK  (移位时钟)
 *   PB6  -> LAT  (锁存)
 *   PB7  -> OE   (使能, 低电平点亮)
 *   PB8  -> DATA (串行数据)
 *
 * P10 单色屏: 32x16 像素, 1/4 扫描
 */

#define HUB75_A_PORT    GPIOB
#define HUB75_A_PIN     GPIO_Pin_3
#define HUB75_B_PORT    GPIOB
#define HUB75_B_PIN     GPIO_Pin_4
#define HUB75_CLK_PORT  GPIOB
#define HUB75_CLK_PIN   GPIO_Pin_5
#define HUB75_LAT_PORT  GPIOB
#define HUB75_LAT_PIN   GPIO_Pin_6
#define HUB75_OE_PORT   GPIOB
#define HUB75_OE_PIN    GPIO_Pin_7
#define HUB75_DATA_PORT GPIOB
#define HUB75_DATA_PIN  GPIO_Pin_8

/* 屏幕尺寸 */
#define HUB75_WIDTH     32
#define HUB75_HEIGHT    16
#define HUB75_ROWS      4       /* 1/4 扫描 */
#define HUB75_ROW_PIXELS (HUB75_HEIGHT / HUB75_ROWS)  /* 4 行 */

/* 显示缓冲区 */
#define HUB75_BUF_SIZE  (HUB75_WIDTH * HUB75_HEIGHT / 8)  /* 64 字节 */

/**
 * @brief  初始化 HUB75 GPIO 和定时器 (刷新率 ~200Hz)
 */
void HUB75_Init(void);

/**
 * @brief  清屏
 */
void HUB75_Clear(void);

/**
 * @brief  设置单个像素
 * @param  x: 列 (0~31)
 * @param  y: 行 (0~15)
 * @param  on: 1=亮, 0=灭
 */
void HUB75_SetPixel(uint8_t x, uint8_t y, uint8_t on);

/**
 * @brief  在指定位置绘制字符
 * @param  x: 起始列
 * @param  y: 起始行
 * @param  ch: ASCII 字符
 */
void HUB75_DrawChar(uint8_t x, uint8_t y, char ch);

/**
 * @brief  在指定位置绘制字符串
 * @param  x: 起始列
 * @param  y: 起始行
 * @param  str: 字符串
 */
void HUB75_DrawString(uint8_t x, uint8_t y, const char *str);

/**
 * @brief  在指定行绘制格式化文本 (覆盖整行)
 * @param  line: 行号 (0~2, 每行 6 像素高)
 * @param  str: 字符串
 */
void HUB75_PrintLine(uint8_t line, const char *str);

/**
 * @brief  刷新显示 (在定时器中断中调用)
 */
void HUB75_Refresh(void);

#endif /* HUB75_H */
