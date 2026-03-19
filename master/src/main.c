/**
 * @file    main.c
 * @brief   值班室主机 - 主程序入口
 */

#include "stm32f10x.h"
#include "app_master.h"

int main(void)
{
    /* 系统时钟初始化 (默认 72MHz HSE) */
    SystemInit();

    /* 主机应用初始化 */
    App_Master_Init();

    /* 主循环 */
    while (1) {
        App_Master_Loop();
    }
}
