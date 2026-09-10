/**
 * @file board_uart.c
 * @brief 板载串口驱动实现（与 03 相同结构：RXDNE 中断环形缓冲，TX 轮询阻塞）
 *
 * N32L40x 复用功能（board_config 原理图值）：
 *   USART1 TX=AF1 RX=AF4；USART2 TX/RX=AF4；UART4 TX/RX=AF6
 */
#include "board_uart.h"

#include "board.h"
#include "misc.h"
#include "n32l40x_rcc.h"
#include "n32l40x_usart.h"
#include "rtt_log.h"

#define UART_RX_BUF_SIZE 256U

typedef struct
{
    USART_Module* usart;
    GPIO_Module*  tx_port;
    uint16_t      tx_pin;
    uint32_t      tx_af;
    GPIO_Module*  rx_port;
    uint16_t      rx_pin;
    uint32_t      rx_af;
    uint8_t       irqn;
    volatile uint16_t rx_head; /* ISR 写 */
    volatile uint16_t rx_tail; /* 任务读 */
    uint8_t           rx_buf[UART_RX_BUF_SIZE];
} uart_dev_t;

static uart_dev_t uart_devs[BOARD_UART_NUM] = {
    [BOARD_UART_GNSS] =
        {
            .usart   = USART1,
            .tx_port = GPIOA,
            .tx_pin  = GPIO_PIN_4,
            .tx_af   = GPIO_AF1_USART1,
            .rx_port = GPIOA,
            .rx_pin  = GPIO_PIN_5,
            .rx_af   = GPIO_AF4_USART1,
            .irqn    = USART1_IRQn,
        },
    [BOARD_UART_JY901B] =
        {
            .usart   = USART2,
            .tx_port = GPIOA,
            .tx_pin  = GPIO_PIN_2,
            .tx_af   = GPIO_AF4_USART2,
            .rx_port = GPIOA,
            .rx_pin  = GPIO_PIN_3,
            .rx_af   = GPIO_AF4_USART2,
            .irqn    = USART2_IRQn,
        },
    [BOARD_UART_RANGER] =
        {
            .usart   = UART4,
            .tx_port = GPIOB,
            .tx_pin  = GPIO_PIN_0,
            .tx_af   = GPIO_AF6_UART4,
            .rx_port = GPIOB,
            .rx_pin  = GPIO_PIN_1,
            .rx_af   = GPIO_AF6_UART4,
            .irqn    = UART4_IRQn,
        },
};

static void usart_rcc_enable(board_uart_t port)
{
    switch (port)
    {
    case BOARD_UART_GNSS:
        RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOA | RCC_APB2_PERIPH_USART1, ENABLE);
        break;
    case BOARD_UART_JY901B:
        RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOA, ENABLE);
        RCC_EnableAPB1PeriphClk(RCC_APB1_PERIPH_USART2, ENABLE);
        break;
    case BOARD_UART_RANGER:
        RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOB | RCC_APB2_PERIPH_UART4, ENABLE);
        break;
    default:
        break;
    }
}

static void uart_gpio_init(GPIO_Module* port, uint16_t pin, GPIO_ModeType mode, uint32_t af)
{
    GPIO_InitType gpio_init;

    GPIO_InitStruct(&gpio_init);
    gpio_init.Pin            = pin;
    gpio_init.GPIO_Pull      = GPIO_Pull_Up;
    gpio_init.GPIO_Mode      = mode;
    gpio_init.GPIO_Alternate = af;
    GPIO_InitPeripheral(port, &gpio_init);
}

void board_uart_init(board_uart_t port, uint32_t baud)
{
    uart_dev_t*    dev = &uart_devs[port];
    USART_InitType usart_init;
    NVIC_InitType  nvic_init;

    usart_rcc_enable(port);

    /* TX：复用推挽；RX：上拉输入（空闲高电平） */
    uart_gpio_init(dev->tx_port, dev->tx_pin, GPIO_Mode_AF_PP, dev->tx_af);
    uart_gpio_init(dev->rx_port, dev->rx_pin, GPIO_Mode_Input, dev->rx_af);

    USART_StructInit(&usart_init);
    usart_init.BaudRate            = baud;
    usart_init.WordLength          = USART_WL_8B;
    usart_init.StopBits            = USART_STPB_1;
    usart_init.Parity              = USART_PE_NO;
    usart_init.Mode                = USART_MODE_RX | USART_MODE_TX;
    usart_init.HardwareFlowControl = USART_HFCTRL_NONE;
    USART_Init(dev->usart, &usart_init);

    dev->rx_head = 0;
    dev->rx_tail = 0;

    /* ISR 内不调用任何 FreeRTOS API，优先级可高于 configMAX_SYSCALL(5) */
    nvic_init.NVIC_IRQChannel                   = dev->irqn;
    nvic_init.NVIC_IRQChannelPreemptionPriority = 4;
    nvic_init.NVIC_IRQChannelSubPriority        = 0;
    nvic_init.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&nvic_init);

    USART_ConfigInt(dev->usart, USART_INT_RXDNE, ENABLE);
    USART_Enable(dev->usart, ENABLE);
}

void board_uart_putc(board_uart_t port, uint8_t byte)
{
    USART_Module* usart = uart_devs[port].usart;

    while (USART_GetFlagStatus(usart, USART_FLAG_TXDE) == RESET)
    {
    }
    USART_SendData(usart, byte);
    if (port == BOARD_UART_RANGER)
    {
        LOGI("send: port=%d byte=0x%02X\r\n", (int)port, (int)byte);
    }
}

void board_uart_write(board_uart_t port, const void* data, size_t len)
{
    const uint8_t* p = (const uint8_t*)data;

    while (len-- > 0U)
    {
        board_uart_putc(port, *p++);
    }
}

size_t board_uart_read(board_uart_t port, void* buf, size_t max_len)
{
    uart_dev_t* dev = &uart_devs[port];
    uint8_t*    out = (uint8_t*)buf;
    size_t      n   = 0;

    while (n < max_len && dev->rx_tail != dev->rx_head)
    {
        out[n++]     = dev->rx_buf[dev->rx_tail];
        dev->rx_tail = (uint16_t)((dev->rx_tail + 1U) % UART_RX_BUF_SIZE);
    }
    return n;
}

size_t board_uart_available(board_uart_t port)
{
    uart_dev_t* dev = &uart_devs[port];

    return (uint16_t)(dev->rx_head - dev->rx_tail) % UART_RX_BUF_SIZE;
}

void board_uart_flush_rx(board_uart_t port)
{
    uart_devs[port].rx_tail = uart_devs[port].rx_head;
}

static void uart_rx_isr(board_uart_t port)
{
    uart_dev_t* dev  = &uart_devs[port];
    uint16_t    next = (uint16_t)((dev->rx_head + 1U) % UART_RX_BUF_SIZE);
    uint8_t     byte = (uint8_t)USART_ReceiveData(dev->usart);
    if (port == BOARD_UART_RANGER)
    {
        LOGI("recv: port=%d byte=0x%02X\r\n", (int)port, (int)byte);
    }
    if (next != dev->rx_tail)
    {
        dev->rx_buf[dev->rx_head] = byte;
        dev->rx_head              = next;
    }
    /* 缓冲满时丢弃新字节 */
}

void USART1_IRQHandler(void)
{
    if (USART_GetIntStatus(USART1, USART_INT_RXDNE) != RESET)
    {
        uart_rx_isr(BOARD_UART_GNSS);
    }
}

void USART2_IRQHandler(void)
{
    if (USART_GetIntStatus(USART2, USART_INT_RXDNE) != RESET)
    {
        uart_rx_isr(BOARD_UART_JY901B);
    }
}

void UART4_IRQHandler(void)
{
    if (USART_GetIntStatus(UART4, USART_INT_RXDNE) != RESET)
    {
        uart_rx_isr(BOARD_UART_RANGER);
    }
}
