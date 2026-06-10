#ifndef CS1622_H
#define CS1622_H

#include <stddef.h>
#include <stdint.h>

#define CS1622_CMD_SYSEN 0x01U
#define CS1622_CMD_LCDON 0x03U

void Cs1622_Init(void);
void Cs1622_WriteCmd(uint8_t command);
void Cs1622_Clear(void);
void Cs1622_WriteRam(uint8_t address, const uint8_t* nibbles, size_t nibble_count);

#endif /* CS1622_H */
