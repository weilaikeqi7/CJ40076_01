/**
 * @file mcp406.c
 * @brief MCP-406 高精度三维电子罗盘驱动实现
 */
#include "mcp406.h"

#include "board.h"
#include "board_uart.h"

#include "FreeRTOS.h"
#include "rtt_log.h"
#include "task.h"

#include <string.h>

#define MCP406_UART BOARD_UART_COMPASS

#define MCP406_RX_BUF_SIZE 128U
#define MCP406_FRAME_MIN_LEN 5U
#define MCP406_FRAME_MAX_LEN 128U

static mcp406_data_t mcp406_data;
static volatile bool  mcp406_cal_done_flag   = false;
static volatile bool  mcp406_cal_running     = false;
static uint32_t       mcp406_cal_total_pts   = 42U; /* 当前校准总点数（方式60默认为42） */

/* ------------------------------ CRC 校验 ------------------------------ */

/**
 * @brief 计算 MCP-406 CRC-16 校验码（手册第 377 行算法）
 *        多项式: 0x1021, 初值: 0x0000
 */
static uint16_t mcp406_calc_crc(const uint8_t* data, uint32_t len)
{
    unsigned char  i;
    const uint8_t* ptr = data;
    unsigned int   crc = 0U;
    uint32_t       index = len;

    while (index--)
    {
        for (i = 0x80U; i != 0U; i = (unsigned char)(i >> 1U))
        {
            if ((crc & 0x8000U) != 0U)
            {
                crc = (crc << 1U) ^ 0x1021U;
            }
            else
            {
                crc = crc << 1U;
            }
            if ((*ptr & i) != 0U)
            {
                crc ^= 0x1021U;
            }
        }
        ptr++;
    }
    return (uint16_t)(crc & 0xFFFFU);
}

/* ------------------------------ 大端解析 ------------------------------ */

static float parse_be_float(const uint8_t* p)
{
    union
    {
        uint32_t u;
        float    f;
    } conv;

    conv.u = ((uint32_t)p[0] << 24) |
             ((uint32_t)p[1] << 16) |
             ((uint32_t)p[2] << 8)  |
             ((uint32_t)p[3]);
    return conv.f;
}

static uint32_t parse_be_u32(const uint8_t* p)
{
    return ((uint32_t)p[0] << 24) |
           ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  |
           ((uint32_t)p[3]);
}

/* ------------------------------ 命令发送 ------------------------------ */

static void mcp406_send_cmd(uint8_t id, const uint8_t* payload, uint16_t payload_len)
{
    uint8_t  buf[64];
    uint16_t total_len = (uint16_t)(5U + payload_len);
    uint16_t crc;

    if (total_len > sizeof(buf))
    {
        return;
    }

    buf[0] = (uint8_t)(total_len >> 8);
    buf[1] = (uint8_t)(total_len & 0xFFU);
    buf[2] = id;

    if (payload != NULL && payload_len > 0U)
    {
        memcpy(&buf[3], payload, payload_len);
    }

    crc = mcp406_calc_crc(buf, (uint32_t)(3U + payload_len));
    buf[3U + payload_len] = (uint8_t)(crc >> 8);
    buf[4U + payload_len] = (uint8_t)(crc & 0xFFU);

    board_uart_write(MCP406_UART, buf, total_len);
}

/* ------------------------------ 帧解析 ------------------------------ */

static void mcp406_handle_frame(uint8_t id, const uint8_t* payload, uint16_t payload_len)
{
    uint32_t now = xTaskGetTickCount();

    switch (id)
    {
    case MCP406_CMD_MOD_INFO_RESP: /* 0x02: 获取罗盘型号与硬件版本响应 */
        {
            char info_buf[32];
            uint16_t cpy_len = payload_len < (sizeof(info_buf) - 1U) ? payload_len : (sizeof(info_buf) - 1U);
            if (cpy_len > 0U)
            {
                memcpy(info_buf, payload, cpy_len);
            }
            info_buf[cpy_len] = '\0';
            LOGI("mcp406: device model/version: [%s]\r\n", info_buf);
        }
        break;

    case MCP406_CMD_GET_DATA_RESP: /* 0x05: 数据查询/广播响应 */
        if (payload_len >= 1U)
        {
            uint8_t  count  = payload[0];
            uint16_t offset = 1U;

            for (uint8_t i = 0U; i < count && offset < payload_len; i++)
            {
                uint8_t item_id = payload[offset++];
                switch (item_id)
                {
                case MCP406_DATA_HEADING: /* 5: 方位角 (Float32) */
                    if ((offset + 4U) <= payload_len)
                    {
                        mcp406_data.heading    = parse_be_float(&payload[offset]);
                        mcp406_data.tick_angle = now;
                        offset += 4U;
                    }
                    break;

                case MCP406_DATA_PITCH: /* 24: 俯仰角 (Float32) */
                    if ((offset + 4U) <= payload_len)
                    {
                        mcp406_data.pitch      = parse_be_float(&payload[offset]);
                        mcp406_data.tick_angle = now;
                        offset += 4U;
                    }
                    break;

                case MCP406_DATA_ROLL: /* 25: 横滚角 (Float32) */
                    if ((offset + 4U) <= payload_len)
                    {
                        mcp406_data.roll       = parse_be_float(&payload[offset]);
                        mcp406_data.tick_angle = now;
                        offset += 4U;
                    }
                    break;

                case MCP406_DATA_TEMPERATURE: /* 7: 温度 (Float32) */
                    if ((offset + 4U) <= payload_len)
                    {
                        mcp406_data.temp_c = parse_be_float(&payload[offset]);
                        offset += 4U;
                    }
                    break;

                case MCP406_DATA_DISTORTION: /* 8: 畸变 (Boolean 1B) */
                    if (offset < payload_len)
                    {
                        mcp406_data.distortion = (payload[offset++] != 0U);
                    }
                    break;

                case MCP406_DATA_CAL_STATUS: /* 9: 校准状态 (Boolean 1B) */
                    if (offset < payload_len)
                    {
                        mcp406_data.cal_status = (payload[offset++] != 0U);
                    }
                    break;

                case MCP406_DATA_ACCEL_X:
                case MCP406_DATA_ACCEL_Y:
                case MCP406_DATA_ACCEL_Z:
                    if ((offset + 4U) <= payload_len)
                    {
                        float a = parse_be_float(&payload[offset]);
                        if (item_id == MCP406_DATA_ACCEL_X)
                        {
                            mcp406_data.acc_x = a;
                        }
                        else if (item_id == MCP406_DATA_ACCEL_Y)
                        {
                            mcp406_data.acc_y = a;
                        }
                        else
                        {
                            mcp406_data.acc_z = a;
                        }
                        mcp406_data.tick_acc = now;
                        offset += 4U;
                    }
                    break;

                case MCP406_DATA_MAG_X:
                case MCP406_DATA_MAG_Y:
                case MCP406_DATA_MAG_Z:
                    if ((offset + 4U) <= payload_len)
                    {
                        float m = parse_be_float(&payload[offset]);
                        if (item_id == MCP406_DATA_MAG_X)
                        {
                            mcp406_data.mag_x = m;
                        }
                        else if (item_id == MCP406_DATA_MAG_Y)
                        {
                            mcp406_data.mag_y = m;
                        }
                        else
                        {
                            mcp406_data.mag_z = m;
                        }
                        mcp406_data.tick_mag = now;
                        offset += 4U;
                    }
                    break;

                default:
                    /* 其他未处理项按 4 字节跳过 */
                    if ((offset + 4U) <= payload_len)
                    {
                        offset += 4U;
                    }
                    break;
                }
            }

            long h_x100 = (long)(mcp406_data.heading * 100.0f);
            long p_x100 = (long)(mcp406_data.pitch * 100.0f);
            long r_x100 = (long)(mcp406_data.roll * 100.0f);
            LOGI("mcp406: data head=%ld.%02ld pit=%ld.%02ld roll=%ld.%02ld\r\n",
                 h_x100 / 100, (h_x100 >= 0 ? h_x100 : -h_x100) % 100,
                 p_x100 / 100, (p_x100 >= 0 ? p_x100 : -p_x100) % 100,
                 r_x100 / 100, (r_x100 >= 0 ? r_x100 : -r_x100) % 100);
        }
        break;

    case MCP406_CMD_USER_CAL_SAMP_COUNT: /* 17 / 0x11: 校准采样点数 */
        if (payload_len >= 4U)
        {
            mcp406_data.cal_sample_cnt = parse_be_u32(&payload[0]);
        }
        else if (payload_len >= 2U)
        {
            mcp406_data.cal_sample_cnt = ((uint32_t)payload[0] << 8) | payload[1];
        }
        else if (payload_len >= 1U)
        {
            mcp406_data.cal_sample_cnt = (uint32_t)payload[0];
        }
        if (mcp406_data.cal_sample_cnt > mcp406_cal_total_pts)
        {
            mcp406_cal_total_pts = mcp406_data.cal_sample_cnt;
        }
        LOGI("calib point: %lu / %lu\r\n", (unsigned long)mcp406_data.cal_sample_cnt,
             (unsigned long)mcp406_cal_total_pts);
        break;

    case MCP406_CMD_CAL_SCORE: /* 18 (0x12): 校准得分，帧格式 00 1D 12 + 6×Float32 + CRC */
        if (payload_len >= 24U)
        {
            mcp406_data.cal_mag_score   = parse_be_float(&payload[0]);
            mcp406_data.cal_accel_score = parse_be_float(&payload[8]);
            mcp406_cal_done_flag        = true;
            mcp406_cal_running          = false;

            /* 若结束时已采点数与总点数不一致，以实际完成点数为准对齐 */
            if (mcp406_data.cal_sample_cnt > 0U)
            {
                mcp406_cal_total_pts = mcp406_data.cal_sample_cnt;
            }

            /* 得分按 2025.08.12 手册第 14 条评价（<0.22优 / 0.22~0.42良 / 0.42~0.72中 /
             * 0.72~1.02差 / 35=磁干扰较强 / 99.9=校准无效磁干扰太强 /
             * 200=未开展此校准 / 400=未进入校准） */
            long mag_x100 = (long)(mcp406_data.cal_mag_score * 100.0f);
            long acc_x100 = (long)(mcp406_data.cal_accel_score * 100.0f);
            LOGI("calib: DONE! total=%lu, mag_score=%ld.%02ld, accel_score=%ld.%02ld\r\n",
                 (unsigned long)mcp406_cal_total_pts,
                 mag_x100 / 100, (mag_x100 >= 0 ? mag_x100 : -mag_x100) % 100,
                 acc_x100 / 100, (acc_x100 >= 0 ? acc_x100 : -acc_x100) % 100);

            if (mcp406_data.cal_mag_score >= 99.0f && mcp406_data.cal_mag_score < 100.0f)
            {
                LOGI("calib: mag_score=99.9 -> calibration INVALID, magnetic interference too strong!\r\n");
            }
        }
        break;

    case MCP406_CMD_SAVE_DONE: /* 16: 保存完成响应 */
        LOGI("mcp406: save done\r\n");
        break;

    case MCP406_CMD_SET_CONFIG_DONE: /* 19: 设置完成响应 */
        LOGI("mcp406: config done\r\n");
        break;

    default:
        LOGI("mcp406: rx frame id=0x%02X len=%u\r\n", (unsigned int)id, (unsigned int)payload_len);
        break;
    }
}

void mcp406_poll(void)
{
    static uint8_t  rx_buf[MCP406_RX_BUF_SIZE];
    static uint16_t rx_len = 0U;
    uint8_t         byte;

    while (board_uart_read(MCP406_UART, &byte, 1U) == 1U)
    {
        if (rx_len < MCP406_RX_BUF_SIZE)
        {
            rx_buf[rx_len++] = byte;
        }
        else
        {
            /* 满载滑动防死锁 */
            memmove(rx_buf, rx_buf + 1, (size_t)(rx_len - 1U));
            rx_len--;
            rx_buf[rx_len++] = byte;
        }

        /* 帧同步与滑动窗口校验 */
        while (rx_len >= MCP406_FRAME_MIN_LEN)
        {
            uint16_t frame_len = ((uint16_t)rx_buf[0] << 8) | rx_buf[1];

            /* 合法性检查：MCP-406 帧长范围通常为 5 到 128 字节 */
            if (frame_len < MCP406_FRAME_MIN_LEN || frame_len > MCP406_FRAME_MAX_LEN)
            {
                memmove(rx_buf, rx_buf + 1, (size_t)(rx_len - 1U));
                rx_len--;
                continue;
            }

            /* 帧未收齐，等待后续字节 */
            if (rx_len < frame_len)
            {
                break;
            }

            /* 校验 CRC-16 */
            uint16_t calc_crc = mcp406_calc_crc(rx_buf, (uint32_t)(frame_len - 2U));
            uint16_t recv_crc = ((uint16_t)rx_buf[frame_len - 2U] << 8) |
                                rx_buf[frame_len - 1U];

            if (calc_crc == recv_crc)
            {
                /* 完整有效帧 */
                mcp406_handle_frame(rx_buf[2], &rx_buf[3], (uint16_t)(frame_len - 5U));

                if (rx_len > frame_len)
                {
                    memmove(rx_buf, rx_buf + frame_len, (size_t)(rx_len - frame_len));
                }
                rx_len -= frame_len;
            }
            else
            {
                if (mcp406_cal_running)
                {
                    LOGI("calib rx err: len=%u calc_crc=0x%04X recv_crc=0x%04X id=0x%02X\r\n",
                         (unsigned int)frame_len, (unsigned int)calc_crc, (unsigned int)recv_crc,
                         (unsigned int)rx_buf[2]);
                }
                /* CRC 不匹配，滑动 1 字节重新搜索帧头 */
                memmove(rx_buf, rx_buf + 1, (size_t)(rx_len - 1U));
                rx_len--;
            }
        }
    }
}

/* ------------------------------ 对外接口 ------------------------------ */

void mcp406_init(void)
{
    board_compass_power(true);
    board_uart_init(MCP406_UART, MCP406_BAUD);

    /* 模块上电就绪等待（罗盘内部微处理器启动约 500ms） */
    vTaskDelay(pdMS_TO_TICKS(500U));
    board_uart_flush_rx(MCP406_UART);

    /* 0. 查询罗盘型号与固件版本（GetModInfo: 00 05 01 EF D4） */
    mcp406_send_cmd(MCP406_CMD_GET_MOD_INFO, NULL, 0U);
    /* 轮询接收响应帧 */
    for (uint8_t wait_i = 0U; wait_i < 10U; wait_i++)
    {
        vTaskDelay(pdMS_TO_TICKS(10U));
        mcp406_poll();
    }

    /* 1. 设置安装方式为 Y 轴朝上 180°（ID 10, 值 12） */
    uint8_t orient_cfg[2] = {MCP406_CFG_MOUNT_ORIENTATION, (uint8_t)MCP406_ORIENT_Y_UP_180};
    mcp406_send_cmd(MCP406_CMD_SET_CONFIG, orient_cfg, sizeof(orient_cfg));
    vTaskDelay(pdMS_TO_TICKS(100U));

    /* 2. 设置输出数据组成：方位角(5)、俯仰角(24)、横滚角(25) */
    uint8_t comp_cfg[4] = {3U, MCP406_DATA_HEADING, MCP406_DATA_PITCH, MCP406_DATA_ROLL};
    mcp406_send_cmd(MCP406_CMD_SET_DATA_COMPONENTS, comp_cfg, sizeof(comp_cfg));
    vTaskDelay(pdMS_TO_TICKS(100U));

    /* 3. 开启校准过程角度输出（标志位 ID 16, True 0x01）：
     *        校准期间罗盘仍推送姿态角，校准页面实时显示航向/俯仰 */
    uint8_t angle_in_cal[2] = {16U, 1U};
    mcp406_send_cmd(MCP406_CMD_SET_CONFIG, angle_in_cal, sizeof(angle_in_cal));
    vTaskDelay(pdMS_TO_TICKS(100U));

    /* 4. 开启校准自动采样（标志位 ID 13, True 0x01） */
    uint8_t auto_sample[2] = {MCP406_CFG_CAL_AUTO_SAMPLE, 1U};
    mcp406_send_cmd(MCP406_CMD_SET_CONFIG, auto_sample, sizeof(auto_sample));
    vTaskDelay(pdMS_TO_TICKS(100U));

    /* 5. 启动连续广播输出（固定 10Hz） */
    mcp406_send_cmd(MCP406_CMD_START_CONTINUOUS, NULL, 0U);
    vTaskDelay(pdMS_TO_TICKS(100U));

    /* 6. 保存配置到罗盘 EEPROM（保证下次上电自动广播与校准参数生效） */
    mcp406_send_cmd(MCP406_CMD_SAVE, NULL, 0U);
    vTaskDelay(pdMS_TO_TICKS(200U));

    memset(&mcp406_data, 0, sizeof(mcp406_data));
}

const mcp406_data_t* mcp406_get_data(void)
{
    return &mcp406_data;
}

bool mcp406_is_alive(uint32_t timeout_ms)
{
    if (mcp406_data.tick_angle == 0U)
    {
        return false;
    }
    return (xTaskGetTickCount() - mcp406_data.tick_angle) < pdMS_TO_TICKS(timeout_ms);
}

void mcp406_start_continuous(void)
{
    mcp406_send_cmd(MCP406_CMD_START_CONTINUOUS, NULL, 0U);
}

void mcp406_stop_continuous(void)
{
    mcp406_send_cmd(MCP406_CMD_STOP_CONTINUOUS, NULL, 0U);
}

void mcp406_set_orientation(mcp406_orient_t orient)
{
    uint8_t cfg[2];
    cfg[0] = MCP406_CFG_MOUNT_ORIENTATION;
    cfg[1] = (uint8_t)orient;
    mcp406_send_cmd(MCP406_CMD_SET_CONFIG, cfg, sizeof(cfg));
    vTaskDelay(pdMS_TO_TICKS(100U));
    mcp406_send_cmd(MCP406_CMD_SAVE, NULL, 0U);
    vTaskDelay(pdMS_TO_TICKS(100U));
}

void mcp406_calib_mag_start(void)
{
    mcp406_cal_running         = true;
    mcp406_cal_done_flag       = false;
    mcp406_data.cal_sample_cnt = 0U;
    mcp406_data.cal_mag_score  = 0.0f;
    mcp406_cal_total_pts       = MCP406_CAL_TOTAL_POINTS; /* 手动空间校准：默认 12 点 */

    /* 关键步骤 1：先停止 10Hz 连续广播输出，释放罗盘主控以进入校准模式 */
    mcp406_send_cmd(MCP406_CMD_STOP_CONTINUOUS, NULL, 0U);
    vTaskDelay(pdMS_TO_TICKS(80U));
    board_uart_flush_rx(MCP406_UART);

    /* 关键步骤 2：发送 TCM-XB 磁场空间手动校准命令：
     * 格式：00 09 0A [校准方式 Uint32 大端] [CRC16]
     * 方式 10 (0x0000000A)：磁场空间手动校准（每姿态静止后主机发采样命令）
     * 数据帧：00 09 0A 00 00 00 0A AF 06 */
    uint8_t mode[4] = {0x00U, 0x00U, 0x00U, (uint8_t)MCP406_CAL_MODE_MAG_3D};
    mcp406_send_cmd(MCP406_CMD_START_CAL, mode, sizeof(mode));

    LOGI("calib: StopCont sent -> StartCal (mode=10 space manual, total=%lu: 00 09 0A 00 00 00 0A AF 06)\r\n",
         (unsigned long)mcp406_cal_total_pts);
}

void mcp406_take_cal_sample(void)
{
    /* 手动校准采样命令（ID 31 / 0x1F） */
    mcp406_send_cmd(MCP406_CMD_TAKE_CAL_SAMPLE, NULL, 0U);
    LOGI("calib: TakeUserCalSample sent\r\n");
}

uint32_t mcp406_get_cal_samples(void)
{
    return mcp406_data.cal_sample_cnt;
}

uint32_t mcp406_get_cal_total_points(void)
{
    return mcp406_cal_total_pts;
}

bool mcp406_is_cal_done(void)
{
    return mcp406_cal_done_flag;
}

float mcp406_get_cal_mag_score(void)
{
    return mcp406_data.cal_mag_score;
}

void mcp406_save(void)
{
    mcp406_send_cmd(MCP406_CMD_SAVE, NULL, 0U);
    vTaskDelay(pdMS_TO_TICKS(100U));

    /* 恢复 10Hz 连续广播输出，回到正常测量态 */
    mcp406_send_cmd(MCP406_CMD_START_CONTINUOUS, NULL, 0U);
    vTaskDelay(pdMS_TO_TICKS(50U));

    mcp406_cal_running   = false;
    mcp406_cal_done_flag = false;
}

void mcp406_stop_cal(void)
{
    /* 发送 StopCal 命令（ID 11）：00 05 0B 4E 9E */
    mcp406_send_cmd(MCP406_CMD_STOP_CAL, NULL, 0U);
    vTaskDelay(pdMS_TO_TICKS(100U));
    mcp406_send_cmd(MCP406_CMD_START_CONTINUOUS, NULL, 0U);

    mcp406_cal_running   = false;
    mcp406_cal_done_flag = false;
    LOGI("mcp406: sent StopCal (00 05 0B 4E 9E) and resumed continuous mode\r\n");
}

void mcp406_abort_cal(void)
{
    mcp406_stop_cal();
}

void mcp406_calib_mag_end(void)
{
    /* 兼容接口：保存并结束 */
    mcp406_save();
}

void mcp406_calib_accel(void)
{
    /* TCM XB 加速度校准（校准方式=100，Uint32 大端 0x00000064） */
    uint8_t mode[4] = {0x00U, 0x00U, 0x00U, 0x64U};
    mcp406_send_cmd(MCP406_CMD_START_CAL, mode, sizeof(mode));
    vTaskDelay(pdMS_TO_TICKS(4000U));
    mcp406_send_cmd(MCP406_CMD_STOP_CAL, NULL, 0U);
    vTaskDelay(pdMS_TO_TICKS(100U));
    mcp406_send_cmd(MCP406_CMD_SAVE, NULL, 0U);
    vTaskDelay(pdMS_TO_TICKS(100U));
}

void mcp406_set_angle_ref(void)
{
    /* 零偏置零：写方位角/俯仰角/横滚角零偏均为 0.0f (3个 Float32 大端) */
    uint8_t zeros[12];
    memset(zeros, 0, sizeof(zeros));
    mcp406_send_cmd(MCP406_CMD_WRITE_ZERO, zeros, sizeof(zeros));
    vTaskDelay(pdMS_TO_TICKS(100U));
    mcp406_send_cmd(MCP406_CMD_SAVE, NULL, 0U);
    vTaskDelay(pdMS_TO_TICKS(100U));
}

void mcp406_factory_reset(void)
{
    /* 恢复磁力计出厂参数 */
    mcp406_send_cmd(MCP406_CMD_FACTORY_MAG_COEFF, NULL, 0U);
    vTaskDelay(pdMS_TO_TICKS(200U));

    /* 恢复加速度计出厂参数 */
    mcp406_send_cmd(MCP406_CMD_FACTORY_ACCEL_COEFF, NULL, 0U);
    vTaskDelay(pdMS_TO_TICKS(200U));

    mcp406_send_cmd(MCP406_CMD_SAVE, NULL, 0U);
    vTaskDelay(pdMS_TO_TICKS(500U));
    board_uart_flush_rx(MCP406_UART);

    /* 重新配置本项目参数：Y轴朝上180°、输出Heading/Pitch/Roll、10Hz广播并保存 */
    uint8_t orient_cfg[2] = {MCP406_CFG_MOUNT_ORIENTATION, (uint8_t)MCP406_ORIENT_Y_UP_180};
    mcp406_send_cmd(MCP406_CMD_SET_CONFIG, orient_cfg, sizeof(orient_cfg));
    vTaskDelay(pdMS_TO_TICKS(100U));

    uint8_t comp_cfg[4] = {3U, MCP406_DATA_HEADING, MCP406_DATA_PITCH, MCP406_DATA_ROLL};
    mcp406_send_cmd(MCP406_CMD_SET_DATA_COMPONENTS, comp_cfg, sizeof(comp_cfg));
    vTaskDelay(pdMS_TO_TICKS(100U));

    mcp406_send_cmd(MCP406_CMD_START_CONTINUOUS, NULL, 0U);
    vTaskDelay(pdMS_TO_TICKS(100U));

    mcp406_send_cmd(MCP406_CMD_SAVE, NULL, 0U);
    vTaskDelay(pdMS_TO_TICKS(200U));
}
