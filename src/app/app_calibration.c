#include "app_internal.h"

#include "app_log.h"
#include "app_state.h"
#include "jy901b.h"
#include "task.h"

#define APP_CALIBRATION_HOLD_START_MS 600U
#define APP_CALIBRATION_REPEAT_MS     100U
#define APP_CALIBRATION_NEXT_PAGE_MS   1000U

static bool g_imu_mag_calibration_active;
static volatile bool g_lcd_calibration_prompt;
static volatile CalibrationPage g_calibration_page;
static CalibrationOffsets g_calibration_offsets;
static CalibrationOffsets g_calibration_edit_offsets;
static uint32_t g_calibration_power_hold_ms;
static uint32_t g_calibration_mode_hold_ms;
static uint32_t g_calibration_power_repeat_ms;
static uint32_t g_calibration_mode_repeat_ms;
static uint32_t g_calibration_both_hold_ms;
static bool g_calibration_power_repeated;
static bool g_calibration_mode_repeated;
static bool g_calibration_both_active;
static bool g_calibration_wait_release;
static bool g_calibration_exit_wait_release;

void AppCalibrationInitFromStorage(void)
{
    MeasureCounter_GetCalibration(&g_calibration_offsets);
    g_calibration_edit_offsets = g_calibration_offsets;
}

bool AppCalibrationSettingsActive(void)
{
    return g_calibration_page != CALIBRATION_PAGE_NONE;
}

static void get_active_calibration_offsets(CalibrationOffsets* offsets)
{
    if (offsets == 0)
    {
        return;
    }

    taskENTER_CRITICAL();
    *offsets = AppCalibrationSettingsActive() ? g_calibration_edit_offsets : g_calibration_offsets;
    taskEXIT_CRITICAL();
}

void AppCalibrationApplyOrientationOffsets(OrientationData* data)
{
    CalibrationOffsets offsets;
    int32_t pitch_cd;
    int32_t yaw_cd;

    if (data == 0)
    {
        return;
    }

    get_active_calibration_offsets(&offsets);
    pitch_cd = -(int32_t)data->pitch_cd + ((int32_t)offsets.pitch_install_tenth_deg * 10);
    yaw_cd = -(int32_t)data->yaw_cd +
             ((int32_t)(offsets.yaw_install_tenth_deg + offsets.yaw_error_tenth_deg) * 10);

    if (pitch_cd > INT16_MAX)
    {
        pitch_cd = INT16_MAX;
    }
    else if (pitch_cd < INT16_MIN)
    {
        pitch_cd = INT16_MIN;
    }

    data->pitch_cd = (int16_t)pitch_cd;
    data->yaw_cd = (int16_t)AppNormalizeYawCentidegree(yaw_cd);
}

CalibrationPage AppCalibrationGetPage(CalibrationOffsets* offsets)
{
    CalibrationPage page;

    taskENTER_CRITICAL();
    page = g_calibration_page;
    if (offsets != 0)
    {
        *offsets = g_calibration_edit_offsets;
    }
    taskEXIT_CRITICAL();

    return page;
}

bool AppCalibrationPromptActive(void)
{
    return g_lcd_calibration_prompt;
}

bool AppCalibrationMagActive(void)
{
    return g_imu_mag_calibration_active;
}

static void reset_calibration_key_state(void)
{
    g_calibration_power_hold_ms = 0U;
    g_calibration_mode_hold_ms = 0U;
    g_calibration_power_repeat_ms = 0U;
    g_calibration_mode_repeat_ms = 0U;
    g_calibration_both_hold_ms = 0U;
    g_calibration_power_repeated = false;
    g_calibration_mode_repeated = false;
    g_calibration_both_active = false;
    g_calibration_wait_release = false;
}

bool AppCalibrationHandleExitWaitRelease(void)
{
    if (!g_calibration_exit_wait_release)
    {
        return false;
    }

    if ((!Keys_IsPowerPressed()) && (!Keys_IsModePressed()))
    {
        g_calibration_exit_wait_release = false;
        reset_calibration_key_state();
    }

    return true;
}

void AppCalibrationEnterSettings(CalibrationPage start_page)
{
    AppState_ClearRange();
    AppRangePowerSet(false);
    AppGnssPowerSet(false);
    AppImuPowerSet(true);

    taskENTER_CRITICAL();
    g_calibration_edit_offsets = g_calibration_offsets;
    g_calibration_page = start_page;
    taskEXIT_CRITICAL();

    g_calibration_exit_wait_release = false;
    reset_calibration_key_state();
}

static void leave_calibration_settings(void)
{
    taskENTER_CRITICAL();
    g_calibration_page = CALIBRATION_PAGE_NONE;
    taskEXIT_CRITICAL();

    reset_calibration_key_state();
    g_calibration_exit_wait_release = true;
    AppState_ClearOrientation();
    AppApplyModePower(AppState_GetMode());
}

static void save_calibration_settings(void)
{
    CalibrationOffsets offsets;

    taskENTER_CRITICAL();
    offsets = g_calibration_edit_offsets;
    taskEXIT_CRITICAL();

    if (!MeasureCounter_SaveCalibration(&offsets))
    {
        APP_LOGE("cal", "failed to save angle offsets");
        return;
    }

    taskENTER_CRITICAL();
    g_calibration_offsets = offsets;
    taskEXIT_CRITICAL();
    leave_calibration_settings();
}

static void next_calibration_page(void)
{
    taskENTER_CRITICAL();
    if (g_calibration_page == CALIBRATION_PAGE_PITCH_INSTALL)
    {
        g_calibration_page = CALIBRATION_PAGE_YAW_INSTALL;
        g_calibration_both_active = false;
        g_calibration_wait_release = true;
    }
    taskEXIT_CRITICAL();
}

static void adjust_calibration_value(int16_t delta)
{
    int16_t* value = 0;
    int16_t minimum = 0;
    int16_t maximum = 0;
    int32_t adjusted;

    taskENTER_CRITICAL();
    switch (g_calibration_page)
    {
    case CALIBRATION_PAGE_PITCH_INSTALL:
        value = &g_calibration_edit_offsets.pitch_install_tenth_deg;
        minimum = -900;
        maximum = 900;
        break;
    case CALIBRATION_PAGE_YAW_INSTALL:
        value = &g_calibration_edit_offsets.yaw_install_tenth_deg;
        minimum = -1800;
        maximum = 1800;
        break;
    case CALIBRATION_PAGE_YAW_ERROR:
        value = &g_calibration_edit_offsets.yaw_error_tenth_deg;
        minimum = -1800;
        maximum = 1800;
        break;
    default:
        break;
    }

    if (value != 0)
    {
        adjusted = (int32_t)*value + delta;
        if (adjusted < minimum)
        {
            adjusted = minimum;
        }
        else if (adjusted > maximum)
        {
            adjusted = maximum;
        }
        *value = (int16_t)adjusted;
    }
    taskEXIT_CRITICAL();
}

void AppCalibrationHandleSettingsInput(KeyEvent event, uint32_t now_ms)
{
    const bool power_pressed = Keys_IsPowerPressed();
    const bool mode_pressed = Keys_IsModePressed();
    const CalibrationPage page = g_calibration_page;
    const bool power_repeated_before = g_calibration_power_repeated;
    const bool mode_repeated_before = g_calibration_mode_repeated;

    if (g_calibration_wait_release)
    {
        if ((!power_pressed) && (!mode_pressed))
        {
            reset_calibration_key_state();
        }
        return;
    }

    if (g_calibration_both_active)
    {
        if (power_pressed && mode_pressed)
        {
            if ((now_ms - g_calibration_both_hold_ms) >= APP_CALIBRATION_NEXT_PAGE_MS)
            {
                if (page == CALIBRATION_PAGE_PITCH_INSTALL)
                {
                    next_calibration_page();
                }
                else
                {
                    g_calibration_both_active = false;
                    save_calibration_settings();
                }
            }
        }
        else
        {
            g_calibration_both_active = false;
            g_calibration_wait_release = true;
        }
        return;
    }

    if (power_pressed && mode_pressed)
    {
        g_calibration_both_active = true;
        g_calibration_both_hold_ms = now_ms;
        g_calibration_power_repeated = true;
        g_calibration_mode_repeated = true;
        return;
    }

    if (power_pressed)
    {
        if (g_calibration_power_hold_ms == 0U)
        {
            g_calibration_power_hold_ms = now_ms;
            g_calibration_power_repeat_ms = now_ms;
        }
        else if (((now_ms - g_calibration_power_hold_ms) >= APP_CALIBRATION_HOLD_START_MS) &&
                 ((now_ms - g_calibration_power_repeat_ms) >= APP_CALIBRATION_REPEAT_MS))
        {
            adjust_calibration_value(1);
            g_calibration_power_repeat_ms = now_ms;
            g_calibration_power_repeated = true;
        }
    }
    else
    {
        g_calibration_power_hold_ms = 0U;
        g_calibration_power_repeat_ms = 0U;
        g_calibration_power_repeated = false;
    }

    if (mode_pressed)
    {
        if (g_calibration_mode_hold_ms == 0U)
        {
            g_calibration_mode_hold_ms = now_ms;
            g_calibration_mode_repeat_ms = now_ms;
        }
        else if (((now_ms - g_calibration_mode_hold_ms) >= APP_CALIBRATION_HOLD_START_MS) &&
                 ((now_ms - g_calibration_mode_repeat_ms) >= APP_CALIBRATION_REPEAT_MS))
        {
            adjust_calibration_value(-1);
            g_calibration_mode_repeat_ms = now_ms;
            g_calibration_mode_repeated = true;
        }
    }
    else
    {
        g_calibration_mode_hold_ms = 0U;
        g_calibration_mode_repeat_ms = 0U;
        g_calibration_mode_repeated = false;
    }

    if ((event == KEY_EVENT_POWER_SHORT) && (!power_repeated_before))
    {
        adjust_calibration_value(1);
    }
    else if ((event == KEY_EVENT_MODE_SHORT) && (!mode_repeated_before))
    {
        adjust_calibration_value(-1);
    }
}

static void enter_imu_calibration_power(void)
{
    AppState_ClearRange();
    AppRangePowerSet(false);
    AppGnssPowerSet(false);
    AppImuPowerSet(true);
}

static void leave_imu_calibration_power(void)
{
    AppApplyModePower(AppState_GetMode());
}

void AppCalibrationCalibrateAccelerometer(void)
{
    bool ok;

    enter_imu_calibration_power();
    g_lcd_calibration_prompt = true;
    ok = Jy901b_CalibrateAccelerometer();
    g_lcd_calibration_prompt = false;
    leave_imu_calibration_power();
    if (!ok)
    {
        APP_LOGE("cal", "accelerometer command failed");
    }
}

void AppCalibrationCalibrateRefAngle(void)
{
    bool ok;

    enter_imu_calibration_power();
    g_lcd_calibration_prompt = true;
    ok = Jy901b_CalibrateRefAngle();
    g_lcd_calibration_prompt = false;
    leave_imu_calibration_power();
    if (!ok)
    {
        APP_LOGE("cal", "ref angle command failed");
    }
}

void AppCalibrationStartMag(void)
{
    enter_imu_calibration_power();
    g_lcd_calibration_prompt = true;
    if (Jy901b_StartMagCalibration())
    {
        g_imu_mag_calibration_active = true;
        APP_LOGI("cal", "mag calibration started");
    }
    else
    {
        g_lcd_calibration_prompt = false;
        leave_imu_calibration_power();
        APP_LOGE("cal", "mag start command failed");
    }
}

void AppCalibrationStopMag(void)
{
    bool ok;

    if (!g_imu_mag_calibration_active)
    {
        return;
    }

    ok = Jy901b_StopMagCalibration();
    g_imu_mag_calibration_active = false;
    g_lcd_calibration_prompt = false;
    leave_imu_calibration_power();
    if (!ok)
    {
        APP_LOGE("cal", "mag stop command failed");
    }
    else
    {
        APP_LOGI("cal", "mag calibration stopped, save command sent");
    }
}

void AppCalibrationAbortActive(void)
{
    taskENTER_CRITICAL();
    g_calibration_page = CALIBRATION_PAGE_NONE;
    taskEXIT_CRITICAL();

    g_imu_mag_calibration_active = false;
    g_lcd_calibration_prompt = false;
    g_calibration_exit_wait_release = false;
    reset_calibration_key_state();
}
