#ifndef LCD_SEGMENTS_H
#define LCD_SEGMENTS_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    LCD_SYMBOL_DIR_E = 1,
    LCD_SYMBOL_DIR_S,
    LCD_SYMBOL_DIR_W,
    LCD_SYMBOL_DIR_N,
    LCD_SYMBOL_DIR_NE_E,
    LCD_SYMBOL_BATTERY_1,
    LCD_SYMBOL_BATTERY_2,
    LCD_SYMBOL_BATTERY_3,
    LCD_SYMBOL_BATTERY_4,
    LCD_SYMBOL_BATTERY_5,
    LCD_SYMBOL_BATTERY_FRAME,
    LCD_SYMBOL_UNIT_M,
    LCD_SYMBOL_RANGE_SINGLE,
    LCD_SYMBOL_RANGE_CONTINUOUS,
    LCD_SYMBOL_RANGE_FIRST_F,
    LCD_SYMBOL_RANGE_LAST_E,
    LCD_SYMBOL_RETICLE,
    LCD_SYMBOL_AZIMUTH_DEG,
    LCD_SYMBOL_PITCH_LABEL_P,
    LCD_SYMBOL_PITCH_DEG,
    LCD_SYMBOL_PITCH_SIGN_MINUS,
    LCD_SYMBOL_PITCH_SIGN_PLUS,
    LCD_SYMBOL_LON_W,
    LCD_SYMBOL_LON_E,
    LCD_SYMBOL_LAT_S,
    LCD_SYMBOL_LAT_N,
    LCD_SYMBOL_COORD_LOCAL,
    LCD_SYMBOL_COORD_TARGET,
    LCD_SYMBOL_COORD_FIRST_F,
    LCD_SYMBOL_COORD_LAST_E,
    LCD_SYMBOL_COORD_DEG,
    LCD_SYMBOL_COORD_MIN,
    LCD_SYMBOL_COORD_SEC,
    LCD_SYMBOL_COORD_DOT,
    LCD_SYMBOL_ELEVATION_LABEL_H,
    LCD_SYMBOL_ELEVATION_UNIT_M
} LcdSymbolId;

void LcdSegments_Init(void);
void LcdSegments_ClearBuffer(void);
void LcdSegments_SetAll(bool on);
void LcdSegments_SetDigit(uint8_t digit_id, int8_t value);
void LcdSegments_SetDash(uint8_t digit_id, bool on);
void LcdSegments_SetNumberRightAligned(const uint8_t* digit_ids, uint8_t digit_count, uint32_t value);
void LcdSegments_SetSignedNumberRightAligned(const uint8_t* digit_ids, uint8_t digit_count, int32_t value);
void LcdSegments_SetSymbol(uint8_t symbol_id, bool on);
void LcdSegments_Flush(void);

#endif /* LCD_SEGMENTS_H */
