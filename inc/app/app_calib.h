/**
 * @file app_calib.h
 * @brief 校准与补偿设置状态机（与 03 业务一致）
 *
 * 角度补偿设置（主控 Flash）：七击 -> PIt 页；四击 -> HEr 页；
 *   页内电源键 +0.1°、模式键 -0.1°（800ms 后 100ms 连发）；
 *   双键同按 1s：PIt -> HIt 切页；HIt/HEr -> 保存 Flash 退出。
 *   补偿值显示在高程区（绝对值，0.1° 单位），实时生效值回各自区域。
 *
 * JY901B 内部校准：五击开始磁场校准（LCD 全显）；六击结束并保存；
 *   八击加速度校准（约 4s 全显）；九击角度参考（约 3s 全显）；
 *   磁场校准中长按关机 = 放弃本轮。
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
    CALIB_NONE = 0,
    CALIB_PIT,
    CALIB_HIT,
    CALIB_HER,
    CALIB_MAG,
    CALIB_ACC_BUSY,
    CALIB_ANG_BUSY,
} calib_state_t;

calib_state_t calib_get_state(void);
int16_t       calib_page_value_c01(void);
bool          calib_handle_key(const app_key_event_t* evt);
bool          calib_mag_in_progress(void);
bool          calib_page_active(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_CALIB_H */
