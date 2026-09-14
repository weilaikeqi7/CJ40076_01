/**
 * @file app_display.c
 * @brief LCD 业务渲染实现（CS1622 玻璃，经 LcdSegments 层）
 */
#include "app_display.h"

#include "app_config.h"
#include "lcd_segments.h"

#include <string.h>

/* 数码管位置组 */
static const uint8_t digits_heading[]  = {1, 2, 3};          /* 航向 0~359 */
static const uint8_t digits_distance[] = {4, 5, 6, 7};       /* 距离整数米 */
static const uint8_t digits_elev[]     = {17, 18, 19, 20};   /* 高程整数米 */
static const uint8_t digits_count[]    = {21, 22, 23, 24, 25}; /* 计数 */

/** 横杠填充 */
static void draw_dashes(const uint8_t* ids, uint8_t count)
{
    for (uint8_t i = 0U; i < count; i++)
    {
        LcdSegments_SetDash(ids[i], true);
    }
}

/** 8 方向罗盘字母（T1=E T2=S T3=W T4=N T5=NE_E） */
static void draw_compass(int32_t heading_c01, bool valid)
{
    static const struct
    {
        uint8_t a;
        uint8_t b;
    } dir_map[8] = {
        {LCD_SYMBOL_DIR_N, 0},                        /* N  */
        {LCD_SYMBOL_DIR_N, LCD_SYMBOL_DIR_NE_E},      /* NE */
        {LCD_SYMBOL_DIR_NE_E, 0},                     /* E（用右侧 E） */
        {LCD_SYMBOL_DIR_S, LCD_SYMBOL_DIR_NE_E},      /* SE */
        {LCD_SYMBOL_DIR_S, 0},                        /* S  */
        {LCD_SYMBOL_DIR_S, LCD_SYMBOL_DIR_W},         /* SW */
        {LCD_SYMBOL_DIR_W, 0},                        /* W  */
        {LCD_SYMBOL_DIR_N, LCD_SYMBOL_DIR_W},         /* NW */
    };
    uint8_t idx;

    LcdSegments_SetSymbol(LCD_SYMBOL_DIR_E, false);
    LcdSegments_SetSymbol(LCD_SYMBOL_DIR_S, false);
    LcdSegments_SetSymbol(LCD_SYMBOL_DIR_W, false);
    LcdSegments_SetSymbol(LCD_SYMBOL_DIR_N, false);
    LcdSegments_SetSymbol(LCD_SYMBOL_DIR_NE_E, false);

    if (!valid)
    {
        return;
    }

    idx = (uint8_t)(((heading_c01 + 2250) % APP_HEADING_PERIOD_C01) / 4500);
    LcdSegments_SetSymbol(dir_map[idx].a, true);
    if (dir_map[idx].b != 0U)
    {
        LcdSegments_SetSymbol(dir_map[idx].b, true);
    }
}

/**
 * @brief LCD 俯仰显示映射：±85.00°~±88.00°线性映射为±85.00°~±90.00°。
 *        只改变显示值，不改变坐标和高程解算使用的姿态值。
 */
static int32_t pitch_display_map(int32_t pitch_c01)
{
    const int32_t start_c01 = APP_PIT_DISPLAY_MAP_START_C01;
    const int32_t end_c01   = APP_PIT_DISPLAY_MAP_END_C01;
    const int32_t max_c01   = APP_PIT_MAX_C01;
    bool          negative  = pitch_c01 < 0;
    int32_t       abs_c01   = negative ? -pitch_c01 : pitch_c01;
    int32_t       mapped_c01;

    if (abs_c01 <= start_c01)
    {
        return pitch_c01;
    }
    if (abs_c01 >= end_c01)
    {
        return negative ? -max_c01 : max_c01;
    }

    mapped_c01 = start_c01 +
                 ((abs_c01 - start_c01) * (max_c01 - start_c01) +
                  (end_c01 - start_c01) / 2) /
                     (end_c01 - start_c01);
    return negative ? -mapped_c01 : mapped_c01;
}

/** 中行俯仰：±XX°（整数，digits 26 27，负号/正号，P 与 °） */
static void draw_pitch(int32_t pitch_c01, bool valid)
{
    uint32_t abs_deg;

    LcdSegments_SetSymbol(LCD_SYMBOL_PITCH_LABEL_P, valid);
    LcdSegments_SetSymbol(LCD_SYMBOL_PITCH_DEG, valid);

    if (!valid)
    {
        LcdSegments_SetDigit(26, -1);
        LcdSegments_SetDigit(27, -1);
        LcdSegments_SetSymbol(LCD_SYMBOL_PITCH_SIGN_MINUS, false);
        LcdSegments_SetSymbol(LCD_SYMBOL_PITCH_SIGN_PLUS, false);
        return;
    }

    pitch_c01 = pitch_display_map(pitch_c01);
    abs_deg = (uint32_t)(pitch_c01 < 0 ? -pitch_c01 : pitch_c01) / 100U;
    if (abs_deg > 99U)
    {
        abs_deg = 99U; /* 两位显示上限 */
    }
    LcdSegments_SetDigit(26, abs_deg >= 10U ? (int8_t)(abs_deg / 10U) : -1);
    LcdSegments_SetDigit(27, (int8_t)(abs_deg % 10U));
    LcdSegments_SetSymbol(LCD_SYMBOL_PITCH_SIGN_MINUS, pitch_c01 < 0);
    LcdSegments_SetSymbol(LCD_SYMBOL_PITCH_SIGN_PLUS, pitch_c01 > 0);
}

/** 大字行坐标：DDD° MM′ SS.ss″（digits 8~16），经纬度/半球指示 */
static void draw_coord(double deg, bool is_lon, bool valid)
{
    uint32_t deg_i, min_i, sec_x100;
    double   abs_val;

    /* 清半球指示 */
    LcdSegments_SetSymbol(LCD_SYMBOL_LAT_N, false);
    LcdSegments_SetSymbol(LCD_SYMBOL_LAT_S, false);
    LcdSegments_SetSymbol(LCD_SYMBOL_LON_E, false);
    LcdSegments_SetSymbol(LCD_SYMBOL_LON_W, false);

    if (!valid)
    {
        draw_dashes((const uint8_t[]){8, 9, 10, 11, 12, 13, 14, 15, 16}, 9U);
        LcdSegments_SetSymbol(LCD_SYMBOL_COORD_DEG, false);
        LcdSegments_SetSymbol(LCD_SYMBOL_COORD_MIN, false);
        LcdSegments_SetSymbol(LCD_SYMBOL_COORD_SEC, false);
        LcdSegments_SetSymbol(LCD_SYMBOL_COORD_DOT, false);
        return;
    }

    abs_val = deg < 0.0 ? -deg : deg;
    deg_i   = (uint32_t)abs_val;
    {
        double rem = (abs_val - (double)deg_i) * 60.0;
        double sec;

        min_i    = (uint32_t)rem;
        sec      = (rem - (double)min_i) * 60.0;
        sec_x100 = (uint32_t)(sec * 100.0 + 0.5);
        if (sec_x100 >= 6000U) /* 60.00″ -> 1′ 进位 */
        {
            sec_x100 = 0U;
            min_i++;
            if (min_i >= 60U)
            {
                min_i = 0U;
                deg_i++;
            }
        }
    }

    LcdSegments_SetDigit(8, deg_i >= 100U ? (int8_t)((deg_i / 100U) % 10U) : -1);
    LcdSegments_SetDigit(9, deg_i >= 10U ? (int8_t)((deg_i / 10U) % 10U) : -1);
    LcdSegments_SetDigit(10, (int8_t)(deg_i % 10U));
    LcdSegments_SetSymbol(LCD_SYMBOL_COORD_DEG, true);

    LcdSegments_SetDigit(11, (int8_t)(min_i / 10U));
    LcdSegments_SetDigit(12, (int8_t)(min_i % 10U));
    LcdSegments_SetSymbol(LCD_SYMBOL_COORD_MIN, true);

    LcdSegments_SetDigit(13, (int8_t)(sec_x100 / 1000U));
    LcdSegments_SetDigit(14, (int8_t)((sec_x100 / 100U) % 10U));
    LcdSegments_SetSymbol(LCD_SYMBOL_COORD_DOT, true);
    LcdSegments_SetDigit(15, (int8_t)((sec_x100 / 10U) % 10U));
    LcdSegments_SetDigit(16, (int8_t)(sec_x100 % 10U));
    LcdSegments_SetSymbol(LCD_SYMBOL_COORD_SEC, true);

    if (is_lon)
    {
        LcdSegments_SetSymbol(deg >= 0.0 ? LCD_SYMBOL_LON_E : LCD_SYMBOL_LON_W, true);
    }
    else
    {
        LcdSegments_SetSymbol(deg >= 0.0 ? LCD_SYMBOL_LAT_N : LCD_SYMBOL_LAT_S, true);
    }
}

/** 底行高程：整数米（负值取绝对值，超 4 位丢弃高位） */
static void draw_elevation(float altitude_m, bool valid)
{
    uint32_t elev = (uint32_t)(altitude_m < 0.0f ? -altitude_m : altitude_m);

    LcdSegments_SetSymbol(LCD_SYMBOL_ELEVATION_LABEL_H, true);
    LcdSegments_SetSymbol(LCD_SYMBOL_ELEVATION_UNIT_M, true);

    if (!valid)
    {
        draw_dashes(digits_elev, 4U);
        return;
    }

    if (elev > 9999U)
    {
        elev %= 10000U;
    }
    LcdSegments_SetNumberRightAligned(digits_elev, 4U, elev);
}

/** 电池 4 段（框 + 4 格条，从左到右掉电：level 档亮最右 level 格） */
static void draw_battery(uint8_t level)
{
    LcdSegments_SetSymbol(LCD_SYMBOL_BATTERY_FRAME, true);
    LcdSegments_SetSymbol(LCD_SYMBOL_BATTERY_1, level >= 4U);
    LcdSegments_SetSymbol(LCD_SYMBOL_BATTERY_2, level >= 3U);
    LcdSegments_SetSymbol(LCD_SYMBOL_BATTERY_3, level >= 2U);
    LcdSegments_SetSymbol(LCD_SYMBOL_BATTERY_4, level >= 1U);
}

void display_init(void)
{
    LcdSegments_Init();
}

void display_render(const disp_state_t* s)
{
    static uint32_t fe_timer;
    static uint32_t ll_timer;
    static bool     fe_show_far;
    static bool     ll_show_lon;

    bool     multi_mode = (s->mode == MEAS_MODE_MULTI || s->mode == MEAS_MODE_TEST);
    uint32_t fe_period =
        multi_mode ? APP_DISP_FE_TOGGLE_SLOW_MS : APP_DISP_FE_TOGGLE_FAST_MS;

    if (s->page == DISP_PAGE_FULL_ON)
    {
        LcdSegments_SetAll(true);
        LcdSegments_Flush();
        return;
    }

    fe_timer += APP_DISP_RENDER_MS;
    if (fe_timer >= fe_period)
    {
        fe_timer    = 0U;
        fe_show_far = !fe_show_far;
    }
    ll_timer += APP_DISP_RENDER_MS;
    if (ll_timer >= APP_DISP_LL_TOGGLE_MS)
    {
        ll_timer    = 0U;
        ll_show_lon = !ll_show_lon;
    }

    LcdSegments_ClearBuffer();

    /* ---------------- 磁场校准进行中：显示已校准点数 / 总点数 ---------------- */
    if (s->page == DISP_PAGE_MAG_CAL)
    {
        /* 航向区（3位）：当前已校准点数 */
        LcdSegments_SetNumberRightAligned(digits_heading, 3U, (uint32_t)s->cal_cur_points);

        /* 距离区（4位）：校准总点数（54） */
        LcdSegments_SetNumberRightAligned(digits_distance, 4U, (uint32_t)s->cal_total_points);

        draw_battery(s->batt_level);
        LcdSegments_Flush();
        return;
    }

    /* ---------------- 磁场校准完成：显示校准得分 ---------------- */
    if (s->page == DISP_PAGE_MAG_DONE)
    {
        uint32_t disp_score;

        /* 得分显示在距离区：
         * 小于 10.0 的正常得分放大 100 倍以整数显示（如 0.18 显示 18）；
         * 大于 10.0 的异常分值（如 200 未校准/400 失败）直接显示整数 */
        if (s->cal_mag_score < 10.0f)
        {
            disp_score = (uint32_t)(s->cal_mag_score * 100.0f + 0.5f);
        }
        else
        {
            disp_score = (uint32_t)(s->cal_mag_score + 0.5f);
        }

        LcdSegments_SetNumberRightAligned(digits_distance, 4U, disp_score);
        LcdSegments_SetNumberRightAligned(digits_heading, 3U, (uint32_t)s->cal_total_points);

        draw_battery(s->batt_level);
        LcdSegments_Flush();
        return;
    }

    /* ---------------- 补偿设置页（PIt/HIt/HEr） ---------------- */
    if (s->page != DISP_PAGE_NONE)
    {
        /* 补偿值显示在高程区（绝对值，0.1° 单位整数显示——玻璃无小数段） */
        uint32_t abs_x1 = (uint32_t)(s->page_value_c01 < 0 ? -(int32_t)s->page_value_c01
                                                           : (int32_t)s->page_value_c01) /
                          10U;

        LcdSegments_SetNumberRightAligned(digits_elev, 4U, abs_x1);
        LcdSegments_SetSymbol(LCD_SYMBOL_ELEVATION_LABEL_H, true);
        LcdSegments_SetSymbol(LCD_SYMBOL_ELEVATION_UNIT_M, true);

        /* 实时生效值回到各自区域 */
        if (s->page == DISP_PAGE_PIT)
        {
            draw_pitch(s->pitch_c01, s->att_valid);
        }
        else
        {
            LcdSegments_SetNumberRightAligned(
                digits_heading, 3U,
                s->att_valid ? (uint32_t)(s->heading_c01 / 100U) : 0U);
            if (!s->att_valid)
            {
                draw_dashes(digits_heading, 3U);
            }
            LcdSegments_SetSymbol(LCD_SYMBOL_AZIMUTH_DEG, s->att_valid);
        }

        draw_battery(s->batt_level);
        LcdSegments_Flush();
        return;
    }

    /* ---------------- 正常模式 ---------------- */

    /* 模式图标（测试 = 单次+连续同亮） */
    LcdSegments_SetSymbol(LCD_SYMBOL_RANGE_SINGLE,
                          s->mode == MEAS_MODE_SINGLE || s->mode == MEAS_MODE_MULTI ||
                              s->mode == MEAS_MODE_TEST);
    LcdSegments_SetSymbol(LCD_SYMBOL_RANGE_CONTINUOUS,
                          s->mode == MEAS_MODE_CONT || s->mode == MEAS_MODE_TEST);

    /* 顶行航向 + 罗盘（仅多功能/测试） */
    if (multi_mode)
    {
        if (s->att_valid)
        {
            LcdSegments_SetNumberRightAligned(digits_heading, 3U,
                                              (uint32_t)(s->heading_c01 / 100U));
        }
        else
        {
            draw_dashes(digits_heading, 3U);
        }
        LcdSegments_SetSymbol(LCD_SYMBOL_AZIMUTH_DEG, s->att_valid);
        draw_compass(s->heading_c01, s->att_valid);
        draw_pitch(s->pitch_c01, s->att_valid);
    }

    /* 第二行距离：F/E 交替 */
    {
        bool     show_far = fe_show_far && s->result->far_valid;
        bool     dist_ok  = !s->measuring && (show_far ? s->result->far_valid : s->result->near_valid);
        uint32_t dist_mm  = show_far ? s->result->far_mm : s->result->near_mm;

        if (dist_ok)
        {
            LcdSegments_SetNumberRightAligned(digits_distance, 4U, (dist_mm + 500U) / 1000U);
        }
        else
        {
            draw_dashes(digits_distance, 4U);
        }
        LcdSegments_SetSymbol(LCD_SYMBOL_UNIT_M, true);
        LcdSegments_SetSymbol(LCD_SYMBOL_RANGE_FIRST_F, dist_ok && !show_far);
        LcdSegments_SetSymbol(LCD_SYMBOL_RANGE_LAST_E, dist_ok && show_far && s->result->far_valid);
    }

    /* 大字行坐标 + 底行高程（仅多功能/测试） */
    if (multi_mode)
    {
        bool show_far = fe_show_far && s->result->far_valid && s->target_valid;

        LcdSegments_SetSymbol(LCD_SYMBOL_COORD_FIRST_F, s->target_valid && !show_far);
        LcdSegments_SetSymbol(LCD_SYMBOL_COORD_LAST_E,
                              s->target_valid && show_far && s->result->far_valid);

        if (s->target_valid)
        {
            /* TARGET：目标坐标/高程 */
            const app_geo_point_t* tgt = show_far ? &s->target_far : &s->target_near;

            LcdSegments_SetSymbol(LCD_SYMBOL_COORD_LOCAL, false);
            LcdSegments_SetSymbol(LCD_SYMBOL_COORD_TARGET, true);

            if (!ll_show_lon)
            {
                draw_coord(tgt->latitude, false, tgt->valid);
            }
            else
            {
                draw_coord(tgt->longitude, true, tgt->valid);
            }
            draw_elevation(tgt->altitude_m, tgt->valid);
        }
        else
        {
            /* LOCAL：本机坐标/高程 */
            LcdSegments_SetSymbol(LCD_SYMBOL_COORD_LOCAL, true);
            LcdSegments_SetSymbol(LCD_SYMBOL_COORD_TARGET, false);

            if (!ll_show_lon)
            {
                draw_coord(s->self.latitude, false, s->self_valid);
            }
            else
            {
                draw_coord(s->self.longitude, true, s->self_valid);
            }
            draw_elevation(s->self.altitude_m, s->self_valid);
        }
    }

    /* 底行计数 21~25（5 位） */
    LcdSegments_SetNumberRightAligned(digits_count, 5U,
                                      s->count > APP_COUNT_MAX ? APP_COUNT_MAX : s->count);

    /* 十字准星常亮（瞄准基准） */
    LcdSegments_SetSymbol(LCD_SYMBOL_RETICLE, true);

    draw_battery(s->batt_level);

    LcdSegments_Flush();
}
