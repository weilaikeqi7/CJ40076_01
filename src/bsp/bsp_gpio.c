#include "bsp_gpio.h"

#include "n32l40x_rcc.h"

void BspGpio_EnableClock(GPIO_Module* port)
{
    if (port == GPIOA)
    {
        RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOA, ENABLE);
    }
    else if (port == GPIOB)
    {
        RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOB, ENABLE);
    }
    else if (port == GPIOC)
    {
        RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOC, ENABLE);
    }
    else if (port == GPIOD)
    {
        RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOD, ENABLE);
    }
}

void BspGpio_InitOutput(GPIO_Module* port, uint16_t pin, bool high)
{
    GPIO_InitType init;

    BspGpio_EnableClock(port);
    GPIO_WriteBit(port, pin, high ? Bit_SET : Bit_RESET);

    GPIO_InitStruct(&init);
    init.Pin            = pin;
    init.GPIO_Current   = GPIO_DC_4mA;
    init.GPIO_Slew_Rate = GPIO_Slew_Rate_Low;
    init.GPIO_Pull      = GPIO_No_Pull;
    init.GPIO_Mode      = GPIO_Mode_Out_PP;
    init.GPIO_Alternate = GPIO_NO_AF;
    GPIO_InitPeripheral(port, &init);
}

void BspGpio_InitInput(GPIO_Module* port, uint16_t pin, GPIO_PuPdType pull)
{
    GPIO_InitType init;

    BspGpio_EnableClock(port);
    GPIO_InitStruct(&init);
    init.Pin            = pin;
    init.GPIO_Current   = GPIO_DC_2mA;
    init.GPIO_Slew_Rate = GPIO_Slew_Rate_Low;
    init.GPIO_Pull      = pull;
    init.GPIO_Mode      = GPIO_Mode_Input;
    init.GPIO_Alternate = GPIO_NO_AF;
    GPIO_InitPeripheral(port, &init);
}

void BspGpio_InitAnalog(GPIO_Module* port, uint16_t pin)
{
    GPIO_InitType init;

    BspGpio_EnableClock(port);
    GPIO_InitStruct(&init);
    init.Pin            = pin;
    init.GPIO_Current   = GPIO_DC_2mA;
    init.GPIO_Slew_Rate = GPIO_Slew_Rate_Low;
    init.GPIO_Pull      = GPIO_No_Pull;
    init.GPIO_Mode      = GPIO_Mode_Analog;
    init.GPIO_Alternate = GPIO_NO_AF;
    GPIO_InitPeripheral(port, &init);
}

void BspGpio_InitAlternate(GPIO_Module* port, uint16_t pin, uint32_t alternate)
{
    GPIO_InitType init;

    BspGpio_EnableClock(port);
    GPIO_InitStruct(&init);
    init.Pin            = pin;
    init.GPIO_Current   = GPIO_DC_4mA;
    init.GPIO_Slew_Rate = GPIO_Slew_Rate_Low;
    init.GPIO_Pull      = GPIO_Pull_Up;
    init.GPIO_Mode      = GPIO_Mode_AF_PP;
    init.GPIO_Alternate = alternate;
    GPIO_InitPeripheral(port, &init);
}

void BspGpio_InitAlternateInput(GPIO_Module* port, uint16_t pin, uint32_t alternate)
{
    GPIO_InitType init;

    BspGpio_EnableClock(port);
    GPIO_InitStruct(&init);
    init.Pin            = pin;
    init.GPIO_Current   = GPIO_DC_2mA;
    init.GPIO_Slew_Rate = GPIO_Slew_Rate_Low;
    init.GPIO_Pull      = GPIO_Pull_Up;
    init.GPIO_Mode      = GPIO_Mode_Input;
    init.GPIO_Alternate = alternate;
    GPIO_InitPeripheral(port, &init);
}

void BspGpio_Write(GPIO_Module* port, uint16_t pin, bool high)
{
    GPIO_WriteBit(port, pin, high ? Bit_SET : Bit_RESET);
}

bool BspGpio_Read(GPIO_Module* port, uint16_t pin)
{
    return GPIO_ReadInputDataBit(port, pin) != 0U;
}
