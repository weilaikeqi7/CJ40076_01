/**
 * @file board_adc.h
 * @brief 电池电压采样（ADC：PA0=CH1，分压 VBAT = Vadc × 3/2）
 *        与 03 相同 API；本板无 NTC 温度检测。
 */
#ifndef BOARD_ADC_H
#define BOARD_ADC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/** 电池分压比：VBAT = Vadc × 3/2 */
#define BOARD_VBAT_DIVIDER_NUM 3U
#define BOARD_VBAT_DIVIDER_DEN 2U

/** ADC 参考电压（VDD = 3.3V）与分辨率 */
#define BOARD_ADC_VREF_MV 3300U
#define BOARD_ADC_FULL    4096U

void board_adc_init(void);

/** 读取 ADC 原始值（0~4095） */
uint16_t board_adc_read_raw(uint8_t channel);

/** 电池电压，单位 mV */
uint32_t board_battery_mv(void);

#ifdef __cplusplus
}
#endif

#endif /* BOARD_ADC_H */
