/**
 * @file    sx1278.c
 * @brief   SX1278 LoRa 无线模块驱动实现
 *
 * 硬件连接 (SPI1):
 *   PA4  -> NSS (片选, 低电平有效)
 *   PA5  -> SCK
 *   PA6  -> MISO
 *   PA7  -> MOSI
 *   PB0  -> DIO0 (中断输入)
 */

#include "sx1278.h"

/* ==================== 硬件引脚定义 ==================== */
#define LORA_SPI            SPI1
#define LORA_NSS_PORT       GPIOA
#define LORA_NSS_PIN        GPIO_Pin_4
#define LORA_DIO0_PORT      GPIOB
#define LORA_DIO0_PIN       GPIO_Pin_0

/* ==================== 私有变量 ==================== */
static volatile uint8_t  _tx_busy = 0;
static volatile uint8_t  _rx_ready = 0;
static uint8_t           _rx_buf[SX1278_MAX_PAYLOAD];
static uint8_t           _rx_len = 0;
static int16_t           _rx_rssi = 0;

static SX1278_RxCallback_t      _rx_callback = 0;
static SX1278_TxDoneCallback_t  _tx_done_callback = 0;

/* 带宽枚举值到寄存器值的映射 */
static const uint8_t bw_table[] = {
    0x00, /* 7.8 kHz */
    0x01, /* 10.4 kHz */
    0x02, /* 15.6 kHz */
    0x03, /* 20.8 kHz */
    0x04, /* 31.25 kHz */
    0x05, /* 41.7 kHz */
    0x06, /* 62.5 kHz */
    0x07, /* 125 kHz */
    0x08, /* 250 kHz */
    0x09, /* 500 kHz */
};

/* ==================== SPI 底层操作 ==================== */

static void SPI_Init_GPIO(void)
{
    GPIO_InitTypeDef gpio;
    SPI_InitTypeDef spi;

    /* 开启时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1 | RCC_APB2Periph_GPIOA |
                           RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);

    /* PA5=SCK, PA7=MOSI -> AF Push-Pull */
    gpio.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_7;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);

    /* PA6=MISO -> Input Pull-Up */
    gpio.GPIO_Pin = GPIO_Pin_6;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOA, &gpio);

    /* PA4=NSS -> Output Push-Pull (软件片选) */
    gpio.GPIO_Pin = GPIO_Pin_4;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);
    GPIO_SetBits(LORA_NSS_PORT, LORA_NSS_PIN);  /* 默认拉高 */

    /* PB0=DIO0 -> Input Pull-Down (外部中断) */
    gpio.GPIO_Pin = GPIO_Pin_0;
    gpio.GPIO_Mode = GPIO_Mode_IPD;
    GPIO_Init(GPIOB, &gpio);

    /* SPI1 配置 */
    spi.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    spi.SPI_Mode = SPI_Mode_Master;
    spi.SPI_DataSize = SPI_DataSize_8b;
    spi.SPI_CPOL = SPI_CPOL_Low;
    spi.SPI_CPHA = SPI_CPHA_1Edge;
    spi.SPI_NSS = SPI_NSS_Soft;
    spi.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_8; /* 9 MHz */
    spi.SPI_FirstBit = SPI_FirstBit_MSB;
    spi.SPI_CRCPolynomial = 7;
    SPI_Init(LORA_SPI, &spi);
    SPI_Cmd(LORA_SPI, ENABLE);
}

static void EXTI_Init_DIO0(void)
{
    EXTI_InitTypeDef exti;
    NVIC_InitTypeDef nvic;

    /* PB0 -> EXTI Line0 */
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOB, GPIO_PinSource0);

    exti.EXTI_Line = EXTI_Line0;
    exti.EXTI_Mode = EXTI_Mode_Interrupt;
    exti.EXTI_Trigger = EXTI_Trigger_Rising;
    exti.EXTI_LineCmd = ENABLE;
    EXTI_Init(&exti);

    nvic.NVIC_IRQChannel = EXTI0_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 1;
    nvic.NVIC_IRQChannelSubPriority = 0;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);
}

static uint8_t SPI_Transfer(uint8_t data)
{
    while (SPI_I2S_GetFlagStatus(LORA_SPI, SPI_I2S_FLAG_TXE) == RESET);
    SPI_I2S_SendData(LORA_SPI, data);
    while (SPI_I2S_GetFlagStatus(LORA_SPI, SPI_I2S_FLAG_RXNE) == RESET);
    return (uint8_t)SPI_I2S_ReceiveData(LORA_SPI);
}

static void NSS_Low(void)  { GPIO_ResetBits(LORA_NSS_PORT, LORA_NSS_PIN); }
static void NSS_High(void) { GPIO_SetBits(LORA_NSS_PORT, LORA_NSS_PIN); }

/* ==================== 寄存器读写 ==================== */

static uint8_t SX1278_ReadReg(uint8_t addr)
{
    uint8_t val;
    NSS_Low();
    SPI_Transfer(addr & 0x7F);  /* 读操作: bit7=0 */
    val = SPI_Transfer(0x00);
    NSS_High();
    return val;
}

static void SX1278_WriteReg(uint8_t addr, uint8_t val)
{
    NSS_Low();
    SPI_Transfer(addr | 0x80);  /* 写操作: bit7=1 */
    SPI_Transfer(val);
    NSS_High();
}

static void SX1278_WriteFIFO(const uint8_t *data, uint8_t len)
{
    NSS_Low();
    SPI_Transfer(SX1278_REG_FIFO | 0x80);
    for (uint8_t i = 0; i < len; i++) {
        SPI_Transfer(data[i]);
    }
    NSS_High();
}

static void SX1278_ReadFIFO(uint8_t *data, uint8_t len)
{
    NSS_Low();
    SPI_Transfer(SX1278_REG_FIFO & 0x7F);
    for (uint8_t i = 0; i < len; i++) {
        data[i] = SPI_Transfer(0x00);
    }
    NSS_High();
}

/* ==================== 模式切换 ==================== */

static void SX1278_SetMode(uint8_t mode)
{
    SX1278_WriteReg(SX1278_REG_OP_MODE, SX1278_MODE_LONG_RANGE | mode);
    /* 等待模式切换完成 */
    for (volatile uint32_t i = 0; i < 10000; i++);
}

/* ==================== 公共函数 ==================== */

uint8_t SX1278_ReadVersion(void)
{
    return SX1278_ReadReg(SX1278_REG_VERSION);
}

SX1278_Status_t SX1278_Init(const SX1278_Config_t *config)
{
    /* 初始化硬件 */
    SPI_Init_GPIO();
    EXTI_Init_DIO0();

    /* 进入睡眠模式以配置 LoRa */
    SX1278_SetMode(SX1278_MODE_SLEEP);

    /* 验证芯片版本 */
    if (SX1278_ReadReg(SX1278_REG_VERSION) != SX1278_VERSION) {
        return SX1278_NOT_FOUND;
    }

    /* 设置频率 */
    uint64_t frf = ((uint64_t)config->frequency << 19) / 32000000;
    SX1278_WriteReg(SX1278_REG_FRF_MSB, (uint8_t)(frf >> 16));
    SX1278_WriteReg(SX1278_REG_FRF_MID, (uint8_t)(frf >> 8));
    SX1278_WriteReg(SX1278_REG_FRF_LSB, (uint8_t)(frf >> 0));

    /* 设置 PA 配置: PA_BOOST, 输出功率 */
    uint8_t pa_config = SX1278_PA_BOOST;
    if (config->tx_power > 17) {
        /* 高功率模式: +20 dBm */
        pa_config |= 0x0F;
        SX1278_WriteReg(SX1278_REG_PA_DAC, 0x87);  /* 高功率 DAC */
    } else {
        pa_config |= (config->tx_power - 2) & 0x0F;
        SX1278_WriteReg(SX1278_REG_PA_DAC, 0x84);  /* 默认 DAC */
    }
    SX1278_WriteReg(SX1278_REG_PA_CONFIG, pa_config);

    /* 设置 OCP (过流保护) */
    SX1278_WriteReg(SX1278_REG_OCP, 0x2B);  /* 100 mA */

    /* 设置 LNA */
    SX1278_WriteReg(SX1278_REG_LNA, 0x23);  /* LNA 增益最大, 150% 电流 */

    /* 设置调制参数 */
    uint8_t bw_idx = config->bandwidth;
    if (bw_idx > 9) bw_idx = 7;  /* 默认 125 kHz */

    /* Modem Config 1: BW + CR + Implicit Header */
    uint8_t mc1 = (bw_table[bw_idx] << 4) | ((config->coding_rate & 0x07) << 1);
    SX1278_WriteReg(SX1278_REG_MODEM_CONFIG_1, mc1);

    /* Modem Config 2: SF + TX Continuous + CRC */
    uint8_t mc2 = ((config->spreading_factor & 0x0F) << 4) | 0x04;  /* CRC on */
    SX1278_WriteReg(SX1278_REG_MODEM_CONFIG_2, mc2);

    /* Modem Config 3: LowDataRateOptimize + AGC */
    uint8_t mc3 = 0x04;  /* AGC auto */
    if (config->spreading_factor >= 11 && bw_idx <= 6) {
        mc3 |= 0x08;  /* LowDataRateOptimize for SF>=11 and BW<=62.5kHz */
    }
    SX1278_WriteReg(SX1278_REG_MODEM_CONFIG_3, mc3);

    /* 设置前导码长度 */
    SX1278_WriteReg(SX1278_REG_PREAMBLE_MSB, (config->preamble_len >> 8) & 0xFF);
    SX1278_WriteReg(SX1278_REG_PREAMBLE_LSB, config->preamble_len & 0xFF);

    /* 设置同步字 */
    SX1278_WriteReg(SX1278_REG_SYNC_WORD, config->sync_word);

    /* 设置最大载荷长度 */
    SX1278_WriteReg(SX1278_REG_MAX_PAYLOAD_LENGTH, SX1278_MAX_PAYLOAD);

    /* 设置 FIFO 基地址 */
    SX1278_WriteReg(SX1278_REG_FIFO_TX_BASE_ADDR, 0x00);
    SX1278_WriteReg(SX1278_REG_FIFO_RX_BASE_ADDR, 0x00);

    /* 清除 IRQ 标志 */
    SX1278_WriteReg(SX1278_REG_IRQ_FLAGS, 0xFF);

    /* 使能 RX_DONE 和 TX_DONE 中断到 DIO0 */
    SX1278_WriteReg(SX1278_REG_DIO_MAPPING_1, 0x00);  /* DIO0=00 -> RxDone */

    /* 进入待机模式 */
    SX1278_SetMode(SX1278_MODE_STDBY);

    return SX1278_OK;
}

SX1278_Status_t SX1278_Send(const uint8_t *data, uint8_t len)
{
    if (_tx_busy || len == 0 || len > SX1278_MAX_PAYLOAD) {
        return SX1278_ERROR;
    }
    _tx_busy = 1;

    SX1278_SetMode(SX1278_MODE_STDBY);

    /* 设置 DIO0 映射为 TxDone */
    SX1278_WriteReg(SX1278_REG_DIO_MAPPING_1, 0x40);  /* DIO0=01 -> TxDone */

    /* 清 IRQ */
    SX1278_WriteReg(SX1278_REG_IRQ_FLAGS, 0xFF);

    /* 设置 FIFO 指针并写入数据 */
    SX1278_WriteReg(SX1278_REG_FIFO_ADDR_PTR, 0x00);
    SX1278_WriteFIFO(data, len);
    SX1278_WriteReg(SX1278_REG_PAYLOAD_LENGTH, len);

    /* 切换到发送模式 */
    SX1278_SetMode(SX1278_MODE_TX);

    return SX1278_OK;
}

SX1278_Status_t SX1278_StartRx(void)
{
    SX1278_SetMode(SX1278_MODE_STDBY);

    /* 设置 DIO0 映射为 RxDone */
    SX1278_WriteReg(SX1278_REG_DIO_MAPPING_1, 0x00);  /* DIO0=00 -> RxDone */

    /* 清 IRQ */
    SX1278_WriteReg(SX1278_REG_IRQ_FLAGS, 0xFF);

    /* 设置 FIFO 指针 */
    SX1278_WriteReg(SX1278_REG_FIFO_ADDR_PTR, SX1278_REG_FIFO_RX_BASE_ADDR);

    /* 进入连续接收模式 */
    SX1278_SetMode(SX1278_MODE_RX_CONTINUOUS);

    return SX1278_OK;
}

uint8_t SX1278_IsBusy(void)
{
    return _tx_busy;
}

void SX1278_SetRxCallback(SX1278_RxCallback_t cb)
{
    _rx_callback = cb;
}

void SX1278_SetTxDoneCallback(SX1278_TxDoneCallback_t cb)
{
    _tx_done_callback = cb;
}

/* ==================== 中断处理 ==================== */

void SX1278_IRQHandler(void)
{
    uint8_t irq_flags = SX1278_ReadReg(SX1278_REG_IRQ_FLAGS);

    /* 发送完成 */
    if (irq_flags & SX1278_IRQ_TX_DONE) {
        SX1278_WriteReg(SX1278_REG_IRQ_FLAGS, SX1278_IRQ_TX_DONE);
        _tx_busy = 0;

        if (_tx_done_callback) {
            _tx_done_callback();
        }

        /* 发送完自动回到接收模式 */
        SX1278_StartRx();
    }

    /* 接收完成 */
    if (irq_flags & SX1278_IRQ_RX_DONE) {
        SX1278_WriteReg(SX1278_REG_IRQ_FLAGS, SX1278_IRQ_RX_DONE);

        /* 读取接收数据 */
        _rx_len = SX1278_ReadReg(SX1278_REG_RX_NB_BYTES);
        if (_rx_len > SX1278_MAX_PAYLOAD) _rx_len = SX1278_MAX_PAYLOAD;

        uint8_t fifo_addr = SX1278_ReadReg(SX1278_REG_FIFO_RX_CURRENT_ADDR);
        SX1278_WriteReg(SX1278_REG_FIFO_ADDR_PTR, fifo_addr);
        SX1278_ReadFIFO(_rx_buf, _rx_len);

        /* 计算 RSSI */
        int16_t snr = (int8_t)SX1278_ReadReg(SX1278_REG_PKT_SNR_VALUE) / 4;
        int16_t rssi_raw = SX1278_ReadReg(SX1278_REG_PKT_RSSI_VALUE);
        if (snr < 0) {
            _rx_rssi = -157 + rssi_raw + snr;
        } else {
            _rx_rssi = -157 + rssi_raw;
        }

        if (_rx_callback) {
            _rx_callback(_rx_buf, _rx_len, _rx_rssi);
        }
    }

    /* 接收超时 */
    if (irq_flags & SX1278_IRQ_RX_TIMEOUT) {
        SX1278_WriteReg(SX1278_REG_IRQ_FLAGS, SX1278_IRQ_RX_TIMEOUT);
    }
}

/* EXTI0 中断服务函数 — 需要放入对应启动文件的中断向量表 */
void EXTI0_IRQHandler(void)
{
    if (EXTI_GetITStatus(EXTI_Line0) != RESET) {
        SX1278_IRQHandler();
        EXTI_ClearITPendingBit(EXTI_Line0);
    }
}
