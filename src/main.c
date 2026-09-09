#include "main.h"

#include "app.h"
#include "board.h"
#include "rtt_log.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdint.h>

#if defined(N32_EXPECT_FPU) && (N32_EXPECT_FPU == 1) && !defined(__clang__)
#if (__FPU_USED != 1)
#error "FPU was requested, but CMSIS reports __FPU_USED != 1. Check -mfpu and -mfloat-abi."
#endif
#endif

extern uint8_t _sfreertos_heap[];
extern uint8_t _efreertos_heap[];

static void freertos_heap_region_init(void)
{
    volatile uint8_t* heap = _sfreertos_heap;

    while (heap < _efreertos_heap)
    {
        *heap++ = 0U;
    }

    __DSB();
}

int main(void)
{
    BaseType_t created;

    /* 最先初始化 GPIO 并保持电源（含电源保持脚置高） */
    board_gpio_init();
    freertos_heap_region_init();
    rtt_log_init();

    created = xTaskCreate(app_run, "APP", configMINIMAL_STACK_SIZE * 4U, NULL,
                          tskIDLE_PRIORITY + 1U, NULL);
    if (created != pdPASS)
    {
        LOGI("main: failed to create app task\r\n");
        Error_Handler();
    }

    vTaskStartScheduler();
    LOGI("main: scheduler returned\r\n");
    Error_Handler();
}

void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}

void AppAssertFailed(const char* file, int line)
{
    LOGI("rtos: assert failed %s:%d\r\n", file, (int)line);
    Error_Handler();
}

void vApplicationMallocFailedHook(void)
{
    LOGI("rtos: malloc failed\r\n");
    Error_Handler();
}

void vApplicationIdleHook(void)
{
    /* 低功耗策略后续可在此进入 sleep；当前空转 */
}

void vApplicationStackOverflowHook(TaskHandle_t task, char* task_name)
{
    (void)task;
    LOGI("rtos: stack overflow: %s\r\n", (task_name != NULL) ? task_name : "-");
    Error_Handler();
}

#ifdef USE_FULL_ASSERT
void assert_failed(const uint8_t* expr, const uint8_t* file, uint32_t line)
{
    (void)expr;
    (void)file;
    (void)line;
    Error_Handler();
}
#endif
