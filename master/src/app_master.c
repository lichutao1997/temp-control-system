/**
 * @file    app_master.c
 * @brief   主机应用层逻辑实现
 *
 * 本模块实现值班室主机的核心功能:
 *   1. 按键扫描 — 4 个按键的消抖检测与事件分发
 *   2. LoRa 通信 — 接收从机数据、发送控制指令
 *   3. 界面显示 — 在 HUB12 点阵屏上显示温湿度、模式、状态
 *   4. 离线检测 — 10 秒无数据则标记从机离线
 *
 * 主循环调用流程:
 *   main() → App_Master_Init() → while(1) App_Master_Loop()
 *   App_Master_Loop() 内部按时间间隔分发各子任务
 */

#include "app_master.h"
#include "hub12.h"          /* HUB12 点阵屏驱动 (原 hub75) */
#include "key.h"
#include "sx1278.h"
#include "protocol.h"
#include <stdio.h>
#include <string.h>

/* ======================== 配置常量 ======================== */

/** @brief 按键扫描间隔 (ms) — 越小响应越快, 但占用 CPU 更多 */
#define KEY_SCAN_INTERVAL_MS    20

/** @brief 点阵屏刷新间隔 (ms) — 500ms 更新一次显示内容 */
#define DISPLAY_UPDATE_MS       500

/** @brief 从机离线超时 (ms) — 超过此时间未收到数据则标记离线 */
#define SLAVE_TIMEOUT_MS        10000

/** @brief 默认温度阈值 (°C) — 上电时的初始值 */
#define DEFAULT_THRESHOLD       30

/* ======================== 私有变量 ======================== */

/** @brief 主机状态 — 存储从机上报的数据和本地控制状态 */
static MasterState_t _state;

/** @brief LoRa 发送缓冲区 — 最大 23 字节 (帧头5 + 数据16 + 校验1 + 尾1) */
static uint8_t _tx_buf[FRAME_MAX_LEN];

/* ======================== SysTick 定时器 ======================== */

/**
 * @brief 毫秒级系统时钟计数器
 * @note  volatile 防止编译器优化, 在 SysTick ISR 中递增
 */
volatile uint32_t g_systick_ms = 0;

/**
 * @brief  SysTick 中断服务函数 — 每 1ms 触发一次
 * @note   SysTick_Config(SystemCoreClock/1000) 配置后自动调用
 */
void SysTick_Handler(void)
{
    g_systick_ms++;
}

/**
 * @brief  获取当前系统时间 (ms)
 * @retval 当前毫秒计数
 */
static uint32_t GetTick(void)
{
    return g_systick_ms;
}

/**
 * @brief  初始化 SysTick 为 1ms 中断
 */
static void SysTick_Init(void)
{
    SysTick_Config(SystemCoreClock / 1000);
}

/* ======================== LoRa 接收回调 ======================== */

/**
 * @brief  LoRa 数据接收完成回调 — 在 EXTI0 中断上下文中调用
 *
 * 处理流程:
 *   1. 调用 protocol_parse 解析帧 (校验帧头/帧尾/异或校验)
 *   2. 检查目标 ID 是否为本机 (DEV_ID_MASTER)
 *   3. 如果是 RPT_SENSOR_DATA 指令, 更新本地状态
 *
 * @param  data: 接收到的原始数据
 * @param  len:  数据长度
 * @param  rssi: 信号强度 (dBm), 本函数未使用但可用于显示
 */
static void OnRxFrame(uint8_t *data, uint8_t len, int16_t rssi)
{
    Frame_t frame;

    /* 帧解析: 检查帧头帧尾 + 异或校验 */
    if (!protocol_parse(data, len, &frame)) {
        return;  /* 帧无效, 丢弃 */
    }

    /* 只处理发给本机 (主机) 的帧 */
    if (frame.dst_id != DEV_ID_MASTER) {
        return;
    }

    /* 处理传感器数据上报 */
    if (frame.type == RPT_SENSOR_DATA && frame.len >= 7) {
        /**
         * RPT_SENSOR_DATA 数据布局 (7 字节):
         *   [0~1] temperature_x10 — 温度 × 10, 大端序
         *   [2~3] humidity_x10    — 湿度 × 10, 大端序
         *   [4]   fan_status      — 0x01=开, 0x00=关
         *   [5]   mode            — 0x01=自动, 0x00=手动
         *   [6]   threshold       — 温度阈值 (20~50)
         */
        SensorData_t *sd = (SensorData_t *)&frame.data[0];

        _state.temperature = (float)sd->temperature_x10 / 10.0f;
        _state.humidity    = (float)sd->humidity_x10 / 10.0f;
        _state.fan_on      = sd->fan_status;
        _state.mode        = sd->mode;
        _state.threshold   = sd->threshold;
        _state.slave_online = 1;
        _state.last_rx_tick = GetTick();
    }
}

/* ======================== 发送控制指令 ======================== */

/**
 * @brief  发送风扇开关指令到从机
 * @param  on: 1=开风扇, 0=关风扇
 */
static void SendFanControl(uint8_t on)
{
    uint8_t data = on ? 0x01 : 0x00;
    uint8_t len = protocol_pack(_tx_buf, DEV_ID_MASTER, DEV_ID_SLAVE,
                                CMD_FAN_CONTROL, &data, 1);
    if (len > 0) {
        SX1278_Send(_tx_buf, len);
    }
}

/**
 * @brief  发送模式切换指令到从机
 * @param  auto_mode: 1=自动模式, 0=手动模式
 */
static void SendSetMode(uint8_t auto_mode)
{
    uint8_t data = auto_mode ? 0x01 : 0x00;
    uint8_t len = protocol_pack(_tx_buf, DEV_ID_MASTER, DEV_ID_SLAVE,
                                CMD_SET_MODE, &data, 1);
    if (len > 0) {
        SX1278_Send(_tx_buf, len);
    }
}

/**
 * @brief  发送温度阈值设置指令到从机
 * @param  threshold: 温度阈值 (20~50°C)
 */
static void SendSetThreshold(uint8_t threshold)
{
    uint8_t data = threshold;
    uint8_t len = protocol_pack(_tx_buf, DEV_ID_MASTER, DEV_ID_SLAVE,
                                CMD_SET_THRESHOLD, &data, 1);
    if (len > 0) {
        SX1278_Send(_tx_buf, len);
    }
}

/* ======================== 按键处理 ======================== */

/**
 * @brief  按键事件处理 — 根据按下的按键执行对应操作
 *
 * 按键功能:
 *   KEY1 (PC13): 手动开关风扇 — 先切到手动模式, 再翻转风扇状态
 *   KEY2 (PA8):  切换自动/手动模式
 *   KEY3 (PB11): 温度阈值 +1°C (上限 50°C)
 *   KEY4 (PB10): 温度阈值 -1°C (下限 20°C)
 */
static void HandleKeys(void)
{
    uint8_t key = Key_GetEvent();
    if (key == KEY_NONE) return;

    switch (key) {
        case KEY_1:
            /**
             * KEY1: 手动开/关风扇
             * 如果当前是自动模式, 先切换到手动模式
             * 然后翻转风扇状态
             */
            if (_state.mode != 0x00) {
                SendSetMode(0);  /* 切换为手动模式 */
            }
            SendFanControl(!_state.fan_on);
            break;

        case KEY_2:
            /**
             * KEY2: 切换自动/手动模式
             * 自动模式: 从机根据温度阈值自动控制风扇
             * 手动模式: 通过主机按键远程控制风扇
             */
            if (_state.mode == MODE_AUTO) {
                SendSetMode(0);  /* 切换为手动 */
            } else {
                SendSetMode(1);  /* 切换为自动 */
            }
            break;

        case KEY_3:
            /**
             * KEY3: 温度阈值 +1°C
             * 范围限制: 20~50°C
             */
            if (_state.threshold < 50) {
                _state.threshold++;
                SendSetThreshold(_state.threshold);
            }
            break;

        case KEY_4:
            /**
             * KEY4: 温度阈值 -1°C
             * 范围限制: 20~50°C
             */
            if (_state.threshold > 20) {
                _state.threshold--;
                SendSetThreshold(_state.threshold);
            }
            break;
    }
}

/* ======================== 显示更新 ======================== */

/**
 * @brief  更新 HUB12 点阵屏显示内容
 *
 * 显示布局 (P10 屏 16 像素高, 分 3 行):
 *   第 0 行 (y=0~5):  温度 + 湿度
 *   第 1 行 (y=6~11): 运行模式 + 风扇状态
 *   第 2 行 (y=12~15): 当前温度阈值
 */
static void UpdateDisplay(void)
{
    char line_buf[32];  /* 显示文本缓冲区, P10 屏每行最多 ~5 个字符 */

    /* ---- 离线状态 ---- */
    if (!_state.slave_online) {
        HUB12_PrintLine(0, "ERR OFFLINE");
        HUB12_PrintLine(1, "CHECK SLAVE");
        HUB12_PrintLine(2, "");
        return;
    }

    /* ---- 第 0 行: 温度 + 湿度 ---- */
    snprintf(line_buf, sizeof(line_buf), "T:%.1fC H:%.0f%%",
             _state.temperature, _state.humidity);
    HUB12_PrintLine(0, line_buf);

    /* ---- 第 1 行: 模式 + 风扇状态 ---- */
    snprintf(line_buf, sizeof(line_buf), "%s FAN:%s",
             (_state.mode == MODE_AUTO) ? "AUTO" : "MANU",
             _state.fan_on ? "ON" : "OFF");
    HUB12_PrintLine(1, line_buf);

    /* ---- 第 2 行: 温度阈值 ---- */
    snprintf(line_buf, sizeof(line_buf), "THR:%dC", _state.threshold);
    HUB12_PrintLine(2, line_buf);
}

/* ======================== 公共函数 ======================== */

void App_Master_Init(void)
{
    /* 初始化状态变量 */
    memset(&_state, 0, sizeof(_state));
    _state.threshold    = DEFAULT_THRESHOLD;   /* 默认 30°C */
    _state.slave_online = 0;                   /* 初始标记离线 */

    /* ---- 1. SysTick 1ms 定时器 ---- */
    SysTick_Init();

    /* ---- 2. HUB12 点阵屏初始化 ---- */
    HUB12_Init();

    /* ---- 3. 按键初始化 ---- */
    Key_Init();

    /* ---- 4. LoRa 模块初始化 ---- */
    SX1278_Config_t lora_cfg = {
        .frequency        = 434000000,     /* 434 MHz */
        .spreading_factor = 9,             /* SF9 — 兼顾距离和速率 */
        .bandwidth        = 7,             /* 125 kHz */
        .coding_rate      = 1,             /* 4/5 */
        .tx_power         = 17,            /* 17 dBm */
        .sync_word        = SX1278_SYNC_WORD,
        .preamble_len     = SX1278_PREAMBLE_LENGTH,
    };

    if (SX1278_Init(&lora_cfg) != SX1278_OK) {
        /**
         * LoRa 初始化失败 — 常见原因:
         *   1. SPI 接线错误 (PA4~PA7)
         *   2. SX1278 模块未供电
         *   3. 模块损坏
         * 此处死循环, 可改为 LED 闪烁指示错误
         */
        while (1);
    }

    /* 注册接收回调并进入接收模式 */
    SX1278_SetRxCallback(OnRxFrame);
    SX1278_StartRx();

    /* ---- 5. 初始显示 ---- */
    HUB12_PrintLine(0, "TEMP CTRL");
    HUB12_PrintLine(1, "V2.0");
    HUB12_PrintLine(2, "WAIT...");
}

void App_Master_Loop(void)
{
    uint32_t now = GetTick();

    /* 静态变量记录上次执行时间, 避免重复调用 */
    static uint32_t last_key_tick = 0;
    static uint32_t last_display_tick = 0;

    /* ---- 按键扫描 (每 20ms) ---- */
    if (now - last_key_tick >= KEY_SCAN_INTERVAL_MS) {
        HandleKeys();
        last_key_tick = now;
    }

    /* ---- 从机在线检测 ---- */
    /**
     * 如果从机之前在线, 但超过 SLAVE_TIMEOUT_MS 没有收到数据,
     * 则标记从机离线。下次收到数据时自动恢复在线状态。
     */
    if (_state.slave_online && (now - _state.last_rx_tick > SLAVE_TIMEOUT_MS)) {
        _state.slave_online = 0;
    }

    /* ---- 显示更新 (每 500ms) ---- */
    if (now - last_display_tick >= DISPLAY_UPDATE_MS) {
        UpdateDisplay();
        last_display_tick = now;
    }
}
