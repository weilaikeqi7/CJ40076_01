#include "app_tasks.h"

#include "app_internal.h"
#include "app_log.h"
#include "app_state.h"
#include "battery.h"
#include "measure_counter.h"
#include "module_selftest.h"
#include "power_manager.h"
#include "task.h"

#define APP_STARTUP_TASK_STACK_WORDS (configMINIMAL_STACK_SIZE * 3U)
#define APP_IO_TASK_STACK_WORDS      (configMINIMAL_STACK_SIZE * 2U)
#define APP_CONTROL_TASK_STACK_WORDS (configMINIMAL_STACK_SIZE * 3U)
#define APP_DISPLAY_TASK_STACK_WORDS (configMINIMAL_STACK_SIZE * 2U)

static BaseType_t create_runtime_tasks(void)
{
    if (xTaskCreate(AppIoTask, "io", APP_IO_TASK_STACK_WORDS, 0, tskIDLE_PRIORITY + 2U, 0) != pdPASS)
    {
        return pdFAIL;
    }

    if (xTaskCreate(AppControlTask, "control", APP_CONTROL_TASK_STACK_WORDS, 0, tskIDLE_PRIORITY + 2U, 0) != pdPASS)
    {
        return pdFAIL;
    }

    if (xTaskCreate(AppDisplayTask, "display", APP_DISPLAY_TASK_STACK_WORDS, 0, tskIDLE_PRIORITY + 1U, 0) != pdPASS)
    {
        return pdFAIL;
    }

    return pdPASS;
}

static void startup_task(void* argument)
{
    BatteryData battery;

    (void)argument;

    PowerManager_Init();
    MeasureCounter_Init();
    AppCalibrationInitFromStorage();
    AppState_SetMeasureCount(MeasureCounter_Get());

    Battery_Read(&battery);
    AppState_UpdateBattery(&battery);
    AppPowerOffIfBatteryLow("startup", &battery);

    // if (!ModuleSelfTest_RunAll())
    // {
    //     APP_LOGE("startup", "module self-test failed, power off");
    //     AppPowerOffWaitForever(false);
    // }

    if (create_runtime_tasks() != pdPASS)
    {
        APP_LOGE("startup", "failed to create runtime tasks");
        PowerManager_EnterFault();
        while (1)
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    vTaskDelete(0);
}

BaseType_t AppTasks_Start(void)
{
    AppState_Init();

    return xTaskCreate(startup_task,
                       "startup",
                       APP_STARTUP_TASK_STACK_WORDS,
                       0,
                       tskIDLE_PRIORITY + 3U,
                       0);
}
