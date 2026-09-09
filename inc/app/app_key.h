/**
 * @file app_key.h
 * @brief 按键驱动：消抖、短按、长按、模式键多击（1~9）、双键组合（与 03 业务一致）
 *
 * 事件模型（app_key_scan 每 10ms 调用一次）：
 *   电源键短按        -> APP_KEY_EVT_POWER_SHORT（开机后首次按压被抑制）
 *   电源键长按 3s     -> APP_KEY_EVT_POWER_LONG（到点立即触发，不等松手）
 *   模式键 N 击       -> APP_KEY_EVT_MODE_CLICKS，arg = 1~9（末次点击 600ms 后触发）
 *   双键同按 1s       -> APP_KEY_EVT_BOTH_LONG（校准页切页/保存）
 *   校准页单击         -> APP_KEY_EVT_POWER_SHORT / APP_KEY_EVT_MODE_SINGLE
 *   校准页按住连调     -> APP_KEY_EVT_POWER_REPEAT / APP_KEY_EVT_MODE_REPEAT（800ms 后每 100ms）
 */
#ifndef APP_KEY_H
#define APP_KEY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    APP_KEY_EVT_NONE         = 0,
    APP_KEY_EVT_POWER_SHORT  = 0x0001,
    APP_KEY_EVT_POWER_LONG   = 0x0002,
    APP_KEY_EVT_MODE_CLICKS  = 0x0004,
    APP_KEY_EVT_MODE_SINGLE  = 0x0008,
    APP_KEY_EVT_BOTH_LONG    = 0x0010,
    APP_KEY_EVT_POWER_REPEAT = 0x0020,
    APP_KEY_EVT_MODE_REPEAT  = 0x0040,
} app_key_evt_t;

typedef struct
{
    uint16_t evt;
    uint8_t  arg;
} app_key_event_t;

void app_key_init(void);
app_key_event_t app_key_scan(void);
void app_key_set_calib_mode(bool on);
bool app_key_power_down(void);
bool app_key_mode_down(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_KEY_H */
