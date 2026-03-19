/**
 * @file    app_slave.c
 * @brief   从机应用层逻辑实现
 */

#include "app_slave.h"
#include "ds18b20.h"
#include "dht22.h"
#include "relay.h"
#include "sx1278.h"
#include "protocol.h"

/* ==================== 配置常量 ==================== */
#define SENSOR_READ_INTERVAL_MS     2000    /* 传感器读取间隔 */
#define REPORT_INTERVAL_MS          3000    /* 数据上报间隔 */
#define TEMP_HYSTERESIS             1.0f    /* 温度回差 (°C) */
#define DEFAULT_THRESHOLD           30      /* 默认温度阈值 */

/* ==================== 私有变量 ==================== */
static SlaveState_t _state;
static uint8_t _tx_buf[FRAME_MAX_LEN];

/* ==================== 定时器 (SysTick 1ms) ==================== */
static volatile uint32_t _systick_ms = 0;

void SysTick_Handler(void)
{
    _systick_ms++;
}

static uint32_t GetTick(void)
{
    return _systick_ms;
}

static void SysTick_Init(void)
{
    SysTick_Config(SystemCoreClock / 1000);  /* 1ms 中断 */
}

/* ==================== 接收回调 ==================== */
static void OnRxFrame(uint8_t *data, uint8_t len, int16_t rssi)
{
    Frame_t frame;
    if (!protocol_parse(data, len, &frame)) {
        return;  /* 帧校验失败 */
    }

    /* 只处理发给本机的帧 */
    if (frame.dst_id != DEV_ID_SLAVE) {
        return;
    }

    switch (frame.type) {
        case CMD_FAN_CONTROL:
            if (frame.len >= 1) {
                if (frame.data[0]) {
                    Relay_On();
                    _state.fan_on = 1;
                } else {
                    Relay_Off();
                    _state.fan_on = 0;
                }
                _state.mode = MODE_MANUAL;
            }
            break;

        case CMD_SET_THRESHOLD:
            if (frame.len >= 1 && frame.data[0] >= 20 && frame.data[0] <= 50) {
                _state.threshold = frame.data[0];
            }
            break;

        case CMD_SET_MODE:
            if (frame.len >= 1) {
                _state.mode = frame.data[0] ? MODE_AUTO : MODE_MANUAL;
                /* 切换到手动模式时关闭风扇 (由主机决定) */
                if (_state.mode == MODE_MANUAL && !frame.data[0]) {
                    /* 保持当前状态 */
                }
            }
            break;
    }
}

/* ==================== 自动温控逻辑 ==================== */
static void AutoFanControl(void)
{
    if (_state.mode != MODE_AUTO) {
        return;
    }

    /* 带回差的温度控制: 高于阈值开, 低于阈值-回差关 */
    if (_state.temperature >= (float)_state.threshold) {
        if (!_state.fan_on) {
            Relay_On();
            _state.fan_on = 1;
        }
    } else if (_state.temperature <= ((float)_state.threshold - TEMP_HYSTERESIS)) {
        if (_state.fan_on) {
            Relay_Off();
            _state.fan_on = 0;
        }
    }
}

/* ==================== 数据上报 ==================== */
static void ReportSensorData(void)
{
    SensorData_t sdata;
    sdata.temperature_x10 = (uint16_t)(_state.temperature * 10);
    sdata.humidity_x10 = (uint16_t)(_state.humidity * 10);
    sdata.fan_status = _state.fan_on;
    sdata.mode = _state.mode;
    sdata.threshold = _state.threshold;

    uint8_t len = protocol_pack_sensor(_tx_buf, &sdata);
    if (len > 0) {
        SX1278_Send(_tx_buf, len);
    }
}

/* ==================== 公共函数 ==================== */
void App_Slave_Init(void)
{
    /* 初始化状态 */
    _state.temperature = 0;
    _state.humidity = 0;
    _state.fan_on = 0;
    _state.mode = MODE_AUTO;
    _state.threshold = DEFAULT_THRESHOLD;
    _state.last_report_tick = 0;

    /* SysTick */
    SysTick_Init();

    /* 外设初始化 */
    DS18B20_Init();
    DHT22_Init();
    Relay_Init();

    /* LoRa 初始化 */
    SX1278_Config_t lora_cfg = {
        .frequency = 434000000,
        .spreading_factor = 9,
        .bandwidth = 7,         /* 125 kHz */
        .coding_rate = 1,       /* 4/5 */
        .tx_power = 17,
        .sync_word = SX1278_SYNC_WORD,
        .preamble_len = SX1278_PREAMBLE_LENGTH,
    };

    if (SX1278_Init(&lora_cfg) != SX1278_OK) {
        /* LoRa 初始化失败, 可在此添加错误指示 */
        while (1);
    }

    SX1278_SetRxCallback(OnRxFrame);

    /* 启动首次温度转换 */
    DS18B20_StartConvert();
}

void App_Slave_Loop(void)
{
    uint32_t now = GetTick();
    static uint32_t last_sensor_tick = 0;
    static uint8_t ds18b20_converting = 0;

    /* ---- 传感器读取 ---- */
    /* DS18B20 需要 750ms 转换时间, 先启动转换再读取 */
    if (!ds18b20_converting && (now - last_sensor_tick >= SENSOR_READ_INTERVAL_MS)) {
        DS18B20_StartConvert();
        ds18b20_converting = 1;
        last_sensor_tick = now;
    }

    if (ds18b20_converting && (now - last_sensor_tick >= 750)) {
        _state.temperature = DS18B20_ReadTemp();

        /* 读取 DHT22 湿度 (同时也有温度, 取 DS18B20 为主) */
        DHT22_Data_t dht;
        if (DHT22_Read(&dht)) {
            _state.humidity = dht.humidity;
        }

        ds18b20_converting = 0;

        /* 自动温控 */
        AutoFanControl();
    }

    /* ---- 数据上报 ---- */
    if (now - _state.last_report_tick >= REPORT_INTERVAL_MS) {
        ReportSensorData();
        _state.last_report_tick = now;
    }
}

const SlaveState_t* App_Slave_GetState(void)
{
    return &_state;
}
