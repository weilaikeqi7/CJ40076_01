/**
 * @file mcp406.h
 * @brief MCP-406-TTL 高精度三维电子罗盘驱动（USART2 PA2/PA3，38400 8N1，TTL 接口）
 *
 * 依据：《MCP-406-TTL 型电子罗盘用户开发手册 - 2025.08.12》
 *
 * 协议（深海蓝科技，兼容 PNI TCM5 / TCM XB）：
 *   帧格式：[Length_H Length_L] [ID] [Data...] [CRC_H CRC_L]
 *   - Length：整帧总字节数（含长度自身与 CRC）；
 *   - CRC-16：对"数据长度 + 数据包"全部字节做 CRC 校验
 *     （算法见手册附录 CRC_Check，非反射、初值 0x0000、多项式 0x1021 等价查表式）；
 *   - 浮点数：ANSI/IEEE Std 754-1985 单精度浮点，大端序。
 *
 * 输出数据（手册 3.2 节，默认"标准 0°"安装定义）：
 *   Heading（方位角）：0.00°~359.99°，指向磁北为 0°，顺时针为正
 *   Pitch（俯仰角）  ：-90.00°~+90.00°，水平为 0°，抬头为正，低头为负
 *   Roll（横滚角）   ：-180.00°~+180.00°，水平为 0°，右倾为正，左倾为负
 *
 * 本项目安装方式：Y 轴朝上 180°（安装方式参数 = 12）。
 *
 * 校准（手册 4 节）：五击进入磁场空间手动校准（方式 10，共 12 点），
 *   发送 StartCal 后罗盘立即采集第 1 点并回报采样点编号；
 *   之后每个姿态静止后由主机发送 TakeUserCalSample 逐点采样；
 *   采满后罗盘自动结束并返回校准得分（CalScore）；
 *   六击可随时发送 StopCal 直接停止校准（手册：发送 StopCal 则认为校准失败）。
 *
 * 校准得分定义（手册第 14 条，2025.08.12 版）：
 *   <0.22 优；0.22~0.42 良；0.42~0.72 中；0.72~1.02 差；
 *   35   = 外界磁干扰较强；
 *   99.9 = 校准无效，外界磁干扰太强；
 *   200  = 未开展此校准功能；
 *   400  = 未进入校准功能，校准指令发送有误。
 */
#ifndef MCP406_H
#define MCP406_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/** 罗盘波特率（手册 2.2 节：默认 38400） */
#define MCP406_BAUD 38400U

/* ------------------------- 协议命令/标识符（手册表 4） ------------------------- */
#define MCP406_CMD_GET_MOD_INFO        1U   /* 查询罗盘型号与硬件版本号 */
#define MCP406_CMD_MOD_INFO_RESP       2U   /* 对 GetModInfo 命令的响应 */
#define MCP406_CMD_SET_DATA_COMPONENTS 3U   /* 设置输出数据组成 */
#define MCP406_CMD_GET_DATA            4U   /* 查询数据 */
#define MCP406_CMD_GET_DATA_RESP       5U   /* 对 GetData 命令的响应 */
#define MCP406_CMD_SET_CONFIG          6U   /* 设置罗盘参数 */
#define MCP406_CMD_GET_CONFIG          7U   /* 查询当前罗盘参数 */
#define MCP406_CMD_GET_CONFIG_RESP     8U   /* 对 GetConfig 命令的响应 */
#define MCP406_CMD_SAVE                9U   /* 数据保存（EEPROM） */
#define MCP406_CMD_START_CAL           10U  /* 模块开始校准 */
#define MCP406_CMD_STOP_CAL            11U  /* 模块停止校准（发送后认为校准失败） */
#define MCP406_CMD_POWER_DOWN          15U  /* 罗盘休眠 */
#define MCP406_CMD_SAVE_DONE           16U  /* 对 Save 命令的响应 */
#define MCP406_CMD_USER_CAL_SAMP_COUNT 17U  /* 校准时罗盘返回的当前已采样点数 */
#define MCP406_CMD_CAL_SCORE           18U  /* 校准得分 */
#define MCP406_CMD_SET_CONFIG_DONE     19U  /* 对 SetConfig 命令的响应 */
#define MCP406_CMD_START_CONTINUOUS    21U  /* 开始广播输出（10Hz） */
#define MCP406_CMD_STOP_CONTINUOUS     22U  /* 停止广播输出 */
#define MCP406_CMD_SET_ACQ_PARAMS      24U  /* 设置广播时间间隔 */
#define MCP406_CMD_GET_ACQ_PARAMS      25U  /* 查询广播间隔时间 */
#define MCP406_CMD_POWER_DOWN_DONE     28U  /* 休眠模式应答 */
#define MCP406_CMD_FACTORY_MAG_COEFF   29U  /* 恢复磁力计校准参数 */
#define MCP406_CMD_TAKE_CAL_SAMPLE     31U  /* 命令罗盘在用户校准时采集数据 */
#define MCP406_CMD_FACTORY_ACCEL_COEFF 36U  /* 恢复加速度计校准参数 */
#define MCP406_CMD_WRITE_ZERO          48U  /* 写方位角、俯仰角、横滚角零偏 */
#define MCP406_CMD_READ_ZERO           59U  /* 读方位角、俯仰角、横滚角零偏 */

/* ------------------------- 输出数据项 ID（手册表 5） ------------------------- */
#define MCP406_DATA_HEADING     5U   /* 方位角   Float32 度   0.00~359.99 */
#define MCP406_DATA_TEMPERATURE 7U   /* 温度     Float32 ℃    -40~85 */
#define MCP406_DATA_DISTORTION  8U   /* 磁场超范围 Boolean */
#define MCP406_DATA_CAL_STATUS  9U   /* 校准状态 Boolean */
#define MCP406_DATA_ACCEL_X     21U  /* X 轴加速度 Float32 g  ±2 */
#define MCP406_DATA_ACCEL_Y     22U  /* Y 轴加速度 Float32 g  ±2 */
#define MCP406_DATA_ACCEL_Z     23U  /* Z 轴加速度 Float32 g  ±2 */
#define MCP406_DATA_PITCH       24U  /* 俯仰角   Float32 度   ±90.00 */
#define MCP406_DATA_ROLL        25U  /* 横滚角   Float32 度   ±180.00 */
#define MCP406_DATA_MAG_X       27U  /* X 轴磁场 Float32 uT   ±800 */
#define MCP406_DATA_MAG_Y       28U  /* Y 轴磁场 Float32 uT   ±800 */
#define MCP406_DATA_MAG_Z       29U  /* Z 轴磁场 Float32 uT   ±800 */

/* ------------------------- 参数设置标志位 ID（手册表 6） ------------------------- */
#define MCP406_CFG_DECLINATION       1U   /* 磁偏角       Float32 -180°~+180°，默认 0° */
#define MCP406_CFG_TRUE_NORTH        2U   /* 真北输出     Boolean 默认 False（磁北） */
#define MCP406_CFG_BIG_ENDIAN        6U   /* 大端小端     Boolean 默认 True（大端） */
#define MCP406_CFG_MOUNT_ORIENTATION 10U  /* 安装方式     Uint8   1~24，默认 1 */
#define MCP406_CFG_CAL_SAMP_COUNT    12U  /* 用户校准采样点数 Uint32 12~32，默认 12 */
#define MCP406_CFG_CAL_AUTO_SAMPLE   13U  /* 用户校准自动采样 Boolean 默认 True */
#define MCP406_CFG_BAUD_RATE         14U  /* 波特率       Uint8   默认 12（38400） */
#define MCP406_CFG_MIL_OUTPUT        15U  /* 密位输出     Boolean 默认 False */
#define MCP406_CFG_CAL_ANGLE_OUT     16U  /* 校准过程角度输出 Boolean 默认 True */
#define MCP406_CFG_MAG_COEFF         18U  /* 磁场系数设置   Uint32 0~7 */
#define MCP406_CFG_ACCEL_COEFF       19U  /* 加速度系数设置 Uint32 0~7 */

/* ------------------------- 24 种安装方式（手册 3.2 节，配置 ID 10） ------------------------- */
typedef enum
{
    MCP406_ORIENT_STD_0    = 1,   /* 标准 0°（默认） */
    MCP406_ORIENT_X_UP_0   = 2,   /* X 轴朝上 0° */
    MCP406_ORIENT_Y_UP_0   = 3,   /* Y 轴朝上 0° */
    MCP406_ORIENT_STD_90   = 4,   /* 标准 90° */
    MCP406_ORIENT_STD_180  = 5,   /* 标准 180° */
    MCP406_ORIENT_STD_270  = 6,   /* 标准 270° */
    MCP406_ORIENT_Z_UP_0   = 7,   /* Z 轴朝上 0° */
    MCP406_ORIENT_X_UP_90  = 8,   /* X 轴朝上 90° */
    MCP406_ORIENT_X_UP_180 = 9,   /* X 轴朝上 180° */
    MCP406_ORIENT_X_UP_270 = 10,  /* X 轴朝上 270° */
    MCP406_ORIENT_Y_UP_90  = 11,  /* Y 轴朝上 90° */
    MCP406_ORIENT_Y_UP_180 = 12,  /* Y 轴朝上 180°（本项目安装方式） */
    MCP406_ORIENT_Y_UP_270 = 13,  /* Y 轴朝上 270° */
    MCP406_ORIENT_Z_UP_90  = 14,  /* Z 轴朝上 90° */
    MCP406_ORIENT_Z_UP_180 = 15,  /* Z 轴朝上 180° */
    MCP406_ORIENT_Z_UP_270 = 16,  /* Z 轴朝上 270° */
    MCP406_ORIENT_X_DN_0   = 17,  /* X 轴朝下 0° */
    MCP406_ORIENT_X_DN_90  = 18,  /* X 轴朝下 90° */
    MCP406_ORIENT_X_DN_180 = 19,  /* X 轴朝下 180° */
    MCP406_ORIENT_X_DN_270 = 20,  /* X 轴朝下 270° */
    MCP406_ORIENT_Y_DN_0   = 21,  /* Y 轴朝下 0° */
    MCP406_ORIENT_Y_DN_90  = 22,  /* Y 轴朝下 90° */
    MCP406_ORIENT_Y_DN_180 = 23,  /* Y 轴朝下 180° */
    MCP406_ORIENT_Y_DN_270 = 24,  /* Y 轴朝下 270° */
} mcp406_orient_t;

/* ------------------------- 校准方式（StartCal 参数，Uint32 大端） ------------------------- */
#define MCP406_CAL_MODE_MAG_SPACE_MANUAL  10U   /* 磁场空间手动校准 */
#define MCP406_CAL_MODE_MAG_PLANE_MANUAL  20U   /* 磁场平面手动校准 */
#define MCP406_CAL_MODE_MAG_PLANE_AUTO    50U   /* 磁场平面自动校准 */
#define MCP406_CAL_MODE_MAG_SPACE_AUTO    60U   /* 磁场空间自动校准（默认 42 点） */
#define MCP406_CAL_MODE_ACCEL             100U  /* 加速度校准 */
#define MCP406_CAL_MODE_MAG_ACCEL         110U  /* 磁场和加速度联合校准 */

/** 本项目校准总采样点数（磁场空间手动校准默认 12 点，由标志位 ID 12 设置） */
#define MCP406_CAL_TOTAL_POINTS 12U

/* ------------------------- 校准得分参考值（手册第 14 条） ------------------------- */
#define MCP406_SCORE_EXCELLENT   0.22f    /* < 0.22  优 */
#define MCP406_SCORE_GOOD        0.42f    /* 0.22~0.42 良 */
#define MCP406_SCORE_FAIR        0.72f    /* 0.42~0.72 中 */
#define MCP406_SCORE_POOR        1.02f    /* 0.72~1.02 差 */
#define MCP406_SCORE_STRONG_MAG  35.0f    /* 35   外界磁干扰较强 */
#define MCP406_SCORE_INVALID     99.9f    /* 99.9 校准无效，外界磁干扰太强 */
#define MCP406_SCORE_NOT_CAL     200.0f   /* 200  未开展此校准功能 */
#define MCP406_SCORE_CMD_ERR     400.0f   /* 400  未进入校准功能，指令有误 */

/** 罗盘最新数据包 */
typedef struct
{
    /* 姿态角，单位 度 */
    float heading; /* 方位角（0.00°~359.99°，磁北为 0°，顺时针为正） */
    float pitch;   /* 俯仰角（±90.00°，水平为 0°，抬头为正，低头为负） */
    float roll;    /* 横滚角（±180.00°，水平为 0°，右倾为正，左倾为负） */

    /* 加速度，单位 g */
    float acc_x;
    float acc_y;
    float acc_z;

    /* 磁场，单位 uT */
    float mag_x;
    float mag_y;
    float mag_z;

    float temp_c;     /* 模块内部温度，℃ */
    bool  distortion; /* 磁场超范围标志（超出线性量程） */
    bool  cal_status; /* 当前校准状态 */

    /* 校准交互相关 */
    uint32_t cal_sample_cnt;  /* 当前校准已采集点数 */
    float    cal_mag_score;   /* 磁场校准得分（含义见 MCP406_SCORE_xxx） */
    float    cal_accel_score; /* 加速度校准得分 */

    /* 各类数据最近一次更新的系统 tick（0 = 从未收到） */
    uint32_t tick_angle;
    uint32_t tick_acc;
    uint32_t tick_mag;
} mcp406_data_t;

/**
 * @brief 上电并初始化 MCP-406-TTL 电子罗盘：
 *        开电源(PA8) -> 等待启动 -> 查询型号版本(GetModInfo) ->
 *        设置安装方式（Y 轴朝上 180°，方式=12）->
 *        配置输出数据组成(Heading/Pitch/Roll) ->
 *        开启校准过程角度输出（ID 16 = True）->
 *        开启校准自动采样（ID 13 = True）->
 *        启动连续广播(10Hz) -> 保存到 EEPROM。
 * @note  必须在 FreeRTOS 任务上下文调用（内部有 vTaskDelay）。
 */
void mcp406_init(void);

/** 喂串口数据解析 MCP-406 数据帧，主循环周期调用 */
void mcp406_poll(void);

/** 取最新数据（只读指针） */
const mcp406_data_t* mcp406_get_data(void);

/** 数据是否在超时内更新过（用于判断罗盘在线状态） */
bool mcp406_is_alive(uint32_t timeout_ms);

/** 启动连续广播模式（10Hz 输出） */
void mcp406_start_continuous(void);

/** 停止连续广播模式 */
void mcp406_stop_continuous(void);

/** 设置罗盘安装方式（1~24，见 mcp406_orient_t） */
void mcp406_set_orientation(mcp406_orient_t orient);

/**
 * @brief 磁场空间手动校准开始（方式 10）。
 *        发送指令：00 09 0A 00 00 00 0A AF 06。
 *        启动前先停止 10Hz 连续广播以释放罗盘引擎。
 *        注意：发送 StartCal 后罗盘立即采集第 1 点并回报编号（手册第 11 条说明），
 *        之后每个姿态静止后调用 mcp406_take_cal_sample() 逐点采样；
 *        采满 12 点后罗盘自动结束并返回得分（CalScore，ID 18）。
 */
void mcp406_calib_mag_start(void);

/** 磁场空间手动校准：在当前姿态采集一个采样点（TakeUserCalSample：00 05 1F 1C 2B） */
void mcp406_take_cal_sample(void);

/** 获取当前磁场校准已采点数 */
uint32_t mcp406_get_cal_samples(void);

/** 获取当前校准总采样点数（默认 12） */
uint32_t mcp406_get_cal_total_points(void);

/** 磁场校准是否已自动结束（已收到校准得分 CalScore） */
bool mcp406_is_cal_done(void);

/** 获取磁场校准得分 */
float mcp406_get_cal_mag_score(void);

/**
 * @brief 保存罗盘配置及校准参数到 EEPROM（Save：00 05 09 6E DC），
 *        随后恢复 10Hz 连续广播。
 *        在校准完成确认（双键长按 1s）后调用。
 */
void mcp406_save(void);

/**
 * @brief 停止当前校准（StopCal：00 05 0B 4E 9E），随后恢复连续广播模式。
 *        注意：手册规定发送 StopCal 后认为本次校准失败。
 */
void mcp406_stop_cal(void);

/**
 * @brief 加速度计校准（方式 100）。
 *        设备水平静置，发送加速度校准指令，等待完成后退出并保存。
 */
void mcp406_calib_accel(void);

/**
 * @brief 角度参考置零（WriteZero：写方位/俯仰/横滚零偏均为 0）。
 */
void mcp406_set_angle_ref(void);

/**
 * @brief 恢复出厂设置并重新写入项目配置
 *        （38400 波特率、Y 轴朝上 180°、连续广播 10Hz、角度帧）。
 */
void mcp406_factory_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* MCP406_H */
