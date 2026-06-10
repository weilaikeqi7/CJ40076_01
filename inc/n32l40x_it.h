#ifndef N32L40X_IT_H
#define N32L40X_IT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "n32l40x.h"

void NMI_Handler(void);
void HardFault_Handler(void);
void MemManage_Handler(void);
void BusFault_Handler(void);
void UsageFault_Handler(void);
void DebugMon_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* N32L40X_IT_H */
