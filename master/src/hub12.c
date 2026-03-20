/**
 * @file    hub12.c
 * @brief   P10 单色点阵 LED 屏 HUB12 接口驱动实现
 *
 * ===================== HUB12 工作原理 =====================
 *
 * P10 模块是 32×16 像素的单色 LED 点阵屏, 采用 HUB12 接口。
 * 模块内部使用 74HC595 移位寄存器级联驱动列数据, 74HC138
 * 行扫描译码器选择当前点亮的行组。
 *
 * 1/4 扫描时序:
 *   地址线 (A, B) 选择当前点亮的行组, 每组包含 4 行:
 *
 *     A  B  │ 点亮的行
 *     ──────┼────────────────
 *     0  0  │ 0,  4,  8,  12
 *     1  0  │ 1,  5,  9,  13
 *     0  1  │ 2,  6,  10, 14
 *     1  1  │ 3,  7,  11, 15
 *
 *   每组扫描时, 先串行移入 32 列 × 4 bit = 128 个时钟,
 *   然后锁存输出, 再切换到下一组。人眼视觉暂留效果使
 *   4 组快速交替点亮时看起来像是同时显示。
 *
 * 数据移入顺序:
 *   从最右列 (col=31) 到最左列 (col=0) 逐列移入。
 *   每列移入 4 bit (从上到下), 对应当前组的 4 行。
 *   先移入的 bit 对应最右边的列。
 *
 * GPIO 速度说明:
 *   使用 BSRR/BRR 寄存器直接操作, 比库函数快约 5 倍。
 *   在 72MHz 主频下, 单次 BSRR 写操作约 0.14μs。
 */

#include "hub12.h"
#include "font.h"
#include <string.h>

/* ======================== 显示缓冲区 ======================== */
/**
 * 帧缓冲区 — 64 字节, 对应 32×16 = 512 像素
 * 每 bit 代表一个像素, 1=亮, 0=灭
 * 排列方式: 逐行, 每行 4 字节 (32 像素 / 8 bit = 4 字节)
 */
static uint8_t _framebuf[HUB12_BUF_SIZE];

/**
 * 当前扫描组号 (0~3)
 * 每次 HUB12_Refresh() 调用后自动 +1 并模 4
 */
static volatile uint8_t _scan_group = 0;

/* ======================== GPIO 快速宏 ======================== */
/**
 * 使用 STM32 的 BSRR (置位) 和 BRR (清零) 寄存器直接操作 GPIO,
 * 比 GPIO_SetBits/GPIO_ResetBits 库函数更快。
 *
 * BSRR: 写 1 到对应位 = 置高, 写 0 无效果
 * BRR:  写 1 到对应位 = 置低, 写 0 无效果
 */
#define A_HIGH()    (HUB12_A_PORT->BSRR = HUB12_A_PIN)
#define A_LOW()     (HUB12_A_PORT->BRR  = HUB12_A_PIN)
#define B_HIGH()    (HUB12_B_PORT->BSRR = HUB12_B_PIN)
#define B_LOW()     (HUB12_B_PORT->BRR  = HUB12_B_PIN)
#define CLK_HIGH()  (HUB12_CLK_PORT->BSRR = HUB12_CLK_PIN)
#define CLK_LOW()   (HUB12_CLK_PORT->BRR  = HUB12_CLK_PIN)
#define STB_HIGH()  (HUB12_STB_PORT->BSRR = HUB12_STB_PIN)
#define STB_LOW()   (HUB12_STB_PORT->BRR  = HUB12_STB_PIN)
#define OE_HIGH()   (HUB12_OE_PORT->BSRR = HUB12_OE_PIN)
#define OE_LOW()    (HUB12_OE_PORT->BRR  = HUB12_OE_PIN)
#define R1_HIGH()   (HUB12_R1_PORT->BSRR = HUB12_R1_PIN)
#define R1_LOW()    (HUB12_R1_PORT->BRR  = HUB12_R1_PIN)

/* ======================== 私有函数 ======================== */

/**
 * @brief  从帧缓冲区读取指定像素的值
 * @param  x: 列 (0~31)
 * @param  y: 行 (0~15)
 * @retval 1=亮, 0=灭
 *
 * 缓冲区布局: byte_idx = y * 4 + (x / 8), bit_idx = x % 8
 */
static uint8_t Framebuf_GetPixel(uint8_t x, uint8_t y)
{
    uint16_t byte_idx = y * (HUB12_WIDTH / 8) + (x / 8);
    uint8_t bit_idx = x % 8;
    return (_framebuf[byte_idx] >> bit_idx) & 1;
}

/* ======================== 公共函数实现 ======================== */

void HUB12_Init(void)
{
    GPIO_InitTypeDef gpio;

    /* 开启 GPIOB 时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    /* 配置 PB3~PB8 为推挽输出, 最大速率 50MHz */
    gpio.GPIO_Pin = HUB12_A_PIN | HUB12_B_PIN | HUB12_CLK_PIN |
                    HUB12_STB_PIN | HUB12_OE_PIN | HUB12_R1_PIN;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &gpio);

    /* 初始状态: 关闭显示, 锁存和时钟为低 */
    OE_HIGH();      /* OE 高电平 = 关闭 LED 输出 (消隐) */
    STB_LOW();      /* 锁存信号初始为低 */
    CLK_LOW();      /* 时钟初始为低 */
    R1_LOW();       /* 数据线初始为低 */

    /* 清空帧缓冲 */
    HUB12_Clear();

    /* ============ TIM2 定时器配置 ============ */
    /**
     * 配置 TIM2 产生周期性中断, 用于自动扫描刷新。
     *
     * 计算:
     *   APB1 时钟 = 72 MHz (TIM2 挂在 APB1 上, 但 APB1 预分频=1, 所以 TIM 时钟=72MHz)
     *   预分频 = 72-1 → 计数频率 = 72MHz / 72 = 1 MHz
     *   周期 = 1250-1 → 中断频率 = 1MHz / 1250 = 800 Hz
     *   4 组扫描 → 每组频率 = 800 / 4 = 200 Hz
     *   总刷新率 = 200 Hz (即每秒整个屏幕刷新 200 次, 无闪烁)
     */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    TIM_TimeBaseInitTypeDef tim;
    TIM_TimeBaseStructInit(&tim);
    tim.TIM_Prescaler = 72 - 1;             /* 72MHz / 72 = 1MHz */
    tim.TIM_Period = 1250 - 1;              /* 1MHz / 1250 = 800Hz */
    tim.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &tim);

    /* 配置 TIM2 中断优先级 */
    NVIC_InitTypeDef nvic;
    nvic.NVIC_IRQChannel = TIM2_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 2;  /* 中等优先级 */
    nvic.NVIC_IRQChannelSubPriority = 0;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);

    /* 使能 TIM2 更新中断并启动定时器 */
    TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);
    TIM_Cmd(TIM2, ENABLE);
}

void HUB12_Clear(void)
{
    memset(_framebuf, 0, sizeof(_framebuf));
}

void HUB12_SetPixel(uint8_t x, uint8_t y, uint8_t on)
{
    /* 边界检查, 防止数组越界 */
    if (x >= HUB12_WIDTH || y >= HUB12_HEIGHT) return;

    /* 计算该像素在缓冲区中的字节索引和位索引 */
    uint16_t byte_idx = y * (HUB12_WIDTH / 8) + (x / 8);
    uint8_t bit_idx = x % 8;

    if (on) {
        _framebuf[byte_idx] |= (1 << bit_idx);    /* 置位 = 点亮 */
    } else {
        _framebuf[byte_idx] &= ~(1 << bit_idx);   /* 清零 = 熄灭 */
    }
}

void HUB12_DrawChar(uint8_t x, uint8_t y, char ch)
{
    const uint8_t *glyph = Font_GetChar(ch);

    /* 字模 5 列 × 7 行, 列优先存储 */
    for (uint8_t col = 0; col < 5; col++) {
        uint8_t data = glyph[col];
        for (uint8_t row = 0; row < 7; row++) {
            if (data & (1 << row)) {
                HUB12_SetPixel(x + col, y + row, 1);
            }
        }
    }
}

void HUB12_DrawString(uint8_t x, uint8_t y, const char *str)
{
    while (*str && x < HUB12_WIDTH) {
        HUB12_DrawChar(x, y, *str++);
        x += 6;  /* 5 列字模 + 1 列间距 */
    }
}

void HUB12_PrintLine(uint8_t line, const char *str)
{
    if (line > 2) return;  /* P10 只有 16 像素高, 最多分 3 行 */

    uint8_t y_start = line * 6;  /* 每行 6 像素高 */
    uint8_t y_end = y_start + 6;
    if (y_end > HUB12_HEIGHT) y_end = HUB12_HEIGHT;

    /* 先清除该行区域 (避免残留旧内容) */
    for (uint8_t py = y_start; py < y_end; py++) {
        for (uint8_t px = 0; px < HUB12_WIDTH; px++) {
            HUB12_SetPixel(px, py, 0);
        }
    }

    /* 绘制新文本 */
    HUB12_DrawString(0, y_start, str);
}

void HUB12_Refresh(void)
{
    /* 步骤 1: 关闭输出 (消隐, 防止移位时产生鬼影) */
    OE_HIGH();

    /* 步骤 2: 设置行选地址线 */
    /**
     * A = _scan_group 的 bit0
     * B = _scan_group 的 bit1
     *
     * scan_group=0 → A=0, B=0 → 点亮行 0,4,8,12
     * scan_group=1 → A=1, B=0 → 点亮行 1,5,9,13
     * scan_group=2 → A=0, B=1 → 点亮行 2,6,10,14
     * scan_group=3 → A=1, B=1 → 点亮行 3,7,11,15
     */
    if (_scan_group & 0x01) A_HIGH(); else A_LOW();
    if (_scan_group & 0x02) B_HIGH(); else B_LOW();

    /* 步骤 3: 串行移入列数据 */
    /**
     * 从最右列 (col=31) 到最左列 (col=0) 逐列移入。
     * 每列移入 4 个 bit (对应当前组的 4 行):
     *   k=0 → y = _scan_group + 0*4 = _scan_group
     *   k=1 → y = _scan_group + 1*4 = _scan_group + 4
     *   k=2 → y = _scan_group + 2*4 = _scan_group + 8
     *   k=3 → y = _scan_group + 3*4 = _scan_group + 12
     *
     * 每 bit: 先设置 R1 数据线电平, 然后产生 CLK 上升沿移入
     */
    for (int8_t col = HUB12_WIDTH - 1; col >= 0; col--) {
        for (uint8_t k = 0; k < HUB12_ROWS_PER_GROUP; k++) {
            uint8_t y = _scan_group + k * HUB12_SCAN_GROUPS;

            if (Framebuf_GetPixel(col, y)) {
                R1_HIGH();  /* 该像素亮 → R1=1 */
            } else {
                R1_LOW();   /* 该像素灭 → R1=0 */
            }

            /* CLK 上升沿: 数据移入移位寄存器 */
            CLK_HIGH();
            CLK_LOW();
        }
    }

    /* 步骤 4: 锁存 — STB 上升沿将移位寄存器内容转移到输出寄存器 */
    STB_HIGH();
    __NOP();    /* 短暂延时确保锁存信号宽度 */
    STB_LOW();

    /* 步骤 5: 打开输出 — OE 低电平允许 LED 点亮 */
    OE_LOW();

    /* 步骤 6: 切换到下一组 */
    _scan_group = (_scan_group + 1) % HUB12_SCAN_GROUPS;
}

/* ======================== TIM2 中断服务函数 ======================== */

/**
 * @brief  TIM2 更新中断 — 每 1.25ms 触发一次, 驱动 HUB12 扫描刷新
 *
 * 该 ISR 必须放在中断向量表中对应的 TIM2 位置。
 * 由于是定时器中断, 执行时间必须足够短 (远小于 1.25ms),
 * 本实现单次刷新约 50~80μs, 占空比约 4~6%, 对主循环无影响。
 */
void TIM2_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET) {
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
        HUB12_Refresh();
    }
}
