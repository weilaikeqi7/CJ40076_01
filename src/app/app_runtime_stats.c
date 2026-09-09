/**
 * @file app_runtime_stats.c
 * @brief FreeRTOS 任务运行时间统计时基（TIM5，10kHz 自由运行计数器）
 *
 * FreeRTOSConfig.h 中 configGENERATE_RUN_TIME_STATS=1 要求提供：
 *   portCONFIGURE_TIMER_FOR_RUN_TIME_STATS() -> AppRunTimeStats_TimerInit()
 *   portGET_RUN_TIME_COUNTER_VALUE()         -> AppRunTimeStats_GetCounter()
 */
#include "n32l40x.h"
#include "n32l40x_rcc.h"
#include "n32l40x_tim.h"

#include <stdint.h>

void AppRunTimeStats_TimerInit(void)
{
    RCC_ClocksType       clocks;
    TIM_TimeBaseInitType tim_init = {0};
    uint32_t             tim_clk;

    RCC_EnableAPB1PeriphClk(RCC_APB1_PERIPH_TIM5, ENABLE);

    /* APB1 预分频不为 1 时定时器时钟 = PCLK1 * 2 */
    RCC_GetClocksFreqValue(&clocks);
    tim_clk = clocks.Pclk1Freq;
    if (clocks.Pclk1Freq != clocks.HclkFreq)
    {
        tim_clk *= 2U;
    }

    /* 10kHz 自由运行（统计分辨率为 tick 的 10 倍即可） */
    tim_init.Prescaler = (uint16_t)((tim_clk / 10000U) - 1U);
    tim_init.CntMode   = TIM_CNT_MODE_UP;
    tim_init.Period    = 0xFFFFU;
    tim_init.ClkDiv    = TIM_CLK_DIV1;
    tim_init.RepetCnt  = 0U;
    TIM_InitTimeBase(TIM5, &tim_init);
    TIM_Enable(TIM5, ENABLE);
}

uint32_t AppRunTimeStats_GetCounter(void)
{
    return TIM5->CNT;
}
