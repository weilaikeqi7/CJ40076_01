#include "app_internal.h"

#include "app_log.h"
#include "app_state.h"
#include "board_config.h"
#include "lcd_segments.h"
#include "task.h"

#include <math.h>

#define APP_EARTH_RADIUS_M 6371000.0

typedef struct
{
    int32_t latitude_e7;
    int32_t longitude_e7;
    int32_t altitude_cm;
} DisplayTargetCoordinate;

typedef struct
{
    uint32_t update_ms;
    bool update_seen;
    bool has_result;
    bool valid;
    bool first_valid;
    bool last_valid;
    DisplayTargetCoordinate first;
    DisplayTargetCoordinate last;
} DisplayTargetCache;

static void display_dash_digits(const uint8_t* digit_ids, uint8_t digit_count)
{
    for (uint8_t i = 0U; i < digit_count; ++i)
    {
        LcdSegments_SetDash(digit_ids[i], true);
    }
}

static void display_number_fixed(const uint8_t* digit_ids, uint8_t digit_count, uint32_t value)
{
    if ((digit_ids == 0) || (digit_count == 0U))
    {
        return;
    }

    for (int8_t i = (int8_t)(digit_count - 1U); i >= 0; --i)
    {
        LcdSegments_SetDigit(digit_ids[i], (int8_t)(value % 10U));
        value /= 10U;
    }
}

static void display_battery_symbols(uint8_t level)
{
    static const LcdSymbolId battery_symbols[] = {
        LCD_SYMBOL_BATTERY_1,
        LCD_SYMBOL_BATTERY_2,
        LCD_SYMBOL_BATTERY_3,
        LCD_SYMBOL_BATTERY_4,
    };

    LcdSegments_SetSymbol((uint8_t)LCD_SYMBOL_BATTERY_FRAME, true);

    if (level > 4U)
    {
        level = 4U;
    }

    for (uint8_t i = 0U; i < (sizeof(battery_symbols) / sizeof(battery_symbols[0])); ++i)
    {
        LcdSegments_SetSymbol((uint8_t)battery_symbols[i], i < level);
    }
}

static void display_calibration_label(const uint8_t* digit_ids, CalibrationPage page)
{
    const char* label = "   ";

    switch (page)
    {
    case CALIBRATION_PAGE_PITCH_INSTALL:
        label = "PIt";
        break;
    case CALIBRATION_PAGE_YAW_INSTALL:
        label = "HIt";
        break;
    case CALIBRATION_PAGE_YAW_ERROR:
        label = "HEr";
        break;
    default:
        break;
    }

    for (uint8_t i = 0U; i < 3U; ++i)
    {
        LcdSegments_SetChar(digit_ids[i], label[i]);
    }
}

static void display_calibration_live_angle(const uint8_t* digit_ids,
                                           CalibrationPage page,
                                           const OrientationData* orientation)
{
    int32_t tenths;

    if ((orientation == 0) || (!orientation->valid))
    {
        display_dash_digits(digit_ids, 4U);
        return;
    }

    if (page == CALIBRATION_PAGE_PITCH_INSTALL)
    {
        tenths = orientation->pitch_cd / 10;
        if (tenths < -999)
        {
            tenths = -999;
        }
        else if (tenths > 9999)
        {
            tenths = 9999;
        }

        if (tenths < 0)
        {
            LcdSegments_SetDash(digit_ids[0], true);
            display_number_fixed(&digit_ids[1], 3U, (uint32_t)(-tenths));
        }
        else
        {
            LcdSegments_SetNumberRightAligned(digit_ids, 4U, (uint32_t)tenths);
        }
    }
    else
    {
        tenths = AppNormalizeYawCentidegree(orientation->yaw_cd);
        if (tenths < 0)
        {
            tenths += 36000;
        }
        tenths /= 10;
        LcdSegments_SetNumberRightAligned(digit_ids, 4U, (uint32_t)tenths);
    }
}

static void display_calibration_settings(const uint8_t* label_digits,
                                         const uint8_t* live_angle_digits,
                                         const uint8_t* value_digits,
                                         const AppStateSnapshot* snapshot)
{
    CalibrationOffsets offsets;
    int16_t value = 0;
    uint32_t absolute;
    const CalibrationPage page = AppCalibrationGetPage(&offsets);

    LcdSegments_ClearBuffer();
    display_battery_symbols(snapshot->battery.level);
    display_calibration_label(label_digits, page);
    display_calibration_live_angle(live_angle_digits, page, &snapshot->orientation);

    switch (page)
    {
    case CALIBRATION_PAGE_PITCH_INSTALL:
        value = offsets.pitch_install_tenth_deg;
        break;
    case CALIBRATION_PAGE_YAW_INSTALL:
        value = offsets.yaw_install_tenth_deg;
        LcdSegments_SetSymbol((uint8_t)LCD_SYMBOL_RANGE_SINGLE, true);
        break;
    case CALIBRATION_PAGE_YAW_ERROR:
        value = offsets.yaw_error_tenth_deg;
        LcdSegments_SetSymbol((uint8_t)LCD_SYMBOL_RANGE_CONTINUOUS, true);
        break;
    default:
        break;
    }

    LcdSegments_SetSymbol((uint8_t)LCD_SYMBOL_PITCH_SIGN_MINUS, true);
    LcdSegments_SetSymbol((uint8_t)LCD_SYMBOL_PITCH_SIGN_PLUS, value >= 0);
    absolute = (uint32_t)((value < 0) ? -value : value);
    LcdSegments_SetNumberRightAligned(value_digits, 4U, absolute);
}

static void log_lcd_refresh_data(const char* view, const AppStateSnapshot* snapshot)
{
    if (snapshot == 0)
    {
        return;
    }

    if (snapshot->orientation.valid)
    {
        APP_LOGI("lcd",
                 "%s mode=%u imu[r_cd=%d p_cd=%d y_cd=%d t=%u]",
                 view,
                 (unsigned int)snapshot->mode,
                 (int)snapshot->orientation.roll_cd,
                 (int)snapshot->orientation.pitch_cd,
                 (int)snapshot->orientation.yaw_cd,
                 (unsigned int)snapshot->orientation.update_ms);
    }

    if (snapshot->range.valid &&
        (snapshot->range.first_valid || snapshot->range.last_valid))
    {
        APP_LOGI("lcd",
                 "%s mode=%u range[cmd=0x%02X f=%u/%u l=%u/%u st=%u t=%u]",
                 view,
                 (unsigned int)snapshot->mode,
                 (unsigned int)snapshot->range.command,
                 snapshot->range.first_valid ? 1U : 0U,
                 (unsigned int)snapshot->range.first_distance_mm,
                 snapshot->range.last_valid ? 1U : 0U,
                 (unsigned int)snapshot->range.last_distance_mm,
                 (unsigned int)snapshot->range.status,
                 (unsigned int)snapshot->range.update_ms);
    }

    if (snapshot->gnss.valid && snapshot->gnss.fix)
    {
        APP_LOGI("lcd",
                 "%s mode=%u gps[fix=%u sat=%u lat_e7=%d lon_e7=%d alt_cm=%d t=%u]",
                 view,
                 (unsigned int)snapshot->mode,
                 snapshot->gnss.fix ? 1U : 0U,
                 (unsigned int)snapshot->gnss.satellites,
                 (int)snapshot->gnss.latitude_e7,
                 (int)snapshot->gnss.longitude_e7,
                 (int)snapshot->gnss.altitude_cm,
                 (unsigned int)snapshot->gnss.update_ms);
    }
}

static void display_direction_symbols(int32_t yaw_deg)
{
    const int32_t degrees = AppNormalizeDegrees(yaw_deg);
    bool north = false;
    bool east = false;
    bool south = false;
    bool west = false;

    if ((degrees >= 337) || (degrees < 22))
    {
        north = true;
    }
    else if (degrees < 67)
    {
        north = true;
        east = true;
    }
    else if (degrees < 112)
    {
        east = true;
    }
    else if (degrees < 157)
    {
        east = true;
        south = true;
    }
    else if (degrees < 202)
    {
        south = true;
    }
    else if (degrees < 247)
    {
        south = true;
        west = true;
    }
    else if (degrees < 292)
    {
        west = true;
    }
    else
    {
        west = true;
        north = true;
    }

    LcdSegments_SetSymbol((uint8_t)LCD_SYMBOL_DIR_N, north);
    LcdSegments_SetSymbol((uint8_t)LCD_SYMBOL_DIR_E, east && !north);
    LcdSegments_SetSymbol((uint8_t)LCD_SYMBOL_DIR_NE_E, north && east);
    LcdSegments_SetSymbol((uint8_t)LCD_SYMBOL_DIR_S, south);
    LcdSegments_SetSymbol((uint8_t)LCD_SYMBOL_DIR_W, west);
}

static void display_mode_symbols(AppWorkMode mode)
{
    const bool multifunction_mode = AppModeUsesMultifunction(mode);
#if APP_WORK_TIME_TEST_MODE_ENABLE
    const bool work_time_test_mode = mode == APP_MODE_WORK_TIME_TEST;
#else
    const bool work_time_test_mode = false;
#endif

    LcdSegments_SetSymbol((uint8_t)LCD_SYMBOL_RETICLE, true);
    LcdSegments_SetSymbol((uint8_t)LCD_SYMBOL_UNIT_M, true);
    LcdSegments_SetSymbol((uint8_t)LCD_SYMBOL_RANGE_SINGLE,
                          (mode == APP_MODE_SINGLE) || multifunction_mode);
    LcdSegments_SetSymbol((uint8_t)LCD_SYMBOL_RANGE_CONTINUOUS,
                          (mode == APP_MODE_CONTINUOUS) || work_time_test_mode);
}

static double degrees_to_radians(double degrees)
{
    return degrees * 3.14159265358979323846 / 180.0;
}

static double radians_to_degrees(double radians)
{
    return radians * 180.0 / 3.14159265358979323846;
}

static int32_t target_coordinate_e7(int32_t origin_e7,
                                    int32_t paired_origin_e7,
                                    uint32_t distance_mm,
                                    int32_t pitch_cd,
                                    int32_t yaw_cd,
                                    bool latitude)
{
    const double lat1 = degrees_to_radians((double)(latitude ? origin_e7 : paired_origin_e7) / 10000000.0);
    const double lon1 = degrees_to_radians((double)(latitude ? paired_origin_e7 : origin_e7) / 10000000.0);
    const double bearing = degrees_to_radians((double)yaw_cd / 100.0);
    const double pitch = degrees_to_radians((double)pitch_cd / 100.0);
    const double horizontal_m = ((double)distance_mm / 1000.0) * cos(pitch);
    const double angular_distance = horizontal_m / APP_EARTH_RADIUS_M;
    const double lat2 = asin((sin(lat1) * cos(angular_distance)) +
                             (cos(lat1) * sin(angular_distance) * cos(bearing)));
    double lon2 = lon1 + atan2(sin(bearing) * sin(angular_distance) * cos(lat1),
                               cos(angular_distance) - (sin(lat1) * sin(lat2)));

    lon2 = fmod(lon2 + (3.0 * 3.14159265358979323846), 2.0 * 3.14159265358979323846) -
           3.14159265358979323846;

    return (int32_t)((latitude ? radians_to_degrees(lat2) : radians_to_degrees(lon2)) * 10000000.0);
}

static int32_t target_altitude_cm(int32_t origin_altitude_cm, uint32_t distance_mm, int32_t pitch_cd)
{
    const double pitch = degrees_to_radians((double)pitch_cd / 100.0);
    const double height_cm = ((double)distance_mm / 10.0) * sin(pitch);

    return (int32_t)((double)origin_altitude_cm + height_cm);
}

static DisplayTargetCoordinate calculate_target_coordinate(const GnssData* gnss,
                                                           const OrientationData* orientation,
                                                           uint32_t range_mm)
{
    DisplayTargetCoordinate target;

    target.latitude_e7 = target_coordinate_e7(gnss->latitude_e7,
                                              gnss->longitude_e7,
                                              range_mm,
                                              orientation->pitch_cd,
                                              orientation->yaw_cd,
                                              true);
    target.longitude_e7 = target_coordinate_e7(gnss->longitude_e7,
                                               gnss->latitude_e7,
                                               range_mm,
                                               orientation->pitch_cd,
                                               orientation->yaw_cd,
                                               false);
    target.altitude_cm = target_altitude_cm(gnss->altitude_cm,
                                            range_mm,
                                            orientation->pitch_cd);

    return target;
}

static void display_coordinate_value(int32_t coordinate_e7,
                                     const uint8_t* degree_digits,
                                     const uint8_t* minute_digits,
                                     const uint8_t* minute_fraction_digits)
{
    uint32_t absolute_e7;
    uint32_t degrees;
    uint32_t minutes;
    uint32_t minute_fraction;
    uint32_t remain_e7;

    if (coordinate_e7 < 0)
    {
        absolute_e7 = (uint32_t)(-coordinate_e7);
    }
    else
    {
        absolute_e7 = (uint32_t)coordinate_e7;
    }

    degrees = absolute_e7 / 10000000U;
    remain_e7 = absolute_e7 - (degrees * 10000000U);
    minutes = (remain_e7 * 60U) / 10000000U;
    remain_e7 = (remain_e7 * 60U) - (minutes * 10000000U);
    minute_fraction = (uint32_t)(((uint64_t)remain_e7 * 10000ULL) / 10000000ULL);

    display_number_fixed(degree_digits, 3U, degrees);
    display_number_fixed(minute_digits, 2U, minutes);
    display_number_fixed(minute_fraction_digits, 4U, minute_fraction);
}

void AppDisplayTask(void* argument)
{
    AppStateSnapshot snapshot;
    AppPowerMode last_power_mode = APP_POWER_FAULT;
    AppWorkMode last_display_mode = APP_MODE_COUNT;
    DisplayTargetCache multi_target_cache = { 0 };
    static const uint8_t azimuth_digits[] = { 1U, 2U, 3U };
    static const uint8_t range_digits[] = { 4U, 5U, 6U, 7U };
    static const uint8_t coord_digits[] = { 8U, 9U, 10U, 11U, 12U, 13U, 14U, 15U, 16U };
    static const uint8_t coord_degree_digits[] = { 8U, 9U, 10U };
    static const uint8_t coord_minute_digits[] = { 11U, 12U };
    static const uint8_t coord_minute_fraction_digits[] = { 13U, 14U, 15U, 16U };
    static const uint8_t altitude_digits[] = { 17U, 18U, 19U, 20U };
    static const uint8_t count_digits[] = { 21U, 22U, 23U, 24U, 25U };
    static const uint8_t pitch_digits[] = { 26U, 27U };

    (void)argument;
    LcdSegments_Init();

    while (1)
    {
        AppState_Get(&snapshot);
        if (snapshot.power_mode != APP_POWER_RUN)
        {
            last_power_mode = snapshot.power_mode;
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        if (last_power_mode != APP_POWER_RUN)
        {
            LcdSegments_Init();
        }
        last_power_mode = APP_POWER_RUN;

        if (snapshot.mode != last_display_mode)
        {
            multi_target_cache = (DisplayTargetCache){ 0 };
            last_display_mode = snapshot.mode;
        }

        if (AppCalibrationPromptActive())
        {
            LcdSegments_SetAll(true);
            LcdSegments_Flush();
            log_lcd_refresh_data("imu_cal", &snapshot);
            vTaskDelay(pdMS_TO_TICKS(200U));
            continue;
        }

        if (AppCalibrationSettingsActive())
        {
            display_calibration_settings(azimuth_digits,
                                         range_digits,
                                         altitude_digits,
                                         &snapshot);
            LcdSegments_Flush();
            log_lcd_refresh_data("offset", &snapshot);
            vTaskDelay(pdMS_TO_TICKS(100U));
            continue;
        }

        LcdSegments_ClearBuffer();
        display_mode_symbols(snapshot.mode);
        display_battery_symbols(snapshot.battery.level);
        if (snapshot.measure_count_valid)
        {
            LcdSegments_SetNumberRightAligned(count_digits, sizeof(count_digits), snapshot.measure_count);
        }
        else
        {
            display_dash_digits(count_digits, sizeof(count_digits));
        }

        const bool range_result_current = AppMeasureRangeResultCurrent(&snapshot);
        bool display_range_valid = false;
        bool display_range_is_last = false;
        uint32_t display_range_mm = 0U;

        if (range_result_current)
        {
            if (snapshot.range.first_valid && snapshot.range.last_valid)
            {
                const uint32_t first_last_toggle_ms =
                    AppModeUsesMultifunction(snapshot.mode) ? 2000U : 1000U;

                display_range_is_last =
                    ((snapshot.uptime_ms / first_last_toggle_ms) & 1U) != 0U;
                display_range_valid = true;
                display_range_mm = display_range_is_last ?
                    snapshot.range.last_distance_mm : snapshot.range.first_distance_mm;
            }
            else if (snapshot.range.first_valid)
            {
                display_range_valid = true;
                display_range_mm = snapshot.range.first_distance_mm;
            }
            else if (snapshot.range.last_valid)
            {
                display_range_valid = true;
                display_range_is_last = true;
                display_range_mm = snapshot.range.last_distance_mm;
            }
            if (display_range_valid)
            {
                LcdSegments_SetNumberRightAligned(range_digits, sizeof(range_digits), display_range_mm / 1000U);
                LcdSegments_SetSymbol((uint8_t)LCD_SYMBOL_RANGE_FIRST_F, !display_range_is_last);
                LcdSegments_SetSymbol((uint8_t)LCD_SYMBOL_RANGE_LAST_E, display_range_is_last);
            }
            else
            {
                display_dash_digits(range_digits, sizeof(range_digits));
            }
        }
        else
        {
            display_dash_digits(range_digits, sizeof(range_digits));
        }

        if (AppModeUsesMultifunction(snapshot.mode))
        {
            if (range_result_current &&
                ((!multi_target_cache.update_seen) ||
                 (snapshot.range.update_ms != multi_target_cache.update_ms)))
            {
                const bool range_has_first_or_last =
                    snapshot.range.first_valid || snapshot.range.last_valid;

                multi_target_cache.update_seen = true;
                multi_target_cache.update_ms = snapshot.range.update_ms;
                multi_target_cache.has_result = range_has_first_or_last;
                multi_target_cache.valid = false;
                multi_target_cache.first_valid = false;
                multi_target_cache.last_valid = false;

                if (range_has_first_or_last && snapshot.orientation.valid && snapshot.gnss.fix)
                {
                    multi_target_cache.first_valid = snapshot.range.first_valid;
                    multi_target_cache.last_valid = snapshot.range.last_valid;

                    if (multi_target_cache.first_valid)
                    {
                        multi_target_cache.first =
                            calculate_target_coordinate(&snapshot.gnss,
                                                        &snapshot.orientation,
                                                        snapshot.range.first_distance_mm);
                    }

                    if (multi_target_cache.last_valid)
                    {
                        multi_target_cache.last =
                            calculate_target_coordinate(&snapshot.gnss,
                                                        &snapshot.orientation,
                                                        snapshot.range.last_distance_mm);
                    }

                    multi_target_cache.valid =
                        multi_target_cache.first_valid ||
                        multi_target_cache.last_valid;
                }
            }
        }
        else
        {
            multi_target_cache.update_seen = false;
            multi_target_cache.has_result = false;
            multi_target_cache.valid = false;
            multi_target_cache.update_ms = 0U;
        }

        if (AppModeUsesMultifunction(snapshot.mode))
        {
            const int32_t yaw_deg = snapshot.orientation.valid ?
                AppNormalizeDegrees(snapshot.orientation.yaw_cd / 100) : 0;
            int32_t pitch_deg = snapshot.orientation.pitch_cd / 100;

            LcdSegments_SetSymbol((uint8_t)LCD_SYMBOL_AZIMUTH_DEG, true);
            LcdSegments_SetSymbol((uint8_t)LCD_SYMBOL_PITCH_LABEL_P, true);
            LcdSegments_SetSymbol((uint8_t)LCD_SYMBOL_PITCH_DEG, true);
            LcdSegments_SetSymbol((uint8_t)LCD_SYMBOL_PITCH_SIGN_MINUS,
                                  snapshot.orientation.valid && (pitch_deg < 0));
            LcdSegments_SetSymbol((uint8_t)LCD_SYMBOL_PITCH_SIGN_PLUS, false);

            if (pitch_deg < 0)
            {
                pitch_deg = -pitch_deg;
            }

            if (snapshot.orientation.valid)
            {
                LcdSegments_SetNumberRightAligned(azimuth_digits, sizeof(azimuth_digits), (uint32_t)yaw_deg);
                LcdSegments_SetNumberRightAligned(pitch_digits, sizeof(pitch_digits), (uint32_t)pitch_deg);
                display_direction_symbols(yaw_deg);
            }
        }

        if (AppModeUsesMultifunction(snapshot.mode))
        {
            int32_t display_latitude = snapshot.gnss.latitude_e7;
            int32_t display_longitude = snapshot.gnss.longitude_e7;
            int32_t display_altitude_cm = snapshot.gnss.altitude_cm;
            const DisplayTargetCoordinate* display_target = 0;
            bool coord_range_is_last = false;
            const bool target_mode_display = range_result_current && multi_target_cache.has_result;
            const bool local_available = snapshot.gnss.fix;
            const bool show_latitude = ((snapshot.uptime_ms / 1000U) & 1U) != 0U;

            if (range_result_current && multi_target_cache.valid)
            {
                if (multi_target_cache.first_valid && multi_target_cache.last_valid)
                {
                    coord_range_is_last =
                        ((snapshot.uptime_ms / 2000U) & 1U) != 0U;
                    display_target = coord_range_is_last ?
                        &multi_target_cache.last : &multi_target_cache.first;
                }
                else if (multi_target_cache.first_valid)
                {
                    display_target = &multi_target_cache.first;
                }
                else if (multi_target_cache.last_valid)
                {
                    coord_range_is_last = true;
                    display_target = &multi_target_cache.last;
                }
            }

            const bool target_available = display_target != 0;

            if (target_available)
            {
                display_latitude = display_target->latitude_e7;
                display_longitude = display_target->longitude_e7;
                display_altitude_cm = display_target->altitude_cm;
            }

            LcdSegments_SetSymbol((uint8_t)LCD_SYMBOL_COORD_LOCAL, !target_mode_display);
            LcdSegments_SetSymbol((uint8_t)LCD_SYMBOL_COORD_TARGET, target_mode_display);
            LcdSegments_SetSymbol((uint8_t)LCD_SYMBOL_COORD_FIRST_F, target_available && !coord_range_is_last);
            LcdSegments_SetSymbol((uint8_t)LCD_SYMBOL_COORD_LAST_E, target_available && coord_range_is_last);
            LcdSegments_SetSymbol((uint8_t)LCD_SYMBOL_COORD_DEG, true);
            LcdSegments_SetSymbol((uint8_t)LCD_SYMBOL_COORD_MIN, true);
            LcdSegments_SetSymbol((uint8_t)LCD_SYMBOL_COORD_SEC, true);
            LcdSegments_SetSymbol((uint8_t)LCD_SYMBOL_COORD_DOT, true);
            LcdSegments_SetSymbol((uint8_t)LCD_SYMBOL_ELEVATION_LABEL_H, true);
            LcdSegments_SetSymbol((uint8_t)LCD_SYMBOL_ELEVATION_UNIT_M, true);

            if ((target_mode_display && !target_available) ||
                ((!target_mode_display) && !local_available))
            {
                display_dash_digits(coord_digits, sizeof(coord_digits));
                display_dash_digits(altitude_digits, sizeof(altitude_digits));
            }
            else
            {
                LcdSegments_SetSymbol((uint8_t)LCD_SYMBOL_LAT_N, show_latitude && (display_latitude >= 0));
                LcdSegments_SetSymbol((uint8_t)LCD_SYMBOL_LAT_S, show_latitude && (display_latitude < 0));
                LcdSegments_SetSymbol((uint8_t)LCD_SYMBOL_LON_E, (!show_latitude) && (display_longitude >= 0));
                LcdSegments_SetSymbol((uint8_t)LCD_SYMBOL_LON_W, (!show_latitude) && (display_longitude < 0));

                display_coordinate_value(show_latitude ? display_latitude : display_longitude,
                                         coord_degree_digits,
                                         coord_minute_digits,
                                         coord_minute_fraction_digits);

                if (display_altitude_cm < 0)
                {
                    display_altitude_cm = -display_altitude_cm;
                }
                LcdSegments_SetNumberRightAligned(altitude_digits,
                                                  sizeof(altitude_digits),
                                                  (uint32_t)display_altitude_cm / 100U);
            }
        }

        LcdSegments_Flush();
        log_lcd_refresh_data("normal", &snapshot);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
