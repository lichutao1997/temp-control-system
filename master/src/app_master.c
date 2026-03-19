/**
 * @file    app_master.c
 * @brief   主机应用层逻辑实现
 */

#include "app_master.h"
#include "hub75.h"
#include "key.h"
#include "sx1278.h"
#include "protocol.h"
#include <stdio.h>
#include <string.h>

/* ==================== 配置常量 ==================== */
#define KEY_SCAN_INTERVAL_MS    20      /* 按键扫描间隔 */
#define DISPLAY_UPDATE_MS       500     /* 显示更新间隔 */
#define SLAVE_TIMEOUT_MS        10000   /* 从机离线超时 */
#define DEFAULT_THRESHOLD       30

/* ==================== 私有变量 ==================== */
static MasterState_t _state;
static uint8_t _tx_buf[FRAME_MAX_LEN];

/* ==================== SysTick ==================== */
volatile uint32_t g_systick_ms = 0;

void SysTick_Handler(void)
{
    g_systick_ms++;
}

static uint32_t GetTick(void)
{
    return g_systick_ms;
}

static void SysTick_Init(void)
{
    SysTick_Config(SystemCoreClock / 1000);
}

/* ==================== LoRa 接收回调 ==================== */
static void OnRxFrame(uint8_t *data, uint8_t len, int16_t rssi)
{
    Frame_t frame;
    if (!protocol_parse(data, len, &frame)) {
        return;
    }

    /* 只处理发给本机的帧 */
    if (frame.dst_id != DEV_ID_MASTER) {
        return;
    }

    if (frame.type == RPT_SENSOR_DATA && frame.len >= 7) {
        SensorData_t *sd = (SensorData_t *)&frame.data[0];

        _state.temperature = (float)sd->temperature_x10 / 10.0f;
        _state.humidity = (float)sd->humidity_x10 / 10.0f;
        _state.fan_on = sd->fan_status;
        _state.mode = sd->mode;
        _state.threshold = sd->threshold;
        _state.slave_online = 1;
        _state.last_rx_tick = GetTick();
    }
}

/* ==================== 发送控制指令 ==================== */
static void SendFanControl(uint8_t on)
{
    uint8_t data = on ? 0x01 : 0x00;
    uint8_t len = protocol_pack(_tx_buf, DEV_ID_MASTER, DEV_ID_SLAVE,
                                CMD_FAN_CONTROL, &data, 1);
    if (len > 0) {
        SX1278_Send(_tx_buf, len);
    }
}

static void SendSetMode(uint8_t auto_mode)
{
    uint8_t data = auto_mode ? 0x01 : 0x00;
    uint8_t len = protocol_pack(_tx_buf, DEV_ID_MASTER, DEV_ID_SLAVE,
                                CMD_SET_MODE, &data, 1);
    if (len > 0) {
        SX1278_Send(_tx_buf, len);
    }
}

static void SendSetThreshold(uint8_t threshold)
{
    uint8_t data = threshold;
    uint8_t len = protocol_pack(_tx_buf, DEV_ID_MASTER, DEV_ID_SLAVE,
                                CMD_SET_THRESHOLD, &data, 1);
    if (len > 0) {
        SX1278_Send(_tx_buf, len);
    }
}

/* ==================== 按键处理 ==================== */
static void HandleKeys(void)
{
    uint8_t key = Key_GetEvent();
    if (key == KEY_NONE) return;

    switch (key) {
        case KEY_1:
            /* 手动开/关风扇 */
            if (_state.mode != 0x00) {
                /* 先切换到手动模式 */
                SendSetMode(0);
            }
            SendFanControl(!_state.fan_on);
            break;

        case KEY_2:
            /* 切换自动/手动模式 */
            if (_state.mode == MODE_AUTO) {
                SendSetMode(0);  /* 切换为手动 */
            } else {
                SendSetMode(1);  /* 切换为自动 */
            }
            break;

        case KEY_3:
            /* 温度阈值 +1°C */
            if (_state.threshold < 50) {
                _state.threshold++;
                SendSetThreshold(_state.threshold);
            }
            break;

        case KEY_4:
            /* 温度阈值 -1°C */
            if (_state.threshold > 20) {
                _state.threshold--;
                SendSetThreshold(_state.threshold);
            }
            break;
    }
}

/* ==================== 显示更新 ==================== */
static void UpdateDisplay(void)
{
    char line[32];

    if (!_state.slave_online) {
        HUB75_PrintLine(0, "设备房 --离线--");
        HUB75_PrintLine(1, "请检查从机连接");
        HUB75_PrintLine(2, "");
        return;
    }

    /* 第一行: 温度 + 湿度 */
    snprintf(line, sizeof(line), "T:%.1fC H:%.0f%%",
             _state.temperature, _state.humidity);
    HUB75_PrintLine(0, line);

    /* 第二行: 模式 + 风扇状态 */
    snprintf(line, sizeof(line), "模式:%s 风扇:%s",
             (_state.mode == MODE_AUTO) ? "自动" : "手动",
             _state.fan_on ? "开" : "关");
    HUB75_PrintLine(1, line);

    /* 第三行: 温度阈值 */
    snprintf(line, sizeof(line), "阈值: %dC", _state.threshold);
    HUB75_PrintLine(2, line);
}

/* ==================== 公共函数 ==================== */
void App_Master_Init(void)
{
    /* 初始化状态 */
    memset(&_state, 0, sizeof(_state));
    _state.threshold = DEFAULT_THRESHOLD;
    _state.slave_online = 0;

    /* SysTick */
    SysTick_Init();

    /* 外设初始化 */
    HUB75_Init();
    Key_Init();

    /* LoRa 初始化 */
    SX1278_Config_t lora_cfg = {
        .frequency = 434000000,
        .spreading_factor = 9,
        .bandwidth = 7,
        .coding_rate = 1,
        .tx_power = 17,
        .sync_word = SX1278_SYNC_WORD,
        .preamble_len = SX1278_PREAMBLE_LENGTH,
    };

    if (SX1278_Init(&lora_cfg) != SX1278_OK) {
        while (1);  /* LoRa 初始化失败 */
    }

    SX1278_SetRxCallback(OnRxFrame);
    SX1278_StartRx();

    /* 初始显示 */
    HUB75_PrintLine(0, "温控系统 v2.0");
    HUB75_PrintLine(1, "等待从机连接");
    HUB75_PrintLine(2, "");
}

void App_Master_Loop(void)
{
    uint32_t now = GetTick();
    static uint32_t last_key_tick = 0;
    static uint32_t last_display_tick = 0;

    /* 按键扫描 */
    if (now - last_key_tick >= KEY_SCAN_INTERVAL_MS) {
        HandleKeys();
        last_key_tick = now;
    }

    /* 检查从机在线状态 */
    if (_state.slave_online && (now - _state.last_rx_tick > SLAVE_TIMEOUT_MS)) {
        _state.slave_online = 0;
    }

    /* 显示更新 */
    if (now - last_display_tick >= DISPLAY_UPDATE_MS) {
        UpdateDisplay();
        last_display_tick = now;
    }
}
