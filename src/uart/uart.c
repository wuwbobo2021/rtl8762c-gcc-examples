/**
 * @file UART.c
 * @ingroup UARTGroup
 * @author marble
 * @date 2021-05-31
 *
 */

// modified by wuwbobo2021 (2023-9-29)

#include <os_msg.h>
#include <os_sync.h>
#include <rtl876x_nvic.h>
#include <rtl876x_pinmux.h>

#include "rcc/rcc.h"
#include "rtlbaud.h"
#include "uart/uart.h"
#include "uart/rtlbaud.h"

#define NUM_UARTS (3)

static uart_t* uarts[NUM_UARTS];

bool uart_init(uart_t* uart_p)
{
    uarts[uart_p->instance_p->index] = uart_p;

    if (true != os_mutex_create(&(uart_p->mutex_p))) {
        // the mutex was not created successfully
        return false;
    }

    uart_pinmux(uart_p);

    // initialize UART
    rcc_periph_set(&(uart_p->instance_p->rcc_periph), ENABLE);

    /* uart init */
    UART_InitTypeDef UART_InitStruct;
    UART_StructInit(&UART_InitStruct);

    Rtl_Uart_BaudRate_Config baud_conf;
    baud_conf = rtl_baud_auto_calc(uart_p->baudrate);
    if (baud_conf.is_valid) {
        UART_InitStruct.div = baud_conf.div;
        UART_InitStruct.ovsr = baud_conf.ovsr;
        UART_InitStruct.ovsr_adj = baud_conf.ovsr_adj;
    }

    UART_InitStruct.parity = uart_p->parity;
    UART_InitStruct.stopBits = uart_p->stop_bits;
    UART_InitStruct.wordLen = uart_p->word_length;
    UART_InitStruct.rxTriggerLevel = uart_p->rx_trigger_level;
    UART_InitStruct.idle_time = uart_p->idle_time;

    UART_Init(uart_p->instance_p->register_p, &UART_InitStruct);

    if ((0 <= uart_p->pads.rx) && (uart_p->pads.rx < TOTAL_PIN_NUM)) {
        // enable rx interrupt and line status interrupt
        UART_INTConfig(UART, UART_INT_RD_AVA, ENABLE);
        UART_INTConfig(UART, UART_INT_IDLE, ENABLE);

        /*  Enable UART IRQ  */
        NVIC_InitTypeDef NVIC_InitStruct;
        NVIC_InitStruct.NVIC_IRQChannel = uart_p->instance_p->irq_channel;
        NVIC_InitStruct.NVIC_IRQChannelCmd = (FunctionalState)ENABLE;
        NVIC_InitStruct.NVIC_IRQChannelPriority = 3;
        NVIC_Init(&NVIC_InitStruct);
    }

    return true;
}

void uart_pinmux(uart_t* uart_p)
{
    if ((0 <= uart_p->pads.tx) && (uart_p->pads.tx < TOTAL_PIN_NUM)) {
        Pad_Config(uart_p->pads.tx, PAD_PINMUX_MODE, PAD_IS_PWRON, PAD_PULL_UP,
            PAD_OUT_DISABLE, PAD_OUT_HIGH);
        Pinmux_Config(uart_p->pads.tx, uart_p->instance_p->tx_pin_function);
    }

    if ((0 <= uart_p->pads.rx) && (uart_p->pads.rx < TOTAL_PIN_NUM)) {
        Pad_Config(uart_p->pads.rx, PAD_PINMUX_MODE, PAD_IS_PWRON, PAD_PULL_UP,
            PAD_OUT_DISABLE, PAD_OUT_HIGH);
        Pinmux_Config(uart_p->pads.rx, uart_p->instance_p->rx_pin_function);
    }
}

void uart_flush(const uart_t* uart_p)
{
    while (UART_GetFlagState(uart_p->instance_p->register_p,
               UART_FLAG_THR_TSR_EMPTY)
        != SET)
        ;
}

bool uart_printn(const uart_t* uart_p, const char* str, size_t vCount)
{
    size_t i = 0;
    if (!os_mutex_take(uart_p->mutex_p, 0xffffffff)) {
        return false;
    }

    uart_flush(uart_p);

    /* send block bytes(16 bytes) */
    for (i = 0; i < (vCount / UART_TX_FIFO_SIZE); i++) {
        UART_SendData(uart_p->instance_p->register_p, (const uint8_t*)str + (UART_TX_FIFO_SIZE * i), UART_TX_FIFO_SIZE);
        /* wait tx fifo empty */
        uart_flush(uart_p);
    }

    /* send left bytes */
    UART_SendData(uart_p->instance_p->register_p, (const uint8_t*)str + (UART_TX_FIFO_SIZE * i), vCount % UART_TX_FIFO_SIZE);

    os_mutex_give(uart_p->mutex_p);

    return true;
}

int uart_printf(const uart_t* uart_p, char* fmt, ...)
{
    int len;
    va_list argptr;

    va_start(argptr, fmt);

    len = vsnprintf(uart_p->tx_buf.data, uart_p->tx_buf.size, fmt, argptr);
    if (len > 0) {
        if (!uart_printn(uart_p, uart_p->tx_buf.data, len)) {
            return -1;
        }
    }

    va_end(argptr);

    return len;
}

static void _genericUARTHandler(int index)
{
    uart_t* uart_p = uarts[index];
    if (NULL == uart_p) {
        return;
    }

    UART_TypeDef* Register_p = uart_p->instance_p->register_p;
    uint16_t rx_len = 0;
    /* diable interrups globally to prevent cascades */
    __disable_irq();
    /* Read interrupt id */
    uint32_t int_status = UART_GetIID(Register_p);
    /* Disable interrupt */
    UART_INTConfig(Register_p, UART_INT_RD_AVA | UART_INT_LINE_STS, DISABLE);

    if (UART_GetFlagState(Register_p, UART_FLAG_RX_IDLE) == SET) {
        /* Clear flag */
        UART_INTConfig(Register_p, UART_INT_IDLE, DISABLE);
        /* Send msg to app task */
        if (NULL != uart_p->rx_cb) {
            uart_p->rx_cb(uart_p);
        }
        /* IO_UART_DLPS_Enter_Allowed = true; */
        UART_INTConfig(Register_p, UART_INT_IDLE, ENABLE);
    }

    switch (int_status & 0x0E) {
    /* Rx time out(0x0C). */
    case UART_INT_ID_RX_TMEOUT:
        rx_len = UART_GetRxFIFOLen(Register_p);
        UART_ReceiveData(UART, (uint8_t*)&uart_p->rx_buf.data[uart_p->rx_buf.count], rx_len);
        uart_p->rx_buf.count += rx_len;
        break;
    /* Receive line status interrupt(0x06). */
    case UART_INT_ID_LINE_STATUS:
        break;
    /* Rx data valiable(0x04). */
    case UART_INT_ID_RX_LEVEL_REACH:
        rx_len = UART_GetRxFIFOLen(Register_p);
        UART_ReceiveData(UART, (uint8_t*)&uart_p->rx_buf.data[uart_p->rx_buf.count], rx_len);
        uart_p->rx_buf.count += rx_len;
        break;
    /* Tx fifo empty(0x02), not enable. */
    case UART_INT_ID_TX_EMPTY:
        /* Do nothing */
        break;
    default:
        break;
    }
    /* enable interrupt again */
    UART_INTConfig(Register_p, UART_INT_RD_AVA, ENABLE);
    /* enable interrups again globally */
    __enable_irq();
}

/* attach generic handler to all possible uarts */
#define UART_HANDLER(INDEX)          \
    void UART##INDEX##_Handler(void) \
    {                                \
        _genericUARTHandler(INDEX);  \
    }

UART_HANDLER(0)
UART_HANDLER(1)
UART_HANDLER(2)
