/**
 * @file app.h
 * @brief CJ40076 V2.0（N32L40x）应用主状态机
 *
 * 业务与 CJ40076_03（V3）一致；硬件差异：
 *   - 无显示屏电源（LCD 常供电）、无加热丝、无 NTC 温度
 *   - 测距机 UART4 常供电；IMU/GNSS 按需供电
 */
#ifndef APP_H
#define APP_H

#ifdef __cplusplus
extern "C" {
#endif

/** 应用入口（FreeRTOS 任务函数，内部不返回） */
void app_run(void* argument);

#ifdef __cplusplus
}
#endif

#endif /* APP_H */
