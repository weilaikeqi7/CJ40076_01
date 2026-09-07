#include "app_internal.h"

#include "board.h"
#include "board_config.h"
#include "bsp_uart.h"
#include "bv220.h"
#include "jy901b.h"
#include "rangefinder.h"
#include "task.h"

static bool g_range_powered;
static bool g_imu_powered;
static bool g_gnss_powered;

uint32_t AppTickMs(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

int32_t AppNormalizeYawCentidegree(int32_t yaw_cd)
{
    while (yaw_cd > 18000)
    {
        yaw_cd -= 36000;
    }
    while (yaw_cd <= -18000)
    {
        yaw_cd += 36000;
    }
    return yaw_cd;
}

int32_t AppNormalizeDegrees(int32_t degrees)
{
    while (degrees < 0)
    {
        degrees += 360;
    }
    while (degrees >= 360)
    {
        degrees -= 360;
    }
    return degrees;
}

bool AppModeUsesMultifunction(AppWorkMode mode)
{
#if APP_WORK_TIME_TEST_MODE_ENABLE
    return (mode == APP_MODE_MULTI) || (mode == APP_MODE_WORK_TIME_TEST);
#else
    return mode == APP_MODE_MULTI;
#endif
}

void AppRangePowerSet(bool enabled)
{
    if (enabled == g_range_powered)
    {
        return;
    }

    if (enabled)
    {
        BspUart_Reinit(BSP_UART_RANGE);
        Board_SetRangePower(true);
        BspUart_FlushRx(BSP_UART_RANGE);
        Rangefinder_Reset();
    }
    else
    {
        Board_SetRangePower(false);
        AppMeasureResetOnRangePowerOff();
    }

    g_range_powered = enabled;
}

bool AppRangePowered(void)
{
    return g_range_powered;
}

void AppImuPowerSet(bool enabled)
{
    if (enabled == g_imu_powered)
    {
        return;
    }

    if (enabled)
    {
        AppState_ClearOrientation();
        BspUart_Reinit(BSP_UART_IMU);
        Board_SetImuPower(true);
        BspUart_FlushRx(BSP_UART_IMU);
        Jy901b_Reset();
        AppState_ClearOrientation();
        g_imu_powered = true;
    }
    else
    {
        g_imu_powered = false;
        Board_SetImuPower(false);
        BspUart_FlushRx(BSP_UART_IMU);
        Jy901b_Reset();
        AppState_ClearOrientation();
    }
}

bool AppImuPowered(void)
{
    return g_imu_powered;
}

void AppGnssPowerSet(bool enabled)
{
    if (enabled == g_gnss_powered)
    {
        return;
    }

    if (enabled)
    {
        AppState_ClearGnss();
        BspUart_Reinit(BSP_UART_GNSS);
        Board_SetGnssPower(true);
        BspUart_FlushRx(BSP_UART_GNSS);
        Bv220_Reset();
        AppState_ClearGnss();
        g_gnss_powered = true;
    }
    else
    {
        g_gnss_powered = false;
        Board_SetGnssPower(false);
        BspUart_FlushRx(BSP_UART_GNSS);
        Bv220_Reset();
        AppState_ClearGnss();
    }
}

bool AppGnssPowered(void)
{
    return g_gnss_powered;
}

void AppModulesOffForSleep(void)
{
    AppRangePowerSet(false);
    AppImuPowerSet(false);
    AppGnssPowerSet(false);
}

void AppApplyModePower(AppWorkMode mode)
{
    AppRangePowerSet(false);
    AppImuPowerSet(AppModeUsesMultifunction(mode));
    AppGnssPowerSet(AppModeUsesMultifunction(mode));
}

void AppFinishMeasurementPower(void)
{
    AppApplyModePower(AppState_GetMode());
}
