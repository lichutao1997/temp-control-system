/**
 * @file    sx1278_regs.h
 * @brief   SX1278 LoRa 模块寄存器定义
 */

#ifndef SX1278_REGS_H
#define SX1278_REGS_H

/* ==================== LoRa 寄存器地址 ==================== */
#define SX1278_REG_FIFO                 0x00
#define SX1278_REG_OP_MODE              0x01
#define SX1278_REG_FRF_MSB              0x06
#define SX1278_REG_FRF_MID              0x07
#define SX1278_REG_FRF_LSB              0x08
#define SX1278_REG_PA_CONFIG            0x09
#define SX1278_REG_PA_RAMP              0x0A
#define SX1278_REG_OCP                  0x0B
#define SX1278_REG_LNA                  0x0C
#define SX1278_REG_FIFO_ADDR_PTR        0x0D
#define SX1278_REG_FIFO_TX_BASE_ADDR    0x0E
#define SX1278_REG_FIFO_RX_BASE_ADDR    0x0F
#define SX1278_REG_FIFO_RX_CURRENT_ADDR 0x10
#define SX1278_REG_IRQ_FLAGS_MASK       0x11
#define SX1278_REG_IRQ_FLAGS            0x12
#define SX1278_REG_RX_NB_BYTES          0x13
#define SX1278_REG_PKT_SNR_VALUE        0x19
#define SX1278_REG_PKT_RSSI_VALUE       0x1A
#define SX1278_REG_MODEM_CONFIG_1       0x1D
#define SX1278_REG_MODEM_CONFIG_2       0x1E
#define SX1278_REG_SYMB_TIMEOUT_LSB     0x1F
#define SX1278_REG_PREAMBLE_MSB         0x20
#define SX1278_REG_PREAMBLE_LSB         0x21
#define SX1278_REG_PAYLOAD_LENGTH       0x22
#define SX1278_REG_MAX_PAYLOAD_LENGTH   0x23
#define SX1278_REG_HOP_PERIOD           0x24
#define SX1278_REG_FIFO_RX_BYTE_ADDR    0x25
#define SX1278_REG_MODEM_CONFIG_3       0x26
#define SX1278_REG_INVERTIQ             0x33
#define SX1278_REG_DETECTION_THRESHOLD  0x37
#define SX1278_REG_SYNC_WORD            0x39
#define SX1278_REG_INVERTIQ2            0x3B
#define SX1278_REG_DIO_MAPPING_1        0x40
#define SX1278_REG_DIO_MAPPING_2        0x41
#define SX1278_REG_VERSION              0x42
#define SX1278_REG_PA_DAC               0x4D

/* ==================== 操作模式 ==================== */
#define SX1278_MODE_LONG_RANGE         0x80
#define SX1278_MODE_SLEEP              0x00
#define SX1278_MODE_STDBY              0x01
#define SX1278_MODE_TX                 0x03
#define SX1278_MODE_RX_CONTINUOUS      0x05
#define SX1278_MODE_RX_SINGLE          0x06

/* ==================== PA 配置 ==================== */
#define SX1278_PA_BOOST                0x80

/* ==================== IRQ 标志 ==================== */
#define SX1278_IRQ_TX_DONE             0x08
#define SX1278_IRQ_RX_DONE             0x40
#define SX1278_IRQ_RX_TIMEOUT          0x80
#define SX1278_IRQ_CAD_DONE            0x04
#define SX1278_IRQ_CAD_DETECTED        0x01

/* ==================== 默认参数 ==================== */
#define SX1278_VERSION                 0x12
#define SX1278_SYNC_WORD               0x12
#define SX1278_PREAMBLE_LENGTH         8
#define SX1278_MAX_PAYLOAD             64

#endif /* SX1278_REGS_H */
