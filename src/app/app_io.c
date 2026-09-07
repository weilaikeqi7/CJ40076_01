#include "app_internal.h"

#include "app_state.h"
#include "bsp_uart.h"
#include "bv220.h"
#include "jy901b.h"
#include "rangefinder.h"
#include "task.h"

void AppUpdateImuFromUart(uint32_t now_ms)
{
    uint8_t byte;
    OrientationData data;

    while (BspUart_ReadByte(BSP_UART_IMU, &byte))
    {
        if (Jy901b_ProcessByte(byte, &data))
        {
            if (!AppImuPowered())
            {
                continue;
            }
            AppCalibrationApplyOrientationOffsets(&data);
            data.update_ms = now_ms;
            AppState_UpdateOrientation(&data);
        }
    }
}

void AppUpdateGnssFromUart(uint32_t now_ms)
{
    uint8_t byte;
    GnssData data;

    while (BspUart_ReadByte(BSP_UART_GNSS, &byte))
    {
        if (Bv220_ProcessByte(byte, &data))
        {
            if (!AppGnssPowered())
            {
                continue;
            }
            data.update_ms = now_ms;
            AppState_UpdateGnss(&data);
        }
    }
}

void AppIoTask(void* argument)
{
    (void)argument;

    Bv220_Reset();
    Jy901b_Reset();
    Rangefinder_Reset();

    while (1)
    {
        const uint32_t now_ms = AppTickMs();
        AppState_SetUptime(now_ms);

        AppMeasureUpdateRangeFromUart(now_ms);
        AppUpdateImuFromUart(now_ms);
        AppUpdateGnssFromUart(now_ms);
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
