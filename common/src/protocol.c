/**
 * @file    protocol.c
 * @brief   LoRa 无线通信协议实现
 */

#include "protocol.h"

uint8_t protocol_pack(uint8_t *frame, uint8_t src, uint8_t dst,
                      uint8_t type, const uint8_t *data, uint8_t len)
{
    if (!frame || len > FRAME_MAX_DATA_LEN) {
        return 0;
    }

    uint8_t idx = 0;
    frame[idx++] = FRAME_HEAD;
    frame[idx++] = src;
    frame[idx++] = dst;
    frame[idx++] = type;
    frame[idx++] = len;

    if (data && len > 0) {
        memcpy(&frame[idx], data, len);
        idx += len;
    }

    /* 计算异或校验: 从 SRC 到 DATA 结尾 */
    frame[idx] = protocol_calc_xor(&frame[1], idx - 1);
    idx++;
    frame[idx++] = FRAME_TAIL;

    return idx;
}

uint8_t protocol_parse(const uint8_t *frame, uint8_t len, Frame_t *out)
{
    if (!frame || !out || len < FRAME_OVERHEAD) {
        return 0;
    }

    /* 检查帧头帧尾 */
    if (frame[0] != FRAME_HEAD || frame[len - 1] != FRAME_TAIL) {
        return 0;
    }

    uint8_t data_len = frame[4];
    if (data_len > FRAME_MAX_DATA_LEN) {
        return 0;
    }

    /* 检查总长度是否匹配 */
    if (len != (FRAME_OVERHEAD + data_len)) {
        return 0;
    }

    /* 校验异或 */
    uint8_t xor_expected = frame[len - 2];
    uint8_t xor_calc = protocol_calc_xor(&frame[1], len - 3);
    if (xor_calc != xor_expected) {
        return 0;
    }

    /* 填充结构体 */
    out->head = frame[0];
    out->src_id = frame[1];
    out->dst_id = frame[2];
    out->type = frame[3];
    out->len = data_len;
    if (data_len > 0) {
        memcpy(out->data, &frame[5], data_len);
    }
    out->xor_check = xor_expected;
    out->tail = frame[len - 1];

    return 1;
}

uint8_t protocol_pack_sensor(uint8_t *frame, const SensorData_t *sdata)
{
    uint8_t buf[7];
    buf[0] = (uint8_t)(sdata->temperature_x10 >> 8);
    buf[1] = (uint8_t)(sdata->temperature_x10 & 0xFF);
    buf[2] = (uint8_t)(sdata->humidity_x10 >> 8);
    buf[3] = (uint8_t)(sdata->humidity_x10 & 0xFF);
    buf[4] = sdata->fan_status;
    buf[5] = sdata->mode;
    buf[6] = sdata->threshold;

    return protocol_pack(frame, DEV_ID_SLAVE, DEV_ID_MASTER,
                         RPT_SENSOR_DATA, buf, 7);
}
