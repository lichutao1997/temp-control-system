/**
 * @file    main.c
 * @brief   设备房从机 - 主程序入口
 */

#include "stm32f10x.h"
#include "app_slave.h"

int main(void)
{
    /* 系统时钟初始化 (默认 72MHz HSE) */
    SystemInit();

    /* 从机应用初始化 */
    App_Slave_Init();

    /* 主循环 */
    while (1) {
        App_Slave_Loop();
    }
}
