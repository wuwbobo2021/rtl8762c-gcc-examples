#include <peripheral/rtl876x_pinmux.h>
#include <peripheral/rtl876x_uart.h>
#undef parity
#undef idle_time
#undef stopBits
#undef wordLen
#undef rxTriggerLevel
#include <platform/rtl876x.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "rcc/rcc.h"

#ifndef _UART_H
#define _UART_H

typedef enum {
    UART0_TX_PIN = UART0_TX,
    UART0_RX_PIN = UART0_RX,
    UART0_CTS_PIN = UART0_CTS,
    UART0_RTS_PIN = UART0_RTS,
    UART1_TX_PIN = UART1_TX,
    UART1_RX_PIN = UART1_RX,
    UART1_CTS_PIN = UART1_CTS,
    UART1_RTS_PIN = UART1_RTS,
    UART2_TX_PIN = UART2_TX,
    UART2_RX_PIN = UART2_RX,
    UART2_CTS_PIN = UART2_CTS,
    UART2_RTS_PIN = UART2_RTS,
    HCI_UART_TX_PIN = HCI_UART_TX,
    HCI_UART_RX_PIN = HCI_UART_RX,
    HCI_UART_CTS_PIN = HCI_UART_CTS,
    HCI_UART_RTS_PIN = HCI_UART_RTS,
} uart_pin_function_t;

typedef struct {
    const uint8_t index;
    UART_TypeDef* register_p;
    const rcc_periph_t rcc_periph;
    const IRQn_Type irq_channel;
    const uart_pin_function_t tx_pin_function, rx_pin_function, cts_pin_function, rts_pin_function;
} uart_instance_t;

#define UART_INSTANCE(INDEX)                                 \
    ((uart_instance_t) {                                     \
        .index = INDEX,                                      \
        .register_p = (UART_TypeDef*)UART##INDEX##_REG_BASE, \
        .rcc_periph = RCC_PERIPH(UART##INDEX),               \
        .irq_channel = UART##INDEX##_IRQn,                   \
        .tx_pin_function = UART##INDEX##_TX_PIN,             \
        .rx_pin_function = UART##INDEX##_RX_PIN,             \
        .cts_pin_function = UART##INDEX##_CTS_PIN,           \
        .rts_pin_function = UART##INDEX##_RTS_PIN,           \
    })

typedef struct {
    uint8_t tx;
    uint8_t rx;
} uart_pads_t;

typedef enum {
    NO_PARTY = UART_PARITY_NO_PARTY,
    PARITY_ODD = UART_PARITY_ODD,
    PARITY_EVEN = UART_PARITY_EVEN,
} uart_parity_t;

typedef enum {
    STOP_BITS_1 = UART_STOP_BITS_1,
    STOP_BITS_2 = UART_STOP_BITS_2,
} uart_stop_bits_t;

typedef enum {
    WORD_LENGTH_7 = UART_WROD_LENGTH_7BIT,
    WORD_LENGTH_8 = UART_WROD_LENGTH_8BIT,
} uart_word_length_t;

typedef enum {
    IDLE_TIME_1 = UART_RX_IDLE_1BYTE,
    IDLE_TIME_2 = UART_RX_IDLE_2BYTE,
    IDLE_TIME_4 = UART_RX_IDLE_4BYTE,
    IDLE_TIME_8 = UART_RX_IDLE_8BYTE,
    IDLE_TIME_16 = UART_RX_IDLE_16BYTE,
    IDLE_TIME_32 = UART_RX_IDLE_32BYTE,
    IDLE_TIME_64 = UART_RX_IDLE_64BYTE,
    IDLE_TIME_128 = UART_RX_IDLE_128BYTE,
    IDLE_TIME_256 = UART_RX_IDLE_256BYTE,
    IDLE_TIME_512 = UART_RX_IDLE_512BYTE,
    IDLE_TIME_1024 = UART_RX_IDLE_1024BYTE,
    IDLE_TIME_2048 = UART_RX_IDLE_2048BYTE,
    IDLE_TIME_4096 = UART_RX_IDLE_4096BYTE,
    IDLE_TIME_8192 = UART_RX_IDLE_8192BYTE,
    IDLE_TIME_16384 = UART_RX_IDLE_16384BYTE,
    IDLE_TIME_32768 = UART_RX_IDLE_32768BYTE,
} uart_idle_time_t;

typedef struct s_uart_t {
    const uart_instance_t* instance_p;
    uart_pads_t pads;
    uint32_t baudrate; //default: 115200 set by UART_StructInit()
    uart_parity_t parity;
    uart_stop_bits_t stop_bits;
    uart_word_length_t word_length;
    uint8_t rx_trigger_level; // 1 to 29
    uart_idle_time_t idle_time;
    struct {
        const size_t size;
        size_t count;
        char* data;
    } rx_buf;
    struct {
        const size_t size;
        char* data;
    } tx_buf;
    void (*rx_cb)(struct s_uart_t* uart_p);
    void* mutex_p;
} uart_t;

bool uart_init(uart_t* uart_p);
void uart_pinmux(uart_t* uart_p);
void uart_flush(const uart_t* uart_p);
bool uart_printn(const uart_t* uart_p, const char* pSend_Buf, size_t vCount);
#define uart_print(_UART, STR) uart_printn(_UART, STR, strlen(STR))
int uart_printf(const uart_t* uart_p, char* fmt, ...);

#endif //_UART_H
