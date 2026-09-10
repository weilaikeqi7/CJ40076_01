/**
 * @file board.h
 * @brief CJ40076 V2.0 主板引脚定义与板级资源驱动（N32L403KBQ7 QFN32）
 *
 * 引脚分配（CJ40076-V2.0 原理图）：
 *   PA0  - 电池电压 ADC（ADC_CH_1，分压 VBAT = Vadc × 3/2）
 *   PA2/PA3  - USART2 TX/RX -> JY901B 姿态传感器（9600）
 *   PA4/PA5  - USART1 TX/RX -> GNSS 模块（115200）
 *   PA6  - 模式键（低有效）
 *   PA7  - 测距机电源开关（高有效）
 *   PA8  - JY901B 电源开关（高有效）
 *   PA9  - 电源键（低有效）
 *   PA10 - 电源保持（高有效，置低关机）
 *   PA11 - LCD 驱动芯片中断脚（输入，未用）
 *   PA12 - GNSS 电源开关（高有效）
 *   PA15 - LCD CS1622 /CS
 *   PB0/PB1  - UART4 TX/RX -> 测距机（115200）
 *   PB3  - LCD CS1622 /RD
 *   PB4/PB5  - 未用（原 UART5 调试串口已删除）
 *   PB6  - LCD CS1622 /WR
 *   PB7  - LCD CS1622 DATA
 *
 * 与 03 的差异：无显示屏电源、无加热丝、无 NTC 温度检测。
 */
#ifndef BOARD_H
#define BOARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "n32l40x.h"
#include <stdbool.h>
#include <stdint.h>

/* ------------------------------ 引脚定义 ------------------------------ */

/* 电池电压 ADC（分压比 3/2） */
#define BOARD_VBAT_ADC_PORT GPIOA
#define BOARD_VBAT_ADC_PIN  GPIO_PIN_0
#define BOARD_VBAT_ADC_CH   ADC_CH_1

/* 测距机电源开关，高有效 */
#define BOARD_PWR_RANGER_PORT GPIOA
#define BOARD_PWR_RANGER_PIN  GPIO_PIN_7

/* JY901B 电源开关，高有效 */
#define BOARD_PWR_JY901B_PORT GPIOA
#define BOARD_PWR_JY901B_PIN  GPIO_PIN_8

/* GNSS 电源开关，高有效 */
#define BOARD_PWR_GNSS_PORT GPIOA
#define BOARD_PWR_GNSS_PIN  GPIO_PIN_12

/* 电源保持，高有效（置低后系统掉电关机） */
#define BOARD_PWR_HOLD_PORT GPIOA
#define BOARD_PWR_HOLD_PIN  GPIO_PIN_10

/* 模式键，低有效 */
#define BOARD_KEY_MODE_PORT GPIOA
#define BOARD_KEY_MODE_PIN  GPIO_PIN_6

/* 电源键，低有效 */
#define BOARD_KEY_POWER_PORT GPIOA
#define BOARD_KEY_POWER_PIN GPIO_PIN_9

/* LCD 驱动芯片（CS1622）接口 */
#define BOARD_LCD_CS_PORT   GPIOA
#define BOARD_LCD_CS_PIN    GPIO_PIN_15
#define BOARD_LCD_RD_PORT   GPIOB
#define BOARD_LCD_RD_PIN    GPIO_PIN_3
#define BOARD_LCD_WR_PORT   GPIOB
#define BOARD_LCD_WR_PIN    GPIO_PIN_6
#define BOARD_LCD_DATA_PORT GPIOB
#define BOARD_LCD_DATA_PIN  GPIO_PIN_7
#define BOARD_LCD_IRQ_PORT  GPIOA
#define BOARD_LCD_IRQ_PIN   GPIO_PIN_11

/* ------------------------------ GPIO 驱动 ------------------------------ */

/**
 * @brief 电源保持控制。开机后必须立即置 true，否则松开电源键后掉电；
 *        置 false 切断整机电源（软关机）。
 */
void board_power_hold(bool on);

/**
 * @brief 初始化板上所有开关量 GPIO（电源开关、按键、电源保持）。
 *        上电默认：所有外设电源关闭。
 * @note  本函数内部会把电源保持脚置高（自动保持供电）。
 */
void board_gpio_init(void);

void board_ranger_power(bool on); /* 测距机电源 */
void board_jy901b_power(bool on); /* JY901B 电源 */
void board_gnss_power(bool on);   /* GNSS 电源 */

bool board_key_mode_pressed(void);  /* 模式键按下返回 true（低有效） */
bool board_key_power_pressed(void); /* 电源键按下返回 true（低有效） */

#ifdef __cplusplus
}
#endif

#endif /* BOARD_H */
