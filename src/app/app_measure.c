#include "app_internal.h"

#include "app_log.h"
#include "app_state.h"
#include "board_config.h"
#include "bsp_uart.h"
#include "measure_counter.h"
#include "rangefinder.h"
#include "task.h"

static bool g_continuous_started;
static bool g_measure_pending;
static AppWorkMode g_measure_mode = APP_MODE_SINGLE;
static uint32_t g_measure_start_ms;
static uint32_t g_continuous_next_measure_ms;
static bool g_range_cycle_active;
static bool g_range_cycle_has_first;
static bool g_range_cycle_has_last;
static uint8_t g_range_cycle_last_index;
static uint8_t g_range_cycle_max_index;
static uint32_t g_range_cycle_last_rx_ms;
static uint32_t g_range_cycle_first_mm;
static uint32_t g_range_cycle_last_mm;
static uint8_t g_range_cycle_last_status;
static uint32_t g_range_rx_bytes_since_start;
static uint32_t g_range_frames_since_start;
static bool g_range_command_ack_received;
static volatile uint8_t g_range_last_ack_command;

static void range_cycle_reset(void)
{
    g_range_cycle_active = false;
    g_range_cycle_has_first = false;
    g_range_cycle_has_last = false;
    g_range_cycle_last_index = 0U;
    g_range_cycle_max_index = 0U;
    g_range_cycle_last_rx_ms = 0U;
    g_range_cycle_first_mm = 0U;
    g_range_cycle_last_mm = 0U;
    g_range_cycle_last_status = 0U;
    g_range_rx_bytes_since_start = 0U;
    g_range_frames_since_start = 0U;
    g_range_command_ack_received = false;
    g_range_last_ack_command = 0U;
}

void AppMeasureResetOnRangePowerOff(void)
{
    g_continuous_started = false;
    g_measure_pending = false;
    g_measure_mode = APP_MODE_SINGLE;
    g_continuous_next_measure_ms = 0U;
    range_cycle_reset();
}

bool AppMeasurePending(void)
{
    return g_measure_pending;
}

bool AppMeasureContinuousStarted(void)
{
    return g_continuous_started;
}

bool AppMeasureRangeResultCurrent(const AppStateSnapshot* snapshot)
{
    return (snapshot != 0) &&
           snapshot->range.valid &&
           (snapshot->range.app_mode == (uint8_t)snapshot->mode) &&
           ((!g_measure_pending) || (snapshot->range.update_ms >= g_measure_start_ms));
}

static bool wait_range_ack(uint8_t command, uint32_t timeout_ms)
{
    const uint32_t start_ms = AppTickMs();

    while ((AppTickMs() - start_ms) < timeout_ms)
    {
        if (g_range_last_ack_command == command)
        {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(5U));
    }

    return false;
}

static void publish_range_result(uint32_t now_ms)
{
    RangefinderData result;

    result.valid = true;
    result.command = 0x02U;
    result.distance_mm = g_range_cycle_has_first ? g_range_cycle_first_mm :
        (g_range_cycle_has_last ? g_range_cycle_last_mm : 0U);
    result.first_distance_mm = g_range_cycle_first_mm;
    result.last_distance_mm = g_range_cycle_last_mm;
    result.status = g_range_cycle_last_status;
    result.target_index = 0U;
    result.first_valid = g_range_cycle_has_first;
    result.last_valid = g_range_cycle_has_last;
    result.self_status[0] = 0U;
    result.self_status[1] = 0U;
    result.self_status[2] = 0U;
    result.self_status[3] = 0U;
    result.self_test_ok = false;
    result.continuous = g_continuous_started;
    result.update_ms = now_ms;
    result.app_mode = (uint8_t)g_measure_mode;

    AppState_UpdateRange(&result);
    AppState_SetMeasureCount(MeasureCounter_Increment());

    if (g_continuous_started)
    {
        g_measure_pending = false;
        g_continuous_next_measure_ms = g_measure_start_ms + APP_RANGE_CONTINUOUS_INTERVAL_MS;
    }

    range_cycle_reset();
}

static void update_range_cycle(const RangefinderData* data, uint32_t now_ms)
{
    if ((data == 0) || ((data->command != 0x02U) && (data->command != 0x04U)))
    {
        return;
    }

    if (g_continuous_started && g_range_cycle_active &&
        (data->target_index == 0U) && (g_range_cycle_last_index != 0U))
    {
        publish_range_result(now_ms);
    }

    if (!g_range_cycle_active)
    {
        g_range_cycle_active = true;
        g_range_cycle_max_index = 0U;
    }

    g_range_cycle_last_rx_ms = now_ms;
    g_range_cycle_last_index = data->target_index;
    g_range_cycle_last_status = data->status;

    if ((!data->first_valid) && (!data->last_valid))
    {
        return;
    }

    if (data->first_valid && (!g_range_cycle_has_first))
    {
        g_range_cycle_has_first = true;
        g_range_cycle_first_mm = data->distance_mm;
    }

    if (data->last_valid &&
        ((!g_range_cycle_has_last) || (data->target_index >= g_range_cycle_max_index)))
    {
        g_range_cycle_has_last = true;
        g_range_cycle_max_index = data->target_index;
        g_range_cycle_last_mm = data->distance_mm;
    }
}

void AppMeasureUpdateRangeFromUart(uint32_t now_ms)
{
    uint8_t byte;
    RangefinderData data;

    while (BspUart_ReadByte(BSP_UART_RANGE, &byte))
    {
        if (g_measure_pending || g_continuous_started)
        {
            ++g_range_rx_bytes_since_start;
        }

        if (Rangefinder_ProcessByte(byte, &data))
        {
            if (g_measure_pending || g_continuous_started)
            {
                ++g_range_frames_since_start;
            }
            data.update_ms = now_ms;
            if (data.valid && ((data.command == 0x02U) || (data.command == 0x04U)))
            {
                update_range_cycle(&data, now_ms);
            }
            else if ((!data.valid) && ((data.command == 0x02U) || (data.command == 0x04U)))
            {
                g_range_command_ack_received = true;
                g_range_last_ack_command = data.command;
            }
            else if (!data.valid)
            {
                g_range_last_ack_command = data.command;
            }
            else if (data.valid)
            {
                AppState_UpdateRange(&data);
            }
        }
    }
}

void AppMeasureStart(AppWorkMode mode)
{
    g_measure_mode = mode;
    g_measure_pending = true;
    range_cycle_reset();
    AppState_ClearRange();

    if (AppModeUsesMultifunction(mode))
    {
        AppImuPowerSet(true);
        AppGnssPowerSet(true);
    }
    else
    {
        AppImuPowerSet(false);
        AppGnssPowerSet(false);
    }

    AppRangePowerSet(true);

    g_range_last_ack_command = 0U;
    Rangefinder_SetTargetMode(RANGE_TARGET_MULTI);
    (void)wait_range_ack(0x03U, APP_RANGE_COMMAND_ACK_TIMEOUT_MS);
    g_measure_start_ms = AppTickMs();
    g_range_last_ack_command = 0U;

    if (mode == APP_MODE_CONTINUOUS)
    {
        g_continuous_started = true;
        g_continuous_next_measure_ms = 0U;
    }

    Rangefinder_StartSingle();
}

void AppMeasureCloseIfDone(uint32_t now_ms)
{
    if (g_continuous_started)
    {
        if (g_measure_pending &&
            g_range_cycle_active &&
            ((now_ms - g_range_cycle_last_rx_ms) >= APP_RANGE_CONTINUOUS_GAP_TIMEOUT_MS))
        {
            publish_range_result(now_ms);
            return;
        }

        if (g_measure_pending &&
            ((now_ms - g_measure_start_ms) >= APP_RANGE_CONTINUOUS_TIMEOUT_MS))
        {
            if (!g_range_command_ack_received)
            {
                APP_LOGW("control", "continuous range timeout, rx_bytes=%u frames=%u",
                         (unsigned int)g_range_rx_bytes_since_start,
                         (unsigned int)g_range_frames_since_start);
            }
            publish_range_result(now_ms);
            return;
        }

        if ((!g_measure_pending) &&
            ((int32_t)(now_ms - g_continuous_next_measure_ms) >= 0))
        {
            g_measure_pending = true;
            range_cycle_reset();
            AppState_ClearRange();
            g_measure_start_ms = now_ms;
            g_range_last_ack_command = 0U;
            Rangefinder_StartSingle();
        }
        return;
    }

    if (!g_measure_pending)
    {
        return;
    }

    if (g_range_cycle_active &&
        ((now_ms - g_range_cycle_last_rx_ms) >= APP_RANGE_SINGLE_GAP_TIMEOUT_MS))
    {
        publish_range_result(now_ms);
        AppFinishMeasurementPower();
    }
    else
    {
        const uint32_t timeout_ms = AppModeUsesMultifunction(g_measure_mode) ?
            APP_MULTI_MEASURE_TIMEOUT_MS : APP_RANGE_SINGLE_TIMEOUT_MS;

        if ((now_ms - g_measure_start_ms) >= timeout_ms)
        {
            const AppWorkMode finished_mode = g_measure_mode;

            if (!g_range_command_ack_received)
            {
                APP_LOGW("control", "%s range timeout, rx_bytes=%u frames=%u",
                         AppModeUsesMultifunction(finished_mode) ? "multi" : "single",
                         (unsigned int)g_range_rx_bytes_since_start,
                         (unsigned int)g_range_frames_since_start);
            }
            publish_range_result(now_ms);
            AppFinishMeasurementPower();
        }
    }
}
