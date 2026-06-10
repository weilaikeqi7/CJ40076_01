#include "cs1622.h"

#include "board_config.h"
#include "bsp_gpio.h"

#include <stdbool.h>

#define CS1622_RAM_ADDRESS_MASK 0x3FU
#define CS1622_RAM_SIZE_NIBBLES 64U
#define CS1622_MODE_COMMAND     0x04U
#define CS1622_MODE_WRITE       0x05U

static void cs1622_delay(void)
{
    for (volatile uint32_t i = 0U; i < 24U; ++i)
    {
        __NOP();
    }
}

static void set_cs(bool high)
{
    BspGpio_Write(BOARD_LCD_CS_PORT, BOARD_LCD_CS_PIN, high);
}

static void set_wr(bool high)
{
    BspGpio_Write(BOARD_LCD_WR_PORT, BOARD_LCD_WR_PIN, high);
}

static void set_data(bool high)
{
    BspGpio_Write(BOARD_LCD_DATA_PORT, BOARD_LCD_DATA_PIN, high);
}

static void write_bit(bool high)
{
    set_data(high);
    cs1622_delay();
    set_wr(false);
    cs1622_delay();
    set_wr(true);
    cs1622_delay();
}

static void write_bits_msb(uint32_t value, uint8_t count)
{
    for (uint8_t i = 0U; i < count; ++i)
    {
        const uint8_t shift = (uint8_t)(count - 1U - i);
        write_bit(((value >> shift) & 0x01U) != 0U);
    }
}

static void write_bits_lsb(uint32_t value, uint8_t count)
{
    for (uint8_t i = 0U; i < count; ++i)
    {
        write_bit(((value >> i) & 0x01U) != 0U);
    }
}

void Cs1622_Init(void)
{
    set_cs(true);
    set_wr(true);
    set_data(false);
    Cs1622_WriteCmd(CS1622_CMD_SYSEN);
    Cs1622_WriteCmd(CS1622_CMD_LCDON);
    Cs1622_Clear();
}

void Cs1622_WriteCmd(uint8_t command)
{
    set_cs(false);
    write_bits_msb(CS1622_MODE_COMMAND, 3U);
    write_bits_msb(command, 8U);
    write_bit(false);
    set_cs(true);
    set_data(false);
}

void Cs1622_Clear(void)
{
    uint8_t zero[CS1622_RAM_SIZE_NIBBLES];

    for (uint32_t i = 0U; i < CS1622_RAM_SIZE_NIBBLES; ++i)
    {
        zero[i] = 0U;
    }

    Cs1622_WriteRam(0U, zero, CS1622_RAM_SIZE_NIBBLES);
}

void Cs1622_WriteRam(uint8_t address, const uint8_t* nibbles, size_t nibble_count)
{
    if ((nibbles == 0) || (nibble_count == 0U))
    {
        return;
    }

    set_cs(false);
    write_bits_msb(CS1622_MODE_WRITE, 3U);
    write_bits_msb(address & CS1622_RAM_ADDRESS_MASK, 6U);

    for (size_t i = 0U; i < nibble_count; ++i)
    {
        write_bits_lsb(nibbles[i] & 0x0FU, 4U);
    }

    set_cs(true);
    set_data(false);
}
