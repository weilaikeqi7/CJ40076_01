#ifndef APP_INTERNAL_H
#define APP_INTERNAL_H

#include "FreeRTOS.h"
#include "app_state.h"
#include "battery.h"
#include "keys.h"
#include "measure_counter.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    CALIBRATION_PAGE_NONE = 0,
    CALIBRATION_PAGE_PITCH_INSTALL,
    CALIBRATION_PAGE_YAW_INSTALL,
    CALIBRATION_PAGE_YAW_ERROR
} CalibrationPage;

uint32_t AppTickMs(void);
int32_t AppNormalizeYawCentidegree(int32_t yaw_cd);
int32_t AppNormalizeDegrees(int32_t degrees);
bool AppModeUsesMultifunction(AppWorkMode mode);

void AppRangePowerSet(bool enabled);
bool AppRangePowered(void);
void AppImuPowerSet(bool enabled);
bool AppImuPowered(void);
void AppGnssPowerSet(bool enabled);
bool AppGnssPowered(void);
void AppModulesOffForSleep(void);
void AppApplyModePower(AppWorkMode mode);
void AppFinishMeasurementPower(void);

void AppMeasureResetOnRangePowerOff(void);
void AppMeasureUpdateRangeFromUart(uint32_t now_ms);
void AppMeasureStart(AppWorkMode mode);
void AppMeasureCloseIfDone(uint32_t now_ms);
bool AppMeasurePending(void);
bool AppMeasureContinuousStarted(void);
bool AppMeasureRangeResultCurrent(const AppStateSnapshot* snapshot);

void AppUpdateImuFromUart(uint32_t now_ms);
void AppUpdateGnssFromUart(uint32_t now_ms);
void AppIoTask(void* argument);

void AppCalibrationInitFromStorage(void);
bool AppCalibrationSettingsActive(void);
void AppCalibrationApplyOrientationOffsets(OrientationData* data);
CalibrationPage AppCalibrationGetPage(CalibrationOffsets* offsets);
bool AppCalibrationPromptActive(void);
bool AppCalibrationMagActive(void);
bool AppCalibrationHandleExitWaitRelease(void);
void AppCalibrationEnterSettings(CalibrationPage start_page);
void AppCalibrationHandleSettingsInput(KeyEvent event, uint32_t now_ms);
void AppCalibrationAbortActive(void);
void AppCalibrationCalibrateAccelerometer(void);
void AppCalibrationCalibrateRefAngle(void);
void AppCalibrationStartMag(void);
void AppCalibrationStopMag(void);

void AppControlTask(void* argument);
void AppDisplayTask(void* argument);
void AppPowerOffWaitForever(bool save_measure_count);
void AppPowerOffIfBatteryLow(const char* module, const BatteryData* battery);

#endif /* APP_INTERNAL_H */
