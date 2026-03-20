/**
 * @file    protocol.h
 * @brief   LoRa 无线通信协议定义
 * @version 2.0
 *
 * ===================== 通信帧协议 =====================
 *
 * 主机和从机之间通过 LoRa SX1278 无线通信。
 * 所有数据以自定义帧格式传输, 包含帧头/帧尾校验和异或校验。
 *
 * 帧格式:
 *   [HEAD][SRC][DST][TYPE][LEN][DATA...][XOR][TAIL]
 *    0xAA  1B   1B   1B   1B   0~16B    1B   0x55
 *
 * 异或校验范围: SRC 到 DATA 结尾 (不含 HEAD、XOR、TAIL)
 *
 * 指令方向:
 *   主机→从机: CMD_FAN_CONTROL, CMD_SET_THRESHOLD, CMD_SET_MODE
 *   从机→主机: RPT_SENSOR_DATA
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <string.h>

/* ======================== 帧常量 ======================== */

#define FRAME_HEAD          0xAA    /**< 帧头标识 */
#define FRAME_TAIL          0x55    /**< 帧尾标识 */
#define FRAME_MAX_DATA_LEN  16      /**< 数据域最大长度 (字节) */
#define FRAME_HEADER_SIZE   5       /**< 帧头大小: HEAD + SRC + DST + TYPE + LEN */
#define FRAME_OVERHEAD      7       /**< 帧开销: HEADER + XOR + TAIL */
#define FRAME_MAX_LEN       (FRAME_OVERHEAD + FRAME_MAX_DATA_LEN) /* 23 字节 */

/* ======================== 设备 ID ======================== */

#define DEV_ID_MASTER       0x00    /**< 值班室主机 ID */
#define DEV_ID_SLAVE        0x01    /**< 设备房从机 ID */

/* ======================== 指令类型 ======================== */

#define CMD_FAN_CONTROL     0x01    /**< 风扇控制 — data[0]: 0x01=开, 0x00=关 */
#define CMD_SET_THRESHOLD   0x02    /**< 设置阈值 — data[0]: 温度值 20~50 (°C) */
#define CMD_SET_MODE        0x03    /**< 设置模式 — data[0]: 0x01=自动, 0x00=手动 */
#define RPT_SENSOR_DATA     0x11    /**< 从机上报数据 — 7 字节 (见 SensorData_t) */

/* ======================== 数据结构 ======================== */

/**
 * @brief 通信帧结构体 — 对应帧格式的各字段
 *
 * 使用 #pragma pack(1) 确保结构体按字节对齐,
 * 可以直接从接收到的字节数组中 cast。
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t head;                       /**< 帧头: 0xAA */
    uint8_t src_id;                     /**< 源设备 ID */
    uint8_t dst_id;                     /**< 目标设备 ID */
    uint8_t type;                       /**< 指令类型 */
    uint8_t len;                        /**< 数据长度 (0~16) */
    uint8_t data[FRAME_MAX_DATA_LEN];   /**< 数据内容 */
    uint8_t xor_check;                  /**< 异或校验值 */
    uint8_t tail;                       /**< 帧尾: 0x55 */
} Frame_t;
#pragma pack(pop)

/**
 * @brief 传感器上报数据结构 — RPT_SENSOR_DATA (0x11) 的数据域
 *
 * 数据域 7 字节, 大端序存储多字节整数。
 * 温度/湿度以 ×10 存储, 避免浮点传输。
 *
 * 示例: temperature_x10 = 285 → 实际温度 = 28.5°C
 */
#pragma pack(push, 1)
typedef struct {
    uint16_t temperature_x10;       /**< 温度 × 10 (大端序) */
    uint16_t humidity_x10;          /**< 湿度 × 10 (大端序) */
    uint8_t  fan_status;            /**< 风扇状态: 0x01=开, 0x00=关 */
    uint8_t  mode;                  /**< 运行模式: 0x01=自动, 0x00=手动 */
    uint8_t  threshold;             /**< 温度阈值 (20~50) */
} SensorData_t;
#pragma pack(pop)

/* ======================== 函数声明 ======================== */

/**
 * @brief  计算异或校验值
 * @param  buf: 数据缓冲区指针
 * @param  len: 需要校验的字节数
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
 * @brief  打包一帧通信数据
 *
 * 帧格式: [0xAA][src][dst][type][len][data...][xor][0x55]
 *
 * @param  frame: 输出帧缓冲区 (至少 FRAME_MAX_LEN 字节)
 * @param  src:   源设备 ID (DEV_ID_MASTER 或 DEV_ID_SLAVE)
 * @param  dst:   目标设备 ID
 * @param  type:  指令类型
 * @param  data:  数据内容指针 (可为 NULL)
 * @param  len:   数据长度 (0~16)
 * @retval 帧总字节数, 失败返回 0 (参数错误)
 */
uint8_t protocol_pack(uint8_t *frame, uint8_t src, uint8_t dst,
                      uint8_t type, const uint8_t *data, uint8_t len);

/**
 * @brief  解析接收到的帧
 *
 * 校验内容:
 *   1. 帧头 = 0xAA
 *   2. 帧尾 = 0x55
 *   3. 数据长度合理 (0~16)
 *   4. 总长度 = 7 + len
 *   5. 异或校验正确
 *
 * @param  frame: 接收缓冲区
 * @param  len:   接收到的字节数
 * @param  out:   解析输出的帧结构体
 * @retval 1=解析成功, 0=帧无效 (校验失败)
 */
uint8_t protocol_parse(const uint8_t *frame, uint8_t len, Frame_t *out);

/**
 * @brief  快速打包传感器上报帧
 *
 * 自动设置: src=DEV_ID_SLAVE, dst=DEV_ID_MASTER, type=RPT_SENSOR_DATA
 *
 * @param  frame:  输出帧缓冲区
 * @param  sdata:  传感器数据结构体
 * @retval 帧总字节数
 */
uint8_t protocol_pack_sensor(uint8_t *frame, const SensorData_t *sdata);

#endif /* PROTOCOL_H */
