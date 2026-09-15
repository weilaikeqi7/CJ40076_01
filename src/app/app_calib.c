/**
 * @file app_calib.c
 * @brief 校准与补偿设置状态机实现
 */
#include "app_calib.h"

#include "app_attitude.h"
#include "app_config.h"
#include "board.h"
#include "board_uart.h"
#include "lcd_segments.h"
#include "mcp406.h"
#include "rtt_log.h"

#include "FreeRTOS.h"
#include "task.h"

static calib_state_t state = CALIB_NONE;
static app_offsets_t work; /* 页内编辑中的补偿值（实时生效，保存才落 Flash） */

/** 进入校准前确保电子罗盘已上电并等待启动（单次/连续模式下它可能断电） */
static void ensure_imu_on(void)
{
    board_compass_power(true);
    vTaskDelay(pdMS_TO_TICKS(300U));
    board_uart_flush_rx(BOARD_UART_COMPASS);
}

static int16_t* page_ptr(void)
{
    switch (state)
    {
    case CALIB_PIT:
        return &work.pit_c01;
    case CALIB_HIT:
        return &work.hit_c01;
    case CALIB_HER:
        return &work.her_c01;
    default:
        return &work.her_c01;
    }
}

static int16_t page_max(void)
{
    return state == CALIB_PIT ? APP_PIT_MAX_C01 : APP_HIT_MAX_C01;
}

static void page_enter(calib_state_t page)
{
    attitude_get_offsets(&work);
    state = page;
    app_key_set_calib_mode(true);
    LOGI("calib: enter page %d\r\n", (int)page);
}

static void page_exit(bool save)
{
    if (save)
    {
        (void)store_save_offsets(&work); /* 保存 Flash（attitude RAM 已实时生效） */
        LOGI("calib: saved, exit\r\n");
    }
    else
    {
        attitude_load_offsets(); /* 放弃未保存修改 */
        LOGI("calib: discard, exit\r\n");
    }
    state = CALIB_NONE;
    app_key_set_calib_mode(false);
}

static void page_adjust(int16_t delta_c01)
{
    int16_t* v   = page_ptr();
    int16_t  max = page_max();
    int32_t  nv  = (int32_t)*v + delta_c01;

    if (nv > max)
    {
        nv = max;
    }
    if (nv < -max)
    {
        nv = -max;
    }
    *v = (int16_t)nv;

    attitude_set_offsets(&work); /* 实时生效 */
}

bool calib_handle_key(const app_key_event_t* evt)
{
    /* ---------- 正常运行态：多击入口 ---------- */
    if (state == CALIB_NONE)
    {
        if ((evt->evt & APP_KEY_EVT_MODE_CLICKS) != 0U)
        {
            switch (evt->arg)
            {
            case 4: /* 四击：HEr */
                ensure_imu_on();
                page_enter(CALIB_HER);
                return true;
            case 5: /* 五击：磁场空间手动校准开始（12 点，每姿态电源键采样） */
                ensure_imu_on();
                state = CALIB_MAG;
                app_key_set_calib_mode(true);
                mcp406_calib_mag_start();
                LOGI("calib: mag calibration started (mode=10 space manual)\r\n");
                return true;
            case 6: /* 六击：仅磁场校准中有效（此处 NONE 态忽略） */
                return true;
            case 7: /* 七击：PIt */
                ensure_imu_on();
                page_enter(CALIB_PIT);
                return true;
            case 8: /* 八击：加速度校准（阻塞约 4.5s，先全显再阻塞） */
                ensure_imu_on();
                state = CALIB_ACC_BUSY;
                LcdSegments_SetAll(true);
                LcdSegments_Flush();
                mcp406_calib_accel();
                state = CALIB_NONE;
                LOGI("calib: accel calibration done\r\n");
                return true;
            case 9: /* 九击：角度校准（阻塞约 3.5s，先全显再阻塞） */
                ensure_imu_on();
                state = CALIB_ANG_BUSY;
                LcdSegments_SetAll(true);
                LcdSegments_Flush();
                mcp406_set_angle_ref();
                state = CALIB_NONE;
                LOGI("calib: angle reference done\r\n");
                return true;
            case 10: /* 十击：MCP-406 恢复出厂设置（先全显再阻塞） */
                ensure_imu_on();
                state = CALIB_ANG_BUSY;
                LcdSegments_SetAll(true);
                LcdSegments_Flush();
                mcp406_factory_reset();
                state = CALIB_NONE;
                LOGI("calib: factory reset done\r\n");
                return true;
            default:
                break;
            }
        }
        return false;
    }

    /* ---------- 磁场校准进行中或采满完成状态 ---------- */
    if (state == CALIB_MAG || state == CALIB_MAG_DONE)
    {
        /* 六击：直接发送停止校准命令（00 05 0B 4E 9E），退出校准 */
        if ((evt->evt & APP_KEY_EVT_MODE_CLICKS) != 0U && evt->arg == 6U)
        {
            mcp406_stop_cal();
            state = CALIB_NONE;
            app_key_set_calib_mode(false);
            LOGI("calib: 6 clicks -> sent StopCal (00 05 0B 4E 9E) and exited calib\r\n");
            return true;
        }

        /* 电源键短按：当前姿态采集一个采样点（手动校准） */
        if (state == CALIB_MAG && (evt->evt & APP_KEY_EVT_POWER_SHORT) != 0U)
        {
            mcp406_take_cal_sample();
            return true;
        }

        /* 同时长按电源键和模式键 1s：保存参数并退出校准 */
        if ((evt->evt & APP_KEY_EVT_BOTH_LONG) != 0U)
        {
            mcp406_save();
            state = CALIB_NONE;
            app_key_set_calib_mode(false);
            LOGI("calib: both keys 1s pressed -> saved and exited mag calib\r\n");
            return true;
        }

        return true; /* 校准中消费所有按键（长按关机由 app 层先判） */
    }

    /* ---------- 设置页（PIt/HIt/HEr） ---------- */
    if ((evt->evt & APP_KEY_EVT_BOTH_LONG) != 0U)
    {
        if (state == CALIB_PIT)
        {
            /* PIt -> HIt 切页（不保存，退出时统一保存） */
            state = CALIB_HIT;
            LOGI("calib: PIt -> HIt\r\n");
        }
        else
        {
            page_exit(true); /* HIt/HEr 页双键长按：保存并退出 */
        }
        return true;
    }

    if ((evt->evt & (APP_KEY_EVT_POWER_SHORT | APP_KEY_EVT_POWER_REPEAT)) != 0U)
    {
        page_adjust(10); /* +0.1° */
        return true;
    }
    if ((evt->evt & (APP_KEY_EVT_MODE_SINGLE | APP_KEY_EVT_MODE_REPEAT)) != 0U)
    {
        page_adjust(-10); /* -0.1° */
        return true;
    }

    return true; /* 设置页内消费所有按键 */
}

calib_state_t calib_get_state(void)
{
    /* 在无按键事件时，主动检查底层 mcp406 是否已收到 CalScore 完成帧 */
    if (state == CALIB_MAG && mcp406_is_cal_done())
    {
        state = CALIB_MAG_DONE;
        LOGI("calib: auto detected mag cal done -> state=CALIB_MAG_DONE\r\n");
    }
    return state;
}

int16_t calib_page_value_c01(void)
{
    if (state >= CALIB_PIT && state <= CALIB_HER)
    {
        return *page_ptr();
    }
    return 0;
}

bool calib_mag_in_progress(void)
{
    return state == CALIB_MAG || state == CALIB_MAG_DONE;
}

bool calib_mag_is_done(void)
{
    return state == CALIB_MAG_DONE;
}

void calib_mag_abort(void)
{
    if (state == CALIB_MAG || state == CALIB_MAG_DONE)
    {
        mcp406_stop_cal();
        state = CALIB_NONE;
        app_key_set_calib_mode(false);
        LOGI("calib: mag calib aborted\r\n");
    }
}

bool calib_page_active(void)
{
    return state >= CALIB_PIT && state <= CALIB_HER;
}
