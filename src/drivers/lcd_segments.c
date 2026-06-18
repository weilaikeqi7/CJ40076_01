#include "lcd_segments.h"

#include "cs1622.h"

#include <stddef.h>
#include <string.h>

#define LCD_RAM_WORDS     64U
#define LCD_FIRST_SEG_PIN 9U
#define LCD_COM_COUNT     8U
#define LCD_FIRST_DIGIT_ID 1U
#define LCD_LAST_DIGIT_ID  27U
#define LCD_POINT(address, bit) { (uint8_t)(((address) / 2U) + LCD_FIRST_SEG_PIN), \
                                  (uint8_t)(((((address) & 1U) * 4U) + (bit)) + 1U) }

#define SEG_A (1U << 0U)
#define SEG_B (1U << 1U)
#define SEG_C (1U << 2U)
#define SEG_D (1U << 3U)
#define SEG_E (1U << 4U)
#define SEG_F (1U << 5U)
#define SEG_G (1U << 6U)

#define SEGMENT_COUNT 7U

typedef struct
{
    uint8_t lcd_pin;
    uint8_t lcd_com;
} LcdPoint;

typedef struct
{
    uint8_t symbol_id;
    LcdPoint point;
} LcdSymbolPoint;

static uint8_t g_ram[LCD_RAM_WORDS];

static const uint8_t g_digit_mask[10] = {
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,
    SEG_B | SEG_C,
    SEG_A | SEG_B | SEG_D | SEG_E | SEG_G,
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_G,
    SEG_B | SEG_C | SEG_F | SEG_G,
    SEG_A | SEG_C | SEG_D | SEG_F | SEG_G,
    SEG_A | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G,
    SEG_A | SEG_B | SEG_C,
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G,
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_F | SEG_G,
};

static const LcdPoint g_digit_points[LCD_LAST_DIGIT_ID + 1U][SEGMENT_COUNT] = {
    [1]  = { { 32U, 1U }, { 33U, 2U }, { 33U, 4U }, { 32U, 4U }, { 32U, 3U }, { 32U, 2U }, { 33U, 3U } },
    [2]  = { { 34U, 1U }, { 35U, 2U }, { 35U, 4U }, { 34U, 4U }, { 34U, 3U }, { 34U, 2U }, { 35U, 3U } },
    [3]  = { { 36U, 1U }, { 37U, 2U }, { 37U, 4U }, { 36U, 4U }, { 36U, 3U }, { 36U, 2U }, { 37U, 3U } },
    [4]  = { { 32U, 5U }, { 33U, 6U }, { 33U, 8U }, { 32U, 8U }, { 32U, 7U }, { 32U, 6U }, { 33U, 7U } },
    [5]  = { { 34U, 5U }, { 35U, 6U }, { 35U, 8U }, { 34U, 8U }, { 34U, 7U }, { 34U, 6U }, { 35U, 7U } },
    [6]  = { { 36U, 5U }, { 37U, 6U }, { 37U, 8U }, { 36U, 8U }, { 36U, 7U }, { 36U, 6U }, { 37U, 7U } },
    [7]  = { { 38U, 5U }, { 39U, 6U }, { 39U, 8U }, { 38U, 8U }, { 38U, 7U }, { 38U, 6U }, { 39U, 7U } },
    [8]  = { { 26U, 8U }, { 25U, 7U }, { 25U, 5U }, { 26U, 5U }, { 26U, 6U }, { 26U, 7U }, { 25U, 6U } },
    [9]  = { { 24U, 8U }, { 23U, 7U }, { 23U, 5U }, { 24U, 5U }, { 24U, 6U }, { 24U, 7U }, { 23U, 6U } },
    [10] = { { 22U, 8U }, { 21U, 7U }, { 21U, 5U }, { 22U, 5U }, { 22U, 6U }, { 22U, 7U }, { 21U, 6U } },
    [11] = { { 20U, 8U }, { 19U, 7U }, { 19U, 5U }, { 20U, 5U }, { 20U, 6U }, { 20U, 7U }, { 19U, 6U } },
    [12] = { { 18U, 8U }, { 17U, 7U }, { 17U, 5U }, { 18U, 5U }, { 18U, 6U }, { 18U, 7U }, { 17U, 6U } },
    [13] = { { 16U, 8U }, { 15U, 7U }, { 15U, 5U }, { 16U, 5U }, { 16U, 6U }, { 16U, 7U }, { 15U, 6U } },
    [14] = { { 14U, 8U }, { 13U, 7U }, { 13U, 5U }, { 14U, 5U }, { 14U, 6U }, { 14U, 7U }, { 13U, 6U } },
    [15] = { { 12U, 8U }, { 11U, 7U }, { 11U, 5U }, { 12U, 5U }, { 12U, 6U }, { 12U, 7U }, { 11U, 6U } },
    [16] = { { 10U, 8U }, { 9U,  7U }, { 9U,  5U }, { 10U, 5U }, { 10U, 6U }, { 10U, 7U }, { 9U,  6U } },
    [17] = { { 26U, 4U }, { 25U, 3U }, { 25U, 1U }, { 26U, 1U }, { 26U, 2U }, { 26U, 3U }, { 25U, 2U } },
    [18] = { { 24U, 4U }, { 23U, 3U }, { 23U, 1U }, { 24U, 1U }, { 24U, 2U }, { 24U, 3U }, { 23U, 2U } },
    [19] = { { 22U, 4U }, { 21U, 3U }, { 21U, 1U }, { 22U, 1U }, { 22U, 2U }, { 22U, 3U }, { 21U, 2U } },
    [20] = { { 20U, 4U }, { 19U, 3U }, { 19U, 1U }, { 20U, 1U }, { 20U, 2U }, { 20U, 3U }, { 19U, 2U } },
    [21] = { { 18U, 4U }, { 17U, 4U }, { 17U, 2U }, { 18U, 1U }, { 18U, 2U }, { 18U, 3U }, { 17U, 3U } },
    [22] = { { 16U, 4U }, { 15U, 4U }, { 15U, 2U }, { 16U, 1U }, { 16U, 2U }, { 16U, 3U }, { 15U, 3U } },
    [23] = { { 14U, 4U }, { 13U, 4U }, { 13U, 2U }, { 14U, 1U }, { 14U, 2U }, { 14U, 3U }, { 13U, 3U } },
    [24] = { { 12U, 4U }, { 11U, 4U }, { 11U, 2U }, { 12U, 1U }, { 12U, 2U }, { 12U, 3U }, { 11U, 3U } },
    [25] = { { 10U, 4U }, { 9U,  4U }, { 9U,  2U }, { 10U, 1U }, { 10U, 2U }, { 10U, 3U }, { 9U,  3U } },
    [26] = { { 28U, 5U }, { 29U, 6U }, { 29U, 8U }, { 28U, 8U }, { 28U, 7U }, { 28U, 6U }, { 29U, 7U } },
    [27] = { { 30U, 5U }, { 31U, 6U }, { 31U, 8U }, { 30U, 8U }, { 30U, 7U }, { 30U, 6U }, { 31U, 7U } },
};

static const LcdSymbolPoint g_symbol_points[] = {
    { LCD_SYMBOL_DIR_E,             LCD_POINT(38U, 2U) },
    { LCD_SYMBOL_DIR_S,             LCD_POINT(38U, 1U) },
    { LCD_SYMBOL_DIR_W,             LCD_POINT(38U, 0U) },
    { LCD_SYMBOL_DIR_N,             LCD_POINT(49U, 0U) },
    { LCD_SYMBOL_DIR_NE_E,          LCD_POINT(53U, 0U) },
    { LCD_SYMBOL_BATTERY_1,         LCD_POINT(58U, 3U) },
    { LCD_SYMBOL_BATTERY_2,         LCD_POINT(58U, 2U) },
    { LCD_SYMBOL_BATTERY_3,         LCD_POINT(60U, 2U) },
    { LCD_SYMBOL_BATTERY_4,         LCD_POINT(60U, 3U) },
    { LCD_SYMBOL_BATTERY_FRAME,     LCD_POINT(60U, 1U) },
    { LCD_SYMBOL_UNIT_M,            LCD_POINT(60U, 0U) },
    { LCD_SYMBOL_RANGE_SINGLE,      LCD_POINT(42U, 1U) },
    { LCD_SYMBOL_RANGE_CONTINUOUS,  LCD_POINT(44U, 1U) },
    { LCD_SYMBOL_RANGE_FIRST_F,     LCD_POINT(44U, 0U) },
    { LCD_SYMBOL_RANGE_LAST_E,      LCD_POINT(42U, 0U) },
    { LCD_SYMBOL_RETICLE,           LCD_POINT(44U, 3U) },
    { LCD_SYMBOL_AZIMUTH_DEG,       LCD_POINT(56U, 0U) },
    { LCD_SYMBOL_PITCH_LABEL_P,     LCD_POINT(41U, 0U) },
    { LCD_SYMBOL_PITCH_DEG,         LCD_POINT(45U, 0U) },
    { LCD_SYMBOL_PITCH_SIGN_MINUS,  LCD_POINT(38U, 3U) },
    { LCD_SYMBOL_PITCH_SIGN_PLUS,   LCD_POINT(40U, 3U) },
    { LCD_SYMBOL_LON_W,             LCD_POINT(37U, 0U) },
    { LCD_SYMBOL_LON_E,             LCD_POINT(37U, 2U) },
    { LCD_SYMBOL_LAT_S,             LCD_POINT(37U, 1U) },
    { LCD_SYMBOL_LAT_N,             LCD_POINT(37U, 3U) },
    { LCD_SYMBOL_COORD_LOCAL,       LCD_POINT(21U, 3U) },
    { LCD_SYMBOL_COORD_TARGET,      LCD_POINT(13U, 3U) },
    { LCD_SYMBOL_COORD_FIRST_F,     LCD_POINT(9U,  3U) },
    { LCD_SYMBOL_COORD_LAST_E,      LCD_POINT(5U,  3U) },
    { LCD_SYMBOL_COORD_DEG,         LCD_POINT(25U, 3U) },
    { LCD_SYMBOL_COORD_MIN,         LCD_POINT(17U, 3U) },
    { LCD_SYMBOL_COORD_SEC,         LCD_POINT(1U,  3U) },
    { LCD_SYMBOL_COORD_DOT,         LCD_POINT(4U,  0U) },
    { LCD_SYMBOL_ELEVATION_LABEL_H, LCD_POINT(32U, 3U) },
    { LCD_SYMBOL_ELEVATION_UNIT_M,  LCD_POINT(20U, 3U) },
};

static void set_point(LcdPoint point, bool on)
{
    uint8_t seg;
    uint8_t com;
    uint8_t address;
    uint8_t mask;

    if ((point.lcd_pin < LCD_FIRST_SEG_PIN) ||
        (point.lcd_com == 0U) ||
        (point.lcd_com > LCD_COM_COUNT))
    {
        return;
    }

    seg = (uint8_t)(point.lcd_pin - LCD_FIRST_SEG_PIN);
    com = (uint8_t)(point.lcd_com - 1U);
    address = (uint8_t)((seg * 2U) + (com / 4U));
    mask = (uint8_t)(1U << (com & 0x03U));

    if (address >= LCD_RAM_WORDS)
    {
        return;
    }

    if (on)
    {
        g_ram[address] = (uint8_t)(g_ram[address] | mask);
    }
    else
    {
        g_ram[address] = (uint8_t)(g_ram[address] & (uint8_t)~mask);
    }
}

void LcdSegments_Init(void)
{
    Cs1622_Init();
    LcdSegments_SetAll(false);
    LcdSegments_Flush();
}

void LcdSegments_ClearBuffer(void)
{
    (void)memset(g_ram, 0, sizeof(g_ram));
}

void LcdSegments_SetAll(bool on)
{
    LcdSegments_ClearBuffer();

    if (!on)
    {
        return;
    }

    for (uint8_t digit_id = LCD_FIRST_DIGIT_ID; digit_id <= LCD_LAST_DIGIT_ID; ++digit_id)
    {
        LcdSegments_SetDigit(digit_id, 8);
    }

    for (uint8_t i = 0U; i < (sizeof(g_symbol_points) / sizeof(g_symbol_points[0])); ++i)
    {
        set_point(g_symbol_points[i].point, true);
    }
}

void LcdSegments_SetDigit(uint8_t digit_id, int8_t value)
{
    const uint8_t segments[] = { SEG_A, SEG_B, SEG_C, SEG_D, SEG_E, SEG_F, SEG_G };
    uint8_t mask = 0U;

    if ((digit_id < LCD_FIRST_DIGIT_ID) || (digit_id > LCD_LAST_DIGIT_ID))
    {
        return;
    }

    if ((value >= 0) && (value <= 9))
    {
        mask = g_digit_mask[value];
    }

    for (uint8_t i = 0U; i < SEGMENT_COUNT; ++i)
    {
        set_point(g_digit_points[digit_id][i], (mask & segments[i]) != 0U);
    }
}

void LcdSegments_SetDash(uint8_t digit_id, bool on)
{
    if ((digit_id < LCD_FIRST_DIGIT_ID) || (digit_id > LCD_LAST_DIGIT_ID))
    {
        return;
    }

    for (uint8_t i = 0U; i < SEGMENT_COUNT; ++i)
    {
        set_point(g_digit_points[digit_id][i], false);
    }
    set_point(g_digit_points[digit_id][3U], on);
}

void LcdSegments_SetNumberRightAligned(const uint8_t* digit_ids, uint8_t digit_count, uint32_t value)
{
    bool force_one_digit = true;

    if ((digit_ids == 0) || (digit_count == 0U))
    {
        return;
    }

    for (int8_t i = (int8_t)(digit_count - 1U); i >= 0; --i)
    {
        const uint8_t digit = (uint8_t)(value % 10U);
        bool visible;

        value /= 10U;
        visible = (value != 0U) || (digit != 0U) || force_one_digit;
        LcdSegments_SetDigit(digit_ids[i], visible ? (int8_t)digit : -1);
        force_one_digit = false;
    }
}

void LcdSegments_SetSignedNumberRightAligned(const uint8_t* digit_ids, uint8_t digit_count, int32_t value)
{
    uint32_t absolute;

    if (value < 0)
    {
        absolute = (uint32_t)(-value);
    }
    else
    {
        absolute = (uint32_t)value;
    }

    LcdSegments_SetNumberRightAligned(digit_ids, digit_count, absolute);
}

void LcdSegments_SetSymbol(uint8_t symbol_id, bool on)
{
    for (uint8_t i = 0U; i < (sizeof(g_symbol_points) / sizeof(g_symbol_points[0])); ++i)
    {
        if (g_symbol_points[i].symbol_id == symbol_id)
        {
            set_point(g_symbol_points[i].point, on);
            return;
        }
    }
}

void LcdSegments_Flush(void)
{
    Cs1622_WriteRam(0U, g_ram, sizeof(g_ram));
}
