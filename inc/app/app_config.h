/**
 * @file app_config.h
 * @brief CJ40076 V2.0（N32L403KBQ7）业务层全局配置
 *
 * 业务规格与 CJ40076_03（V3/N32G4FR）一致，差异：
 *   - 无显示屏电源引脚（LCD 常供电）
 *   - 无加热丝、无 NTC 温度检测
 *   - 显示屏为 CS1622 驱动的 COM×SEG 玻璃：距离/航向/俯仰/高程无小数段，
 *     按整数显示；坐标行 DDD°MM′SS.ss″（唯一带小数点段 T46）
 *   - 计数显示 5 位数码管（21~25），上限沿用 9999
 */
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#define APP_FIRMWARE_NAME "CJ40076"
#define APP_HARDWARE_VER  "CJ40076-V2.0"
#define APP_SOFTWARE_VER  "0.2.0"

/* ------------------------------ 按键 ------------------------------ */
#define APP_KEY_SCAN_MS        10U
#define APP_KEY_DEBOUNCE_MS    40U   /* 沿用 V2.0 硬件调试值 */
#define APP_KEY_MULTICLICK_MS  600U
#define APP_KEY_LONG_MS        3000U
#define APP_KEY_BOTH_LONG_MS   1000U
#define APP_KEY_REPEAT_MS      100U

/* ------------------------------ 测距 ------------------------------ */
#define APP_MEASURE_SILENCE_MS    200U   /* 帧间静默判定一轮结束（统一 200ms） */
#define APP_MEASURE_TIMEOUT_MS    3000U
#define APP_MEASURE_CONT_PERIOD_MS 8000U
#define APP_MEASURE_TEST_PERIOD_MS 12000U

/* ------------------------------ 姿态/罗盘 ------------------------------ */
/** 默认补偿（0.01 度）：PIt=0.00°，HIt=+90.00°，HEr=0.00° */
#define APP_DEFAULT_PIT_C01 0
#define APP_DEFAULT_HIT_C01 9000
#define APP_DEFAULT_HER_C01 0

#define APP_PIT_MAX_C01 9000
#define APP_HIT_MAX_C01 18000
#define APP_HER_MAX_C01 18000

#define APP_IMU_TIMEOUT_MS  500U
#define APP_GNSS_TIMEOUT_MS 2500U

/* ------------------------------ 电池 ------------------------------ */
/** 电池分压：VBAT = Vadc × 3/2（board_config.h） */
#define APP_BATT_LVL4_MV 3800U
#define APP_BATT_LVL3_MV 3700U
#define APP_BATT_LVL2_MV 3600U
#define APP_BATT_LOW_OFF_MV 2600U
#define APP_BATT_CHECK_MS 500U

/* ------------------------------ 计数 ------------------------------ */
#define APP_COUNT_MAX 9999U

/* ------------------------------ 显示交替周期 ------------------------------ */
#define APP_DISP_FE_TOGGLE_FAST_MS 1000U
#define APP_DISP_FE_TOGGLE_SLOW_MS 2000U
#define APP_DISP_LL_TOGGLE_MS     1000U
#define APP_DISP_RENDER_MS        100U

#endif /* APP_CONFIG_H */
