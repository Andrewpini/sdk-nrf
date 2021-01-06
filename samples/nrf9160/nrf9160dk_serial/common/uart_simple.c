/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>
#include <stdio.h>
#include <string.h>
#include <drivers/uart.h>
#include <net/buf.h>
#include <sys/crc.h>
#include <errno.h>
#include "slip.h"
#include "uart_simple.h"

#define CHECKSUM_SIZE 2
#define UART_PACKET_RECIEVED_SUCCESS 0
#define CHANNEL_ID_SIZE 1
#define CHANNEL_CTX_MAX 4
#define SLIP_BUF_SIZE 512
#define UART_RX_THREAD_STACK_SIZE 4096
#define UART_RX_THREAD_PRIORITY 7
#define UART_RX_POOL_SIZE 2048
#define UART_RX_POOL_MAX_ENTRIES 16
#define UART_TX_BUF_SIZE 512

K_MUTEX_DEFINE(uart_send_mtx);

/* Uart device */
static const struct device *uart_dev;
static rx_cb channel_list[CHANNEL_CTX_MAX];

/* Tx thread for Uart */
static K_THREAD_STACK_DEFINE(rx_thread_stack, UART_RX_THREAD_STACK_SIZE);
static struct k_thread rx_thread_data;
static K_FIFO_DEFINE(tx_queue);

NET_BUF_POOL_VAR_DEFINE(uart_pool, UART_RX_POOL_MAX_ENTRIES, UART_RX_POOL_SIZE,
			NULL);

static void uart_isr(const struct device *unused, void *user_data)
{
	static uint8_t slip_buffer[SLIP_BUF_SIZE];
	static slip_t slip = { .state = SLIP_STATE_DECODING,
			       .p_buffer = slip_buffer,
			       .current_index = 0,
			       .buffer_len = sizeof(slip_buffer) };

	ARG_UNUSED(unused);
	ARG_UNUSED(user_data);

	while (uart_irq_update(uart_dev) && uart_irq_is_pending(uart_dev)) {
		if (!uart_irq_rx_ready(uart_dev)) {
			if (uart_irq_tx_ready(uart_dev)) {
				printk("transmit ready\n");
			} else {
				printk("spurious interrupt\n");
			}
			/* Only the UART RX path is interrupt-enabled */
			break;
		}

		uint8_t byte;

		uart_fifo_read(uart_dev, &byte, sizeof(byte));
		int ret_code = slip_decode_add_byte(&slip, byte);

		switch (ret_code) {
		case UART_PACKET_RECIEVED_SUCCESS: {
			uint16_t checksum_local =
				crc16_ansi(slip.p_buffer,
					   slip.current_index - CHECKSUM_SIZE);
			uint16_t checksum_incoming;

			memcpy(&checksum_incoming,
			       slip.p_buffer +
				       (slip.current_index - CHECKSUM_SIZE),
			       sizeof(checksum_incoming));

			if (checksum_incoming == checksum_local) {
				struct net_buf *buf = net_buf_alloc_len(
					&uart_pool,
					slip.current_index - CHECKSUM_SIZE,
					K_NO_WAIT);
				net_buf_add_mem(buf, slip.p_buffer,
						slip.current_index -
							CHECKSUM_SIZE);
				net_buf_put(&tx_queue, buf);
			} else {
				printk("Invalid Checksum\n");
			}
		}
			// fall through
		case -ENOMEM:
			slip.current_index = 0;
			slip.state = SLIP_STATE_DECODING;
			break;

		default:
			break;
		}
	}
}

static void uart_irq_init(void)
{
	uart_irq_rx_disable(uart_dev);
	uart_irq_tx_disable(uart_dev);
	uart_irq_callback_set(uart_dev, uart_isr);
	uart_irq_rx_enable(uart_dev);
}

static void rx_thread(void *p1, void *p2, void *p3)
{
	while (1) {
		struct net_buf *get_buf = net_buf_get(&tx_queue, K_FOREVER);
		uint8_t id = net_buf_pull_u8(get_buf);

		if (channel_list[id]) {
			channel_list[id](get_buf);
		} else {
			printk("Incoming Uart packet thrown away\n");
		}

		net_buf_unref(get_buf);
		k_yield();
	}
}

static void rx_thread_create(void)
{
	k_thread_create(&rx_thread_data, rx_thread_stack,
			K_THREAD_STACK_SIZEOF(rx_thread_stack), rx_thread, NULL,
			NULL, NULL, K_PRIO_PREEMPT(UART_RX_THREAD_PRIORITY), 0,
			K_NO_WAIT);
	k_thread_name_set(&rx_thread_data, "Uart Simple Rx");
}

void uart_simple_send(struct uart_channel_ctx *channel_ctx, uint8_t *data,
		      uint16_t len)
{
	if (UART_TX_BUF_SIZE < (len + CHECKSUM_SIZE + CHANNEL_ID_SIZE)) {
		printk("Uart message length exceeds maximum allowed size\n");
		return;
	}

	if ((!channel_list[channel_ctx->channel_id]) ||
	    (channel_list[channel_ctx->channel_id] != channel_ctx->rx_cb)) {
		printk("This Uart channel is not properly initialized\n");
		return;
	}

	k_mutex_lock(&uart_send_mtx, K_FOREVER);
	static uint8_t buf[UART_TX_BUF_SIZE];
	uint8_t buf_idx = 0;

	memcpy(buf + buf_idx, &channel_ctx->channel_id, CHANNEL_ID_SIZE);
	buf_idx += CHANNEL_ID_SIZE;

	memcpy(buf + buf_idx, data, len);
	buf_idx += len;

	/* Add Data and Data length to outgoing packet buffer*/
	uint16_t checksum = crc16_ansi(buf, len + CHANNEL_ID_SIZE);

	memcpy(buf + buf_idx, &checksum, sizeof(checksum));

	/* Wrap outgoing packet with SLIP */
	static uint8_t slip_buf[UART_TX_BUF_SIZE * 2];
	uint32_t slip_buf_len;

	slip_encode(slip_buf, buf, (len + CHECKSUM_SIZE + CHANNEL_ID_SIZE),
		    &slip_buf_len);

	/* Send packet over UART */
	for (int i = 0; i < slip_buf_len; i++) {
		uart_poll_out(uart_dev, slip_buf[i]);
	}
	k_mutex_unlock(&uart_send_mtx);
}

void uart_simple_channel_create(struct uart_channel_ctx *channel_ctx)
{
	if (channel_list[channel_ctx->channel_id]) {
		printk("Uart channel %d is already in use\n",
		       channel_ctx->channel_id);
		return;
	}

	if (!channel_ctx->rx_cb) {
		printk("Uart channel callback must be provided\n");
		return;
	}

	channel_list[channel_ctx->channel_id] = channel_ctx->rx_cb;
}

void uart_simple_channel_delete(struct uart_channel_ctx *channel_ctx)
{
	channel_list[channel_ctx->channel_id] = NULL;
}


void uart_simple_init(void)
{
	printk("Uart Simple started\n");

	uart_dev = device_get_binding("UART_1");

	uart_irq_init();
	rx_thread_create();
}
