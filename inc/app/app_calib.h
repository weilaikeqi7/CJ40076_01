/**
 * @file app_calib.h
 * @brief 校准与补偿设置状态机
 *
 * 角度补偿设置（主控 Flash）：
 *   七击 -> PIt 页；四击 -> HEr 页
 *   页内：电源键 +0.1°，模式键 -0.1°（按住 800ms 后每 100ms 连调）
 *   双键同按 1s：PIt 页 -> 切 HIt 页；HIt/HEr 页 -> 保存 Flash 并退出
 *   补偿值显示在高程区（绝对值，0.1°），实时生效值显示在各自区域
 *
 * MCP-406 电子罗盘内部校准（写模块自身存储）：
 *   五击 -> 磁场校准开始（方式 60 空间自动采样）；六击 -> 直接发送停止校准（StopCal）
 *   采满后自动结束并显示得分，双键长按 1s -> 保存并退出
 *   八击 -> 加速度校准（水平静置约 4s，LCD 全显）
 *   九击 -> 角度参考置零（LCD 全显）
 *   十击 -> MCP-406 恢复出厂设置并重新写入本项目配置（LCD 全显）
 *   磁场校准中长按关机 = 放弃本轮（不发送结束命令）
 */
#ifndef APP_CALIB_H
#define APP_CALIB_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "app_key.h"
#include "app_store.h"

typedef enum
{
    CALIB_NONE = 0,    /* 正常运行 */
    CALIB_PIT,         /* PIt 页 */
    CALIB_HIT,         /* HIt 页 */
    CALIB_HER,         /* HEr 页 */
    CALIB_MAG,         /* 磁场自动校准采点中 */
    CALIB_MAG_DONE,    /* 磁场自动校准采满结束，显示得分等待保存 */
    CALIB_ACC_BUSY,    /* 加速度校准进行中（阻塞 4s） */
    CALIB_ANG_BUSY,    /* 角度校准进行中（阻塞 3s） */
} calib_state_t;

/** 当前状态 */
calib_state_t calib_get_state(void);

/** 当前页补偿值（0.01°），非设置页返回 0 */
int16_t calib_page_value_c01(void);

/**
 * @brief 处理按键事件（模式键多击/校准页单击连发/双键长按）。
 * @return true = 事件被校准模块消费（app 不再按正常业务处理）
 */
bool calib_handle_key(const app_key_event_t* evt);

/** 磁场校准中（采点中或已完成得分显示）：用于供电与关机判定 */
bool calib_mag_in_progress(void);

/** 磁场校准是否已采满点自动结束（等待双键长按保存） */
bool calib_mag_is_done(void);

/** 放弃当前磁场校准（关机或强行中止时调用，发送 StopCal） */
void calib_mag_abort(void);

/** 设置页是否激活（用于供电调度：GNSS 关、IMU 保） */
bool calib_page_active(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_CALIB_H */
