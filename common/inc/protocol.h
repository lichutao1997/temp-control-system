/**
 * @file    protocol.h
 * @brief   LoRa 无线通信协议定义
 * @version 2.0
 *
 * 帧格式: [HEAD][SRC][DST][TYPE][LEN][DATA...][XOR][TAIL]
 *         0xAA  1B   1B   1B   1B   0~16B    1B  0x55
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <string.h>

/* ======================== 常量定义 ======================== */
#define FRAME_HEAD          0xAA
#define FRAME_TAIL          0x55
#define FRAME_MAX_DATA_LEN  16
#define FRAME_HEADER_SIZE   5       /* HEAD + SRC + DST + TYPE + LEN */
#define FRAME_OVERHEAD      7       /* HEADER + XOR + TAIL */
#define FRAME_MAX_LEN       (FRAME_OVERHEAD + FRAME_MAX_DATA_LEN) /* 23 */

/* ======================== 设备 ID ======================== */
#define DEV_ID_MASTER       0x00    /* 值班室主机 */
#define DEV_ID_SLAVE        0x01    /* 设备房从机 */

/* ======================== 指令类型 ======================== */
#define CMD_FAN_CONTROL     0x01    /* 数据: 0x01=开, 0x00=关 */
#define CMD_SET_THRESHOLD   0x02    /* 数据: 温度阈值 20~50 (1字节) */
#define CMD_SET_MODE        0x03    /* 数据: 0x01=自动, 0x00=手动 */
#define RPT_SENSOR_DATA     0x11    /* 从机上报数据 */

/* ======================== 数据帧结构 ======================== */
#pragma pack(push, 1)
typedef struct {
    uint8_t head;                   /* 0xAA */
    uint8_t src_id;                 /* 源设备 ID */
    uint8_t dst_id;                 /* 目标设备 ID */
    uint8_t type;                   /* 指令类型 */
    uint8_t len;                    /* 数据长度 */
    uint8_t data[FRAME_MAX_DATA_LEN]; /* 数据内容 */
    uint8_t xor_check;              /* 异或校验 */
    uint8_t tail;                   /* 0x55 */
} Frame_t;
#pragma pack(pop)

/* 传感器上报数据结构 (RPT_SENSOR_DATA) */
#pragma pack(push, 1)
typedef struct {
    uint16_t temperature_x10;       /* 温度 x10, 如 285 = 28.5°C */
    uint16_t humidity_x10;          /* 湿度 x10, 如 650 = 65.0% */
    uint8_t  fan_status;            /* 0x01=开, 0x00=关 */
    uint8_t  mode;                  /* 0x01=自动, 0x00=手动 */
    uint8_t  threshold;             /* 温度阈值 */
} SensorData_t;
#pragma pack(pop)

/* ======================== 函数声明 ======================== */

/**
 * @brief  计算异或校验值
 * @param  buf: 数据缓冲区
 * @param  len: 需要校验的数据长度 (不含校验字节本身)
 * @retval 异或校验结果
 */
static inline uint8_t protocol_calc_xor(const uint8_t *buf, uint8_t len)
{
    uint8_t xor_val = 0;
    for (uint8_t i = 0; i < len; i++) {
        xor_val ^= buf[i];
    }
    return xor_val;
}

/**
 * @brief  打包一帧数据
 * @param  frame: 输出帧缓冲区
 * @param  src:   源设备 ID
 * @param  dst:   目标设备 ID
 * @param  type:  指令类型
 * @param  data:  数据内容
 * @param  len:   数据长度 (0~16)
 * @retval 帧总字节数，打包失败返回 0
 */
uint8_t protocol_pack(uint8_t *frame, uint8_t src, uint8_t dst,
                      uint8_t type, const uint8_t *data, uint8_t len);

/**
 * @brief  解析接收到的帧
 * @param  frame: 接收缓冲区
 * @param  len:   接收到的字节数
 * @param  out:   解析输出的帧结构体
 * @retval 1=解析成功, 0=帧无效
 */
uint8_t protocol_parse(const uint8_t *frame, uint8_t len, Frame_t *out);

/**
 * @brief  快速打包传感器上报帧
 * @param  frame:  输出帧缓冲区
 * @param  sdata:  传感器数据
 * @retval 帧总字节数
 */
uint8_t protocol_pack_sensor(uint8_t *frame, const SensorData_t *sdata);

#endif /* PROTOCOL_H */
