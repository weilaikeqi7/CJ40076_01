/**
 * @file board_uart.h
 * @brief 板载串口驱动：GNSS(USART1) / JY901B(USART2) / 测距机(UART4) / 调试(UART5)
 *        （与 03 相同 API：中断环形接收缓冲 + 阻塞发送）
 */
#ifndef BOARD_UART_H
#define BOARD_UART_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

typedef enum
{
    BOARD_UART_GNSS = 0, /* USART1 PA4/PA5，115200 */
    BOARD_UART_JY901B,   /* USART2 PA2/PA3，9600 */
    BOARD_UART_RANGER,   /* UART4  PB0/PB1，115200 */
    BOARD_UART_HOST,     /* UART5  PB4/PB5，115200（调试预留） */
    BOARD_UART_NUM
} board_uart_t;

void board_uart_init(board_uart_t port, uint32_t baud);
void board_uart_write(board_uart_t port, const void* data, size_t len);
void board_uart_putc(board_uart_t port, uint8_t byte);
size_t board_uart_read(board_uart_t port, void* buf, size_t max_len);
size_t board_uart_available(board_uart_t port);
void board_uart_flush_rx(board_uart_t port);

#ifdef __cplusplus
}
#endif

#endif /* BOARD_UART_H */
