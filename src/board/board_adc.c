/**
 * @file board_adc.c
 * @brief 电池电压采样实现（N32L40x ADC 软件触发单通道转换）
 */
#include "board_adc.h"

#include "board.h"

#include "n32l40x_adc.h"
#include "n32l40x_rcc.h"

void board_adc_init(void)
{
    GPIO_InitType gpio_init;
    ADC_InitType  adc_init;

    /* PA0 模拟输入 */
    RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOA, ENABLE);
    GPIO_InitStruct(&gpio_init);
    gpio_init.Pin            = BOARD_VBAT_ADC_PIN;
    gpio_init.GPIO_Pull      = GPIO_No_Pull;
    gpio_init.GPIO_Mode      = GPIO_Mode_Analog;
    gpio_init.GPIO_Alternate = GPIO_NO_AF;
    GPIO_InitPeripheral(BOARD_VBAT_ADC_PORT, &gpio_init);

    /* ADC 时钟：AHB 使能 + 分频 */
    RCC_EnableAHBPeriphClk(RCC_AHB_PERIPH_ADC, ENABLE);
    ADC_ConfigClk(ADC_CTRL3_CKMOD_AHB, RCC_ADCHCLK_DIV16);
    RCC_ConfigAdc1mClk(RCC_ADC1MCLK_SRC_HSI, RCC_ADC1MCLK_DIV16);

    ADC_InitStruct(&adc_init);
    adc_init.MultiChEn      = DISABLE;
    adc_init.ContinueConvEn = DISABLE;
    adc_init.ExtTrigSelect  = ADC_EXT_TRIGCONV_NONE;
    adc_init.DatAlign       = ADC_DAT_ALIGN_R;
    adc_init.ChsNumber      = 1;
    ADC_Init(ADC, &adc_init);

    /* 上电 -> 等待就绪 -> 自校准 */
    ADC_Enable(ADC, ENABLE);
    while (ADC_GetFlagStatusNew(ADC, ADC_FLAG_RDY) == RESET)
    {
    }
    ADC_StartCalibration(ADC);
    while (ADC_GetCalibrationStatus(ADC) == SET)
    {
    }
}

uint16_t board_adc_read_raw(uint8_t channel)
{
    ADC_ConfigRegularChannel(ADC, channel, 1, ADC_SAMP_TIME_55CYCLES5);
    ADC_ClearFlag(ADC, ADC_FLAG_ENDC);
    ADC_EnableSoftwareStartConv(ADC, ENABLE);
    while (ADC_GetFlagStatus(ADC, ADC_FLAG_ENDC) == RESET)
    {
    }
    return (uint16_t)ADC->DAT;
}

uint32_t board_battery_mv(void)
{
    uint32_t raw = board_adc_read_raw(BOARD_VBAT_ADC_CH);

    /* VBAT = raw * Vref / 4096 * 3 / 2 */
    return raw * BOARD_ADC_VREF_MV * BOARD_VBAT_DIVIDER_NUM / (BOARD_ADC_FULL * BOARD_VBAT_DIVIDER_DEN);
}
