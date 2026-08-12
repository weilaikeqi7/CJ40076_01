#include "measure_counter.h"

#include "n32l40x_flash.h"

#include <stdbool.h>

#define MEASURE_COUNTER_FLASH_ADDR  (0x08000000UL + (128UL * 1024UL) - 0x800UL)
#define MEASURE_COUNTER_MAGIC       0x434A0076UL

typedef struct
{
    uint32_t count;
    uint32_t magic;
} MeasureCounterRecord;

static MeasureCounterRecord g_record;
static bool g_dirty;

static bool save_record(void)
{
    bool saved = false;

    FLASH_Unlock();
    if (FLASH_COMPL == FLASH_EraseOnePage(MEASURE_COUNTER_FLASH_ADDR))
    {
        saved = (FLASH_COMPL == FLASH_ProgramWord(MEASURE_COUNTER_FLASH_ADDR, g_record.count)) &&
                (FLASH_COMPL == FLASH_ProgramWord(MEASURE_COUNTER_FLASH_ADDR + 4UL, g_record.magic));
    }
    FLASH_Lock();

    return saved;
}

void MeasureCounter_Init(void)
{
    const MeasureCounterRecord* stored = (const MeasureCounterRecord*)MEASURE_COUNTER_FLASH_ADDR;

    g_record = *stored;
    if (g_record.magic != MEASURE_COUNTER_MAGIC)
    {
        g_record.count = 0U;
        g_record.magic = MEASURE_COUNTER_MAGIC;
        g_dirty = true;
    }
    else
    {
        g_dirty = false;
    }
}

uint32_t MeasureCounter_Get(void)
{
    return g_record.count;
}

uint32_t MeasureCounter_Increment(void)
{
    if (g_record.count < 99999U)
    {
        ++g_record.count;
        g_dirty = true;
    }
    return g_record.count;
}

void MeasureCounter_Reset(void)
{
    g_record.count = 0U;
    g_record.magic = MEASURE_COUNTER_MAGIC;
    g_dirty = true;
}

bool MeasureCounter_Save(void)
{
    if (!g_dirty)
    {
        return true;
    }

    if (!save_record())
    {
        return false;
    }

    g_dirty = false;
    return true;
}
