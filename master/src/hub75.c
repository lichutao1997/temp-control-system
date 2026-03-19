/**
 * @file    hub75.c
 * @brief   P10 点阵 LED 屏 HUB75 接口驱动实现
 */

#include "hub75.h"
#include "font.h"
#include <string.h>
#include <stdio.h>

/* 显示缓冲区 (1 bit per pixel) */
static uint8_t _framebuf[HUB75_BUF_SIZE];  /* 64 字节 */
static volatile uint8_t _current_row = 0;   /* 当前扫描行 0~3 */

/* ==================== GPIO 快速操作 ==================== */
/* 使用位带操作加速 GPIO */
#define A_HIGH()    (HUB75_A_PORT->BSRR = HUB75_A_PIN)
#define A_LOW()     (HUB75_A_PORT->BRR  = HUB75_A_PIN)
#define B_HIGH()    (HUB75_B_PORT->BSRR = HUB75_B_PIN)
#define B_LOW()     (HUB75_B_PORT->BRR  = HUB75_B_PIN)
#define CLK_HIGH()  (HUB75_CLK_PORT->BSRR = HUB75_CLK_PIN)
#define CLK_LOW()   (HUB75_CLK_PORT->BRR  = HUB75_CLK_PIN)
#define LAT_HIGH()  (HUB75_LAT_PORT->BSRR = HUB75_LAT_PIN)
#define LAT_LOW()   (HUB75_LAT_PORT->BRR  = HUB75_LAT_PIN)
#define OE_HIGH()   (HUB75_OE_PORT->BSRR = HUB75_OE_PIN)
#define OE_LOW()    (HUB75_OE_PORT->BRR  = HUB75_OE_PIN)
#define DATA_HIGH() (HUB75_DATA_PORT->BSRR = HUB75_DATA_PIN)
#define DATA_LOW()  (HUB75_DATA_PORT->BRR  = HUB75_DATA_PIN)

void HUB75_Init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    /* 所有 HUB75 引脚: Output Push-Pull */
    gpio.GPIO_Pin = HUB75_A_PIN | HUB75_B_PIN | HUB75_CLK_PIN |
                    HUB75_LAT_PIN | HUB75_OE_PIN | HUB75_DATA_PIN;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &gpio);

    /* 初始状态 */
    OE_HIGH();      /* 关闭显示 */
    LAT_LOW();
    CLK_LOW();

    HUB75_Clear();

    /* 配置 TIM2 用于扫描刷新 (~200Hz per row, 总 ~50Hz) */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    TIM_TimeBaseInitTypeDef tim;
    TIM_TimeBaseStructInit(&tim);
    tim.TIM_Prescaler = 72 - 1;         /* 1 MHz */
    tim.TIM_Period = 1250 - 1;          /* 800 Hz -> 4 行 -> 200 Hz */
    tim.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &tim);

    NVIC_InitTypeDef nvic;
    nvic.NVIC_IRQChannel = TIM2_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 2;
    nvic.NVIC_IRQChannelSubPriority = 0;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);

    TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);
    TIM_Cmd(TIM2, ENABLE);
}

void HUB75_Clear(void)
{
    memset(_framebuf, 0, sizeof(_framebuf));
}

void HUB75_SetPixel(uint8_t x, uint8_t y, uint8_t on)
{
    if (x >= HUB75_WIDTH || y >= HUB75_HEIGHT) return;

    uint16_t byte_idx = y * (HUB75_WIDTH / 8) + (x / 8);
    uint8_t bit_idx = x % 8;

    if (on) {
        _framebuf[byte_idx] |= (1 << bit_idx);
    } else {
        _framebuf[byte_idx] &= ~(1 << bit_idx);
    }
}

void HUB75_DrawChar(uint8_t x, uint8_t y, char ch)
{
    const uint8_t *glyph = Font_GetChar(ch);

    for (uint8_t col = 0; col < 5; col++) {
        uint8_t data = glyph[col];
        for (uint8_t row = 0; row < 7; row++) {
            if (data & (1 << row)) {
                HUB75_SetPixel(x + col, y + row, 1);
            }
        }
    }
}

void HUB75_DrawString(uint8_t x, uint8_t y, const char *str)
{
    while (*str && x < HUB75_WIDTH) {
        HUB75_DrawChar(x, y, *str++);
        x += 6;  /* 5 列 + 1 列间隔 */
    }
}

void HUB75_PrintLine(uint8_t line, const char *str)
{
    if (line > 2) return;

    uint8_t y = line * 6;  /* 每行 6 像素高 */
    uint8_t x = 0;

    /* 清除该行区域 */
    for (uint8_t px = 0; px < HUB75_WIDTH; px++) {
        for (uint8_t py = y; py < y + 6 && py < HUB75_HEIGHT; py++) {
            HUB75_SetPixel(px, py, 0);
        }
    }

    HUB75_DrawString(x, y, str);
}

/**
 * @brief  读取缓冲区中某像素的值
 * @param  x: 列 (0~31)
 * @param  y: 行 (0~15)
 * @retval 1=亮, 0=灭
 */
static uint8_t Framebuf_GetPixel(uint8_t x, uint8_t y)
{
    uint16_t byte_idx = y * (HUB75_WIDTH / 8) + (x / 8);
    uint8_t bit_idx = x % 8;
    return (_framebuf[byte_idx] >> bit_idx) & 1;
}

/**
 * @brief  扫描刷新 — P10 单色 32x16, 1/4 扫描
 *
 *  P10 面板内部结构:
 *    32列 x 16行, 分为 4 组扫描
 *    每组扫描 4 行, 2-bit 地址线 (A, B) 选择当前组
 *    scan=0 -> 点亮行 0,4,8,12
 *    scan=1 -> 点亮行 1,5,9,13
 *    scan=2 -> 点亮行 2,6,10,14
 *    scan=3 -> 点亮行 3,7,11,15
 *
 *  每列串行移入 4 bit (从上到下), 共 32 列 x 4 bit = 128 CLK
 */
void HUB75_Refresh(void)
{
    /* 关闭显示 (避免移位时鬼影) */
    OE_HIGH();

    /* 设置扫描行地址 (A=LSB, B=MSB) */
    if (_current_row & 0x01) A_HIGH(); else A_LOW();
    if (_current_row & 0x02) B_HIGH(); else B_LOW();

    /* 移入数据: 从右到左 (列 31 到列 0) */
    for (int8_t col = HUB75_WIDTH - 1; col >= 0; col--) {
        /* 当前扫描组点亮的行: _current_row, _current_row+4, +8, +12 */
        for (uint8_t k = 0; k < 4; k++) {
            uint8_t y = _current_row + k * 4;
            if (Framebuf_GetPixel(col, y)) {
                DATA_HIGH();
            } else {
                DATA_LOW();
            }
            CLK_HIGH();
            CLK_LOW();
        }
    }

    /* 锁存数据 */
    LAT_HIGH();
    __NOP();
    LAT_LOW();

    /* 打开显示 */
    OE_LOW();

    /* 下一组扫描 */
    _current_row = (_current_row + 1) % HUB75_ROWS;
}

/* TIM2 中断服务函数 */
void TIM2_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET) {
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
        HUB75_Refresh();
    }
}
