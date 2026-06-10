#ifndef BSP_GPIO_H
#define BSP_GPIO_H

#include "n32l40x.h"

#include <stdbool.h>
#include <stdint.h>

void BspGpio_EnableClock(GPIO_Module* port);
void BspGpio_InitOutput(GPIO_Module* port, uint16_t pin, bool high);
void BspGpio_InitInput(GPIO_Module* port, uint16_t pin, GPIO_PuPdType pull);
void BspGpio_InitAnalog(GPIO_Module* port, uint16_t pin);
void BspGpio_InitAlternate(GPIO_Module* port, uint16_t pin, uint32_t alternate);
void BspGpio_InitAlternateInput(GPIO_Module* port, uint16_t pin, uint32_t alternate);
void BspGpio_Write(GPIO_Module* port, uint16_t pin, bool high);
bool BspGpio_Read(GPIO_Module* port, uint16_t pin);

#endif /* BSP_GPIO_H */
