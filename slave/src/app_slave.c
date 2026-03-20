/**
 * @file    app_slave.c
 * @brief   从机应用层逻辑实现
 *
 * ===================== 从机核心逻辑 =====================
 *
 * 从机在 while(1) 主循环中执行以下任务:
 *
 *   1. 传感器读取 (每 2000ms)
 *      - 发送 DS18B20 温度转换命令 (非阻塞)
 *      - 等待 750ms (12 位精度转换时间)
 *      - 读取 DS18B20 温度值
 *      - 读取 DHT22 湿度值
 *
 *   2. 自动温控
 *      - 仅在 MODE_AUTO 模式下生效
 *      - 温度 >= 阈值 → 开风扇
 *      - 温度 <= 阈值 - 1°C → 关风扇 (回差控制, 防止频繁切换)
 *
 *   3. LoRa 数据上报 (每 3000ms)
 *      - 打包传感器数据为协议帧
 *      - 通过 SX1278 发送
 *
 *   4. LoRa 指令接收 (中断驱动)
 *      - CMD_FAN_CONTROL:  开/关风扇 (进入手动模式)
 *      - CMD_SET_THRESHOLD: 修改温度阈值
 *      - CMD_SET_MODE:      切换自动/手动模式
 */

#include "app_slave.h"
#include "ds18b20.h"
#include "dht22.h"
#include "relay.h"
#include "sx1278.h"
#include "protocol.h"

/* ======================== 配置常量 ======================== */

/**
 * @brief 传感器读取间隔 (ms)
 * DS18B20 需要 750ms 转换时间, 总周期 = 2000ms (含等待)
 */
#define SENSOR_READ_INTERVAL_MS     2000

/**
 * @brief LoRa 数据上报间隔 (ms)
 * 主机离线检测超时为 10000ms, 上报间隔 3000ms 有足够冗余
 */
#define REPORT_INTERVAL_MS          3000

/**
 * @brief 温度回差 (°C)
 *
 * 回差控制原理:
 *   假设阈值 = 30°C, 回差 = 1°C:
 *   - 温度升到 30°C → 开风扇
 *   - 温度降到 29°C → 关风扇
 *
 *   如果没有回差, 温度在 29.9~30.0°C 波动时风扇会频繁开关,
 *   损坏继电器触点。1°C 回差确保风扇状态稳定。
 */
#define TEMP_HYSTERESIS             1.0f

/**
 * @brief 默认温度阈值 (°C)
 */
#define DEFAULT_THRESHOLD           30

/* ======================== 私有变量 ======================== */

/** @brief 从机运行状态 */
static SlaveState_t _state;

/** @brief LoRa 发送缓冲区 */
static uint8_t _tx_buf[FRAME_MAX_LEN];

/* ======================== SysTick 定时器 ======================== */

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

/* ======================== LoRa 接收回调 ======================== */

/**
 * @brief  LoRa 数据接收回调 — 在 EXTI0 中断中调用
 *
 * 解析帧并执行对应的控制指令:
 *   - CMD_FAN_CONTROL (0x01): data[0]=0x01→开, 0x00→关
 *   - CMD_SET_THRESHOLD (0x02): data[0]=阈值 (20~50)
 *   - CMD_SET_MODE (0x03): data[0]=0x01→自动, 0x00→手动
 */
static void OnRxFrame(uint8_t *data, uint8_t len, int16_t rssi)
{
    Frame_t frame;

    /* 帧校验: 帧头 0xAA + 帧尾 0x55 + 异或校验 */
    if (!protocol_parse(data, len, &frame)) {
        return;  /* 校验失败, 丢弃 */
    }

    /* 只处理发给本机 (从机) 的帧 */
    if (frame.dst_id != DEV_ID_SLAVE) {
        return;
    }

    switch (frame.type) {
        case CMD_FAN_CONTROL:
            /**
             * 风扇控制指令 — 主机 KEY1 触发
             * data[0]: 0x01=开风扇, 0x00=关风扇
             * 同时切换为手动模式
             */
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
            /**
             * 温度阈值设置指令 — 主机 KEY3/KEY4 触发
             * data[0]: 阈值 (20~50°C)
             */
            if (frame.len >= 1 && frame.data[0] >= 20 && frame.data[0] <= 50) {
                _state.threshold = frame.data[0];
            }
            break;

        case CMD_SET_MODE:
            /**
             * 模式切换指令 — 主机 KEY2 触发
             * data[0]: 0x01=自动模式, 0x00=手动模式
             */
            if (frame.len >= 1) {
                _state.mode = frame.data[0] ? MODE_AUTO : MODE_MANUAL;
            }
            break;
    }
}

/* ======================== 自动温控逻辑 ======================== */

/**
 * @brief  自动温控 — 仅在 MODE_AUTO 模式下执行
 *
 * 控制逻辑 (带回差):
 *   if (温度 >= 阈值)          → 开风扇 (如果未开)
 *   if (温度 <= 阈值 - 1°C)    → 关风扇 (如果已开)
 *   温度在 (阈值-1°C, 阈值) 之间 → 保持当前状态不变
 *
 * 例如: 阈值=30°C, 回差=1°C
 *   温度从 29°C 升到 30°C  → 开风扇
 *   温度从 30°C 降到 29.5°C → 保持开 (因为 29.5 > 29)
 *   温度降到 29°C          → 关风扇
 */
static void AutoFanControl(void)
{
    if (_state.mode != MODE_AUTO) {
        return;  /* 手动模式不执行自动控制 */
    }

    /* 温度超过阈值 → 开风扇 */
    if (_state.temperature >= (float)_state.threshold) {
        if (!_state.fan_on) {
            Relay_On();
            _state.fan_on = 1;
        }
    }
    /* 温度低于 (阈值 - 回差) → 关风扇 */
    else if (_state.temperature <= ((float)_state.threshold - TEMP_HYSTERESIS)) {
        if (_state.fan_on) {
            Relay_Off();
            _state.fan_on = 0;
        }
    }
    /* 在回差区间内 → 保持当前状态 */
}

/* ======================== 数据上报 ======================== */

/**
 * @brief  组装并发送传感器数据帧
 *
 * 数据格式 (7 字节):
 *   [0~1] temperature_x10 (大端序)
 *   [2~3] humidity_x10    (大端序)
 *   [4]   fan_status
 *   [5]   mode
 *   [6]   threshold
 */
static void ReportSensorData(void)
{
    SensorData_t sdata;

    /* 温度 × 10 转整数 (如 28.5°C → 285) */
    sdata.temperature_x10 = (uint16_t)(_state.temperature * 10);

    /* 湿度 × 10 转整数 (如 65.2% → 652) */
    sdata.humidity_x10    = (uint16_t)(_state.humidity * 10);

    sdata.fan_status = _state.fan_on;
    sdata.mode       = _state.mode;
    sdata.threshold  = _state.threshold;

    /* 打包帧并发送 */
    uint8_t len = protocol_pack_sensor(_tx_buf, &sdata);
    if (len > 0) {
        SX1278_Send(_tx_buf, len);
    }
}

/* ======================== 公共函数 ======================== */

void App_Slave_Init(void)
{
    /* ---- 1. 状态初始化 ---- */
    _state.temperature      = 0;
    _state.humidity         = 0;
    _state.fan_on           = 0;               /* 风扇默认关闭 */
    _state.mode             = MODE_AUTO;       /* 默认自动模式 */
    _state.threshold        = DEFAULT_THRESHOLD;  /* 默认 30°C */
    _state.last_report_tick = 0;

    /* ---- 2. SysTick 1ms 定时器 ---- */
    SysTick_Init();

    /* ---- 3. 外设初始化 ---- */
    DS18B20_Init();   /* 温度传感器 (PA0, 单总线) */
    DHT22_Init();     /* 温湿度传感器 (PA1, 单总线) */
    Relay_Init();     /* 继电器 (PB1, 默认关闭) */

    /* ---- 4. LoRa 初始化 ---- */
    SX1278_Config_t lora_cfg = {
        .frequency        = 434000000,     /* 434 MHz */
        .spreading_factor = 9,             /* SF9 */
        .bandwidth        = 7,             /* 125 kHz */
        .coding_rate      = 1,             /* 4/5 */
        .tx_power         = 17,            /* 17 dBm */
        .sync_word        = SX1278_SYNC_WORD,
        .preamble_len     = SX1278_PREAMBLE_LENGTH,
    };

    if (SX1278_Init(&lora_cfg) != SX1278_OK) {
        /**
         * LoRa 初始化失败 — 常见原因:
         *   1. SPI 接线错误 (检查 PA4=NSS, PA5=SCK, PA6=MISO, PA7=MOSI)
         *   2. SX1278 未供电或损坏
         *   3. 未接天线 (虽然初始化不需要天线, 但发射时必须接)
         *
         * 此处死循环等待, 实际项目中可用 LED 闪烁指示错误
         */
        while (1);
    }

    /* 注册接收回调 — 收到数据时自动调用 OnRxFrame */
    SX1278_SetRxCallback(OnRxFrame);

    /* ---- 5. 启动首次温度转换 ---- */
    DS18B20_StartConvert();  /* 非阻塞, 约 750ms 后可读取 */
}

void App_Slave_Loop(void)
{
    uint32_t now = GetTick();

    /* 静态变量 — 保持上次执行状态 */
    static uint32_t last_sensor_tick = 0;
    static uint8_t  ds18b20_converting = 0;

    /* ==================== 传感器读取 ==================== */

    /**
     * DS18B20 温度读取流程 (非阻塞方式):
     *   1. 到达读取间隔 (2000ms) 时, 发送转换命令
     *   2. 标记正在转换 (ds18b20_converting = 1)
     *   3. 等待 750ms (转换时间)
     *   4. 读取温度值
     *   5. 同时读取 DHT22 湿度
     *   6. 执行自动温控
     *   7. 清除转换标记
     */

    /* 步骤 1~2: 发起转换 */
    if (!ds18b20_converting && (now - last_sensor_tick >= SENSOR_READ_INTERVAL_MS)) {
        DS18B20_StartConvert();          /* 发送 Skip ROM + Convert T */
        ds18b20_converting = 1;
        last_sensor_tick = now;
    }

    /* 步骤 3~7: 等待转换完成并读取 */
    if (ds18b20_converting && (now - last_sensor_tick >= 750)) {
        /* 读取 DS18B20 温度 */
        _state.temperature = DS18B20_ReadTemp();

        /* 读取 DHT22 湿度 (DHT22 同时也有温度, 但以 DS18B20 为主) */
        DHT22_Data_t dht;
        if (DHT22_Read(&dht)) {
            _state.humidity = dht.humidity;
        }

        ds18b20_converting = 0;

        /* 执行自动温控判断 */
        AutoFanControl();
    }

    /* ==================== 数据上报 ==================== */

    /**
     * 每 3 秒打包并发送一次传感器数据。
     * 主机离线检测超时 10 秒, 3 秒间隔留有足够冗余。
     */
    if (now - _state.last_report_tick >= REPORT_INTERVAL_MS) {
        ReportSensorData();
        _state.last_report_tick = now;
    }

    /**
     * 注意: LoRa 接收通过 EXTI0 中断异步处理,
     * 收到数据后自动调用 OnRxFrame 回调, 无需在主循环中轮询。
     */
}

const SlaveState_t* App_Slave_GetState(void)
{
    return &_state;
}
