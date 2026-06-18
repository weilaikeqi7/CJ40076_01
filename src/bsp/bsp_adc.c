#include "bsp_adc.h"

#include "n32l40x_adc.h"
#include "n32l40x_dma.h"
#include "n32l40x_rcc.h"

#define BSP_ADC_TIMEOUT_LOOPS 100000UL
#define BSP_ADC_OVERSAMPLE    8U
#define BSP_ADC_DMA_CH        DMA_CH5
#define BSP_ADC_DMA_TC_FLAG   DMA_FLAG_TC5
#define BSP_ADC_DMA_GL_FLAG   DMA_FLAG_GL5

static volatile uint16_t g_adc_dma_value;

static void adc_start_channel(uint8_t channel)
{
    DMA_EnableChannel(BSP_ADC_DMA_CH, DISABLE);
    ADC_EnableDMA(ADC, DISABLE);
    ADC_EnableSoftwareStartConv(ADC, DISABLE);

    DMA_SetCurrDataCounter(BSP_ADC_DMA_CH, 1U);
    DMA_ClearFlag(BSP_ADC_DMA_GL_FLAG, DMA);
    ADC_ClearFlag(ADC, ADC_FLAG_ENDC);
    ADC_ConfigRegularChannel(ADC, channel, 1U, ADC_SAMP_TIME_239CYCLES5);

    ADC_EnableDMA(ADC, ENABLE);
    DMA_EnableChannel(BSP_ADC_DMA_CH, ENABLE);
    ADC_EnableSoftwareStartConv(ADC, ENABLE);
}

void BspAdc_Init(void)
{
    ADC_InitType init;
    DMA_InitType dma_init;
    uint32_t timeout = BSP_ADC_TIMEOUT_LOOPS;

    RCC_EnableAHBPeriphClk(RCC_AHB_PERIPH_DMA, ENABLE);
    RCC_EnableAHBPeriphClk(RCC_AHB_PERIPH_ADC, ENABLE);
    ADC_ConfigClk(ADC_CTRL3_CKMOD_AHB, RCC_ADCHCLK_DIV16);
    RCC_ConfigAdc1mClk(RCC_ADC1MCLK_SRC_HSI, RCC_ADC1MCLK_DIV16);

    DMA_DeInit(BSP_ADC_DMA_CH);
    DMA_StructInit(&dma_init);
    dma_init.PeriphAddr     = (uint32_t)&ADC->DAT;
    dma_init.MemAddr        = (uint32_t)&g_adc_dma_value;
    dma_init.Direction      = DMA_DIR_PERIPH_SRC;
    dma_init.BufSize        = 1U;
    dma_init.PeriphInc      = DMA_PERIPH_INC_DISABLE;
    dma_init.DMA_MemoryInc  = DMA_MEM_INC_DISABLE;
    dma_init.PeriphDataSize = DMA_PERIPH_DATA_SIZE_HALFWORD;
    dma_init.MemDataSize    = DMA_MemoryDataSize_HalfWord;
    dma_init.CircularMode   = DMA_MODE_NORMAL;
    dma_init.Priority       = DMA_PRIORITY_HIGH;
    dma_init.Mem2Mem        = DMA_M2M_DISABLE;
    DMA_Init(BSP_ADC_DMA_CH, &dma_init);
    DMA_RequestRemap(DMA_REMAP_ADC1, DMA, BSP_ADC_DMA_CH, ENABLE);

    ADC_InitStruct(&init);
    init.MultiChEn      = ENABLE;
    init.ContinueConvEn = DISABLE;
    init.ExtTrigSelect  = ADC_EXT_TRIGCONV_NONE;
    init.DatAlign       = ADC_DAT_ALIGN_R;
    init.ChsNumber      = 1U;
    ADC_Init(ADC, &init);
    ADC_Enable(ADC, ENABLE);

    timeout = BSP_ADC_TIMEOUT_LOOPS;
    while ((ADC_GetFlagStatusNew(ADC, ADC_FLAG_RDY) == RESET) && (timeout > 0U))
    {
        --timeout;
    }

    timeout = BSP_ADC_TIMEOUT_LOOPS;
    ADC_StartCalibration(ADC);
    while ((ADC_GetCalibrationStatus(ADC) == SET) && (timeout > 0U))
    {
        --timeout;
    }

    g_adc_dma_value = 0U;
}

static bool adc_sample_once(uint8_t channel, uint16_t* raw)
{
    uint32_t timeout = BSP_ADC_TIMEOUT_LOOPS;

    adc_start_channel(channel);

    while ((DMA_GetFlagStatus(BSP_ADC_DMA_TC_FLAG, DMA) == RESET) && (timeout > 0U))
    {
        --timeout;
    }

    if (timeout == 0U)
    {
        return false;
    }

    *raw = g_adc_dma_value;
    return true;
}

bool BspAdc_ReadRaw(uint8_t channel, uint16_t* raw)
{
    uint32_t sum = 0U;
    uint16_t min = 0xFFFFU;
    uint16_t max = 0U;
    uint8_t  i;

    if (raw == 0)
    {
        return false;
    }

    for (i = 0U; i < BSP_ADC_OVERSAMPLE; ++i)
    {
        uint16_t sample = 0U;

        if (!adc_sample_once(channel, &sample))
        {
            return false;
        }

        sum += sample;
        if (sample < min)
        {
            min = sample;
        }
        if (sample > max)
        {
            max = sample;
        }
    }

    sum -= min;
    sum -= max;
    *raw = (uint16_t)(sum / (BSP_ADC_OVERSAMPLE - 2U));
    return true;
}
