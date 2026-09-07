#include "app_internal.h"

#include "app_log.h"
#include "app_state.h"
#include "board.h"
#include "board_config.h"
#include "measure_counter.h"
#include "task.h"

static uint32_t g_mode_click_last_ms;
static uint8_t g_mode_click_count;
#if APP_WORK_TIME_TEST_MODE_ENABLE
static bool g_work_time_test_active;
static uint32_t g_work_time_test_next_ms;
#endif

static void stop_work_time_test(void)
{
#if APP_WORK_TIME_TEST_MODE_ENABLE
    g_work_time_test_active = false;
#endif
}

static void handle_mode_key(void)
{
    AppWorkMode mode;

    mode = AppState_GetMode();
#if APP_WORK_TIME_TEST_MODE_ENABLE
    mode = (AppWorkMode)(((uint32_t)mode + 1U) % (uint32_t)APP_MODE_COUNT);
#else
    mode = (mode >= APP_MODE_MULTI) ? APP_MODE_SINGLE : (AppWorkMode)((uint32_t)mode + 1U);
#endif
    AppState_SetMode(mode);
    AppState_ClearRange();
    AppApplyModePower(mode);
}

static void clear_measure_count(void)
{
    MeasureCounter_Reset();
    AppState_SetMeasureCount(MeasureCounter_Get());
}

static void enter_calibration_page(CalibrationPage page)
{
    stop_work_time_test();
    AppCalibrationEnterSettings(page);
}

static void handle_mode_click_count(uint8_t count)
{
    if (AppCalibrationMagActive())
    {
        if (count == 6U)
        {
            AppCalibrationStopMag();
        }
        return;
    }

    switch (count)
    {
    case 1U:
        handle_mode_key();
        break;
    case 3U:
        clear_measure_count();
        break;
    case 4U:
        enter_calibration_page(CALIBRATION_PAGE_YAW_ERROR);
        break;
    case 5U:
        stop_work_time_test();
        AppCalibrationStartMag();
        break;
    case 6U:
        AppCalibrationStopMag();
        break;
    case 7U:
        enter_calibration_page(CALIBRATION_PAGE_PITCH_INSTALL);
        break;
    case 8U:
        stop_work_time_test();
        AppCalibrationCalibrateAccelerometer();
        break;
    case 9U:
        stop_work_time_test();
        AppCalibrationCalibrateRefAngle();
        break;
    default:
        break;
    }
}

static void note_mode_short_click(uint32_t now_ms)
{
#if APP_WORK_TIME_TEST_MODE_ENABLE
    if (g_work_time_test_active)
    {
        return;
    }
#endif

    if ((g_mode_click_count == 0U) ||
        ((now_ms - g_mode_click_last_ms) > APP_MODE_MULTI_CLICK_MS))
    {
        g_mode_click_count = 0U;
    }

    if (g_mode_click_count < 9U)
    {
        ++g_mode_click_count;
    }
    g_mode_click_last_ms = now_ms;
}

static void close_mode_click_window(uint32_t now_ms)
{
    if ((g_mode_click_count == 0U) ||
        ((now_ms - g_mode_click_last_ms) < APP_MODE_MULTI_CLICK_MS))
    {
        return;
    }

    const uint8_t count = g_mode_click_count;
    g_mode_click_count = 0U;
    handle_mode_click_count(count);
}

static void handle_power_short(void)
{
    const AppWorkMode mode = AppState_GetMode();

    if (AppCalibrationMagActive())
    {
        return;
    }

#if APP_WORK_TIME_TEST_MODE_ENABLE
    if (mode == APP_MODE_WORK_TIME_TEST)
    {
        if (g_work_time_test_active)
        {
            g_work_time_test_active = false;
            AppRangePowerSet(false);
        }
        else
        {
            g_work_time_test_active = true;
            g_work_time_test_next_ms = AppTickMs();
        }
        return;
    }
#endif

    if ((mode == APP_MODE_CONTINUOUS) && AppMeasureContinuousStarted())
    {
        AppRangePowerSet(false);
        return;
    }

    if (mode == APP_MODE_CONTINUOUS)
    {
        if (AppMeasurePending() || AppRangePowered())
        {
            AppRangePowerSet(false);
        }
        else
        {
            AppMeasureStart(APP_MODE_CONTINUOUS);
        }
        return;
    }

    AppMeasureStart(mode);
}

static void power_off_sequence(bool save_measure_count)
{
    stop_work_time_test();
    AppCalibrationAbortActive();
    AppModulesOffForSleep();
    if (save_measure_count && !MeasureCounter_Save())
    {
        APP_LOGE("counter", "failed to save measure count");
    }
    Board_PowerHold(false);
}

static bool battery_low_voltage(const BatteryData* battery)
{
    return (battery != 0) &&
           battery->valid &&
           (battery->voltage_mv < APP_BAT_EMPTY_MV);
}

void AppPowerOffWaitForever(bool save_measure_count)
{
    power_off_sequence(save_measure_count);
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000U));
    }
}

void AppPowerOffIfBatteryLow(const char* module, const BatteryData* battery)
{
    if (!battery_low_voltage(battery))
    {
        return;
    }

    APP_LOGE(module,
             "battery low %u mV < %u mV, power off",
             (unsigned int)battery->voltage_mv,
             (unsigned int)APP_BAT_EMPTY_MV);
    AppPowerOffWaitForever(false);
}

#if APP_WORK_TIME_TEST_MODE_ENABLE
static void update_work_time_test(uint32_t now_ms)
{
    if ((!g_work_time_test_active) ||
        (AppState_GetMode() != APP_MODE_WORK_TIME_TEST) ||
        AppMeasurePending() || AppRangePowered())
    {
        return;
    }

    if ((int32_t)(now_ms - g_work_time_test_next_ms) >= 0)
    {
        g_work_time_test_next_ms = now_ms + APP_WORK_TIME_TEST_INTERVAL_MS;
        AppMeasureStart(APP_MODE_WORK_TIME_TEST);
    }
}
#endif

void AppControlTask(void* argument)
{
    BatteryData battery;
    uint32_t last_battery_ms = 0U;
#if APP_AUTO_POWER_OFF_ENABLE
    uint32_t last_activity_ms = AppTickMs();
#endif

    (void)argument;
    Keys_Init();
    AppApplyModePower(AppState_GetMode());
    vTaskDelay(pdMS_TO_TICKS(1U));

    while (1)
    {
        const uint32_t now_ms = AppTickMs();
        const KeyEvent event = Keys_Poll(now_ms);

#if APP_AUTO_POWER_OFF_ENABLE
        if (event != KEY_EVENT_NONE)
        {
            last_activity_ms = now_ms;
        }
#endif

        if (AppCalibrationSettingsActive())
        {
            AppCalibrationHandleSettingsInput(event, now_ms);
        }
        else if (AppCalibrationHandleExitWaitRelease())
        {
        }
        else if (event == KEY_EVENT_POWER_LONG)
        {
            AppPowerOffWaitForever(true);
        }
        else if (event == KEY_EVENT_MODE_SHORT)
        {
            note_mode_short_click(now_ms);
        }
        else if (event == KEY_EVENT_POWER_SHORT)
        {
            handle_power_short();
        }

        AppMeasureCloseIfDone(AppTickMs());
#if APP_WORK_TIME_TEST_MODE_ENABLE
        if (!AppCalibrationSettingsActive())
        {
            update_work_time_test(AppTickMs());
        }
#endif
        if (!AppCalibrationSettingsActive())
        {
            close_mode_click_window(AppTickMs());
        }

        if ((now_ms - last_battery_ms) >= 1000U)
        {
            last_battery_ms = now_ms;
            Battery_Read(&battery);
            AppState_UpdateBattery(&battery);
            AppPowerOffIfBatteryLow("battery", &battery);
        }

#if APP_AUTO_POWER_OFF_ENABLE
        if (
#if APP_WORK_TIME_TEST_MODE_ENABLE
            (!g_work_time_test_active) &&
#endif
            ((now_ms - last_activity_ms) >= APP_AUTO_POWER_OFF_MS))
        {
            AppPowerOffWaitForever(true);
        }
#endif

        vTaskDelay(pdMS_TO_TICKS(20U));
    }
}
