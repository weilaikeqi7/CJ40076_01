/**
 * @file app_store.h
 * @brief Flash 参数区：测量计数 + 角度补偿值（PIt/HIt/HEr）
 *
 * 使用内部 Flash 最后一页（N32L403KB 128K，末页 0x0801F800，2KB），
 * magic + CRC16 校验，损坏回退默认值。
 *
 * 写入时机（与 03 一致）：
 *   - 三击计数清零：立即写
 *   - 正常长按关机 / 欠压关机：写计数
 *   - 补偿设置页双键长按：写补偿值
 */
#ifndef APP_STORE_H
#define APP_STORE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    int16_t pit_c01;
    int16_t hit_c01;
    int16_t her_c01;
} app_offsets_t;

void store_init(void);

uint32_t store_get_count(void);
void     store_set_count_ram(uint32_t count);
bool     store_save_count(void);

void store_get_offsets(app_offsets_t* out);
bool store_save_offsets(const app_offsets_t* offsets);

#ifdef __cplusplus
}
#endif

#endif /* APP_STORE_H */
