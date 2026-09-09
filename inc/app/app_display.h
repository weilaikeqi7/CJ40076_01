/**
 * @file app_display.h
 * @brief LCD 业务渲染（CS1622 玻璃，与 03 业务一致、按玻璃能力显示）
 *
 * 区域划分（数码管编号 1~27 同 lcd_segments.c）：
 *   顶行：  1 2 3 -> 航向 0~359° 整数（多功能/测试显示）+ 8 方向罗盘字母
 *           + 电池框 + 4 格条
 *   第二行：4 5 6 7 -> 距离整数米 + M + 首末目标图标 F/E + 单次/连续模式图标
 *   中行：  26 27 -> 俯仰整数度（2 位）+ P + ° + 负号
 *   大字行：8~16 -> 坐标 DDD° MM′ SS.ss″（经纬度 1s 交替，WSEN 指示半球）
 *           本机/目标：定位图标=LOCAL，靶心图标=TARGET；坐标行 F/E 跟随
 *   底行：  H + 17 18 19 20 -> 高程整数米（负值取绝对值）+ M
 *           21~25 -> 计数（5 位，上限 9999）
 *
 * F/E 交替：单次/连续 1s；多功能/测试 2s（坐标/高程跟随 F/E 同步交替）。
 */
#ifndef APP_DISPLAY_H
#define APP_DISPLAY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "app_coord.h"
#include "app_measure.h"

typedef enum
{
    DISP_PAGE_NONE = 0,
    DISP_PAGE_PIT,
    DISP_PAGE_HIT,
    DISP_PAGE_HER,
    DISP_PAGE_FULL_ON,
} disp_page_t;

typedef struct
{
    meas_mode_t mode;
    bool        measuring;

    const measure_result_t* result;

    bool    att_valid;
    int32_t heading_c01;
    int32_t pitch_c01;

    bool           self_valid;
    app_geo_point_t self;

    bool           target_valid;
    app_geo_point_t target_near;
    app_geo_point_t target_far;

    uint32_t count;
    uint8_t  batt_level; /* 1~4（4 段，level 格条全亮数=level） */

    disp_page_t page;
    int16_t     page_value_c01;
} disp_state_t;

void display_init(void);
void display_render(const disp_state_t* s);

#ifdef __cplusplus
}
#endif

#endif /* APP_DISPLAY_H */
