/**
 * @file board.c
 * @brief CJ40076 V2.0 板级 GPIO 驱动实现（与 03 相同 API 结构）
 */
#include "board.h"

#include "misc.h"
#include "n32l40x_rcc.h"

static void enable_gpio_clock(GPIO_Module* gpio)
{
    if (gpio == GPIOA)
    {
        RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOA, ENABLE);
    }
    else if (gpio == GPIOB)
    {
        RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOB, ENABLE);
    }
    else if (gpio == GPIOC)
    {
        RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOC, ENABLE);
    }
    else if (gpio == GPIOD)
    {
        RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOD, ENABLE);
    }
}

static void init_output(GPIO_Module* gpio, uint16_t pin, bool on)
{
    GPIO_InitType gpio_init;

    enable_gpio_clock(gpio);
    GPIO_WriteBit(gpio, pin, on ? Bit_SET : Bit_RESET);

    GPIO_InitStruct(&gpio_init);
    gpio_init.Pin            = pin;
    gpio_init.GPIO_Current   = GPIO_DC_4mA;
    gpio_init.GPIO_Slew_Rate = GPIO_Slew_Rate_Low;
    gpio_init.GPIO_Pull      = GPIO_No_Pull;
    gpio_init.GPIO_Mode      = GPIO_Mode_Out_PP;
    gpio_init.GPIO_Alternate = GPIO_NO_AF;
    GPIO_InitPeripheral(gpio, &gpio_init);
}

static void init_input_pullup(GPIO_Module* gpio, uint16_t pin)
{
    GPIO_InitType gpio_init;

    enable_gpio_clock(gpio);
    GPIO_InitStruct(&gpio_init);
    gpio_init.Pin            = pin;
    gpio_init.GPIO_Pull      = GPIO_Pull_Up;
    gpio_init.GPIO_Mode      = GPIO_Mode_Input;
    gpio_init.GPIO_Alternate = GPIO_NO_AF;
    GPIO_InitPeripheral(gpio, &gpio_init);
}

static void write_pin(GPIO_Module* gpio, uint16_t pin, bool on)
{
    GPIO_WriteBit(gpio, pin, on ? Bit_SET : Bit_RESET);
}

void board_power_hold(bool on)
{
    init_output(BOARD_PWR_HOLD_PORT, BOARD_PWR_HOLD_PIN, on);
}

void board_gpio_init(void)
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);

    /* 最先保持电源，防止松开电源键后掉电 */
    board_power_hold(true);

    /* 各外设电源默认关闭 */
    init_output(BOARD_PWR_RANGER_PORT, BOARD_PWR_RANGER_PIN, false);
    init_output(BOARD_PWR_JY901B_PORT, BOARD_PWR_JY901B_PIN, false);
    init_output(BOARD_PWR_GNSS_PORT, BOARD_PWR_GNSS_PIN, false);

    /* 按键：低有效，上拉输入 */
    init_input_pullup(BOARD_KEY_MODE_PORT, BOARD_KEY_MODE_PIN);
    init_input_pullup(BOARD_KEY_POWER_PORT, BOARD_KEY_POWER_PIN);

    /* LCD 驱动芯片接口（CS/RD/WR 空闲高，DATA 低） */
    init_output(BOARD_LCD_CS_PORT, BOARD_LCD_CS_PIN, true);
    init_output(BOARD_LCD_RD_PORT, BOARD_LCD_RD_PIN, true);
    init_output(BOARD_LCD_WR_PORT, BOARD_LCD_WR_PIN, true);
    init_output(BOARD_LCD_DATA_PORT, BOARD_LCD_DATA_PIN, false);
    init_input_pullup(BOARD_LCD_IRQ_PORT, BOARD_LCD_IRQ_PIN);
}

void board_ranger_power(bool on)
{
    write_pin(BOARD_PWR_RANGER_PORT, BOARD_PWR_RANGER_PIN, on);
}

void board_jy901b_power(bool on)
{
    write_pin(BOARD_PWR_JY901B_PORT, BOARD_PWR_JY901B_PIN, on);
}

void board_gnss_power(bool on)
{
    write_pin(BOARD_PWR_GNSS_PORT, BOARD_PWR_GNSS_PIN, on);
}

bool board_key_mode_pressed(void)
{
    return GPIO_ReadInputDataBit(BOARD_KEY_MODE_PORT, BOARD_KEY_MODE_PIN) == 0U;
}

bool board_key_power_pressed(void)
{
    return GPIO_ReadInputDataBit(BOARD_KEY_POWER_PORT, BOARD_KEY_POWER_PIN) == 0U;
}
