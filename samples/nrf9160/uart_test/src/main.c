/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>
#include <stdio.h>
#include <drivers/uart.h>

#define BUF_SIZE 64
static K_MEM_SLAB_DEFINE(uart_slab, BUF_SIZE, 3, 4);
static struct device *uart_dev;

static void uart_callback(struct device *dev,
			  struct uart_event *evt, void *user_data)
{
	struct device *uart = user_data;
	int err;


	switch (evt->type) {
	case UART_TX_DONE:
		printk("Tx sent %d bytes\n", evt->data.tx.len);
		break;

	case UART_TX_ABORTED:
		printk("Tx aborted\n");
		break;

	case UART_RX_RDY:
		printk("Received data %d bytes\n", evt->data.rx.len);
		break;

	case UART_RX_BUF_REQUEST:
	{
		uint8_t *buf;

		// err = k_mem_slab_alloc(&uart_slab, (void **)&buf, K_NO_WAIT);

		// err = uart_rx_buf_rsp(uart, buf, BUF_SIZE);
		break;
	}

	case UART_RX_BUF_RELEASED:
		k_mem_slab_free(&uart_slab, (void **)&evt->data.rx_buf.buf);
		break;

	case UART_RX_DISABLED:
		break;

	case UART_RX_STOPPED:
		break;
	}
}

void test_chained_read_callback(struct device *uart_dev,
				struct uart_event *evt, void *user_data)
{
	switch (evt->type) {
	case UART_TX_DONE:
		printk("UART_TX_DONE\n");
		break;
	case UART_RX_RDY:{
		// char *msg;
		printk("UART_RX_RDY:\n");
		// memcpy(msg, evt->data.rx.buf, evt->data.rx.len);
		// for (size_t i = 0; i < evt->data.rx.len; i++)
		// {
		printk("%s\n", evt->data.rx.buf + evt->data.rx.offset);
		// }
		// printk("\n");
		break;}
	case UART_RX_BUF_REQUEST:
		printk("UART_RX_BUF_REQUEST\n");

		uint8_t *buf;
		k_mem_slab_alloc(&uart_slab, (void **)&buf, K_NO_WAIT);
		uart_rx_buf_rsp(uart_dev, buf, BUF_SIZE);

		break;
	case UART_RX_DISABLED:
		printk("UART_RX_DISABLED\n");
		break;

	case UART_RX_BUF_RELEASED:
		printk("UART_RX_BUF_RELEASED\n");
		k_mem_slab_free(&uart_slab, (void **)&evt->data.rx_buf.buf);
		break;
	default:
		break;
	}

}

static void uart_isr(struct device *unused, void *user_data)
{

	ARG_UNUSED(unused);
	ARG_UNUSED(user_data);

	while (uart_irq_update(uart_dev) &&
	       uart_irq_is_pending(uart_dev)) {
		int read;

		if (!uart_irq_rx_ready(uart_dev)) {
			if (uart_irq_tx_ready(uart_dev)) {
				printk("transmit ready\n");
			} else {
				printk("spurious interrupt\n");
			}
			/* Only the UART RX path is interrupt-enabled */
			break;
		}

		uint8_t buf;
		uart_fifo_read(uart_dev, &buf, 1);
		printk("Char: %c\n", buf);
		return;

	}
}


void main(void)
{
	printk("The Uart test sample started\n");

	uart_dev = device_get_binding("UART_1");

	// uart_callback_set(uart_dev, test_chained_read_callback, NULL);
	uint8_t *buf;
	k_mem_slab_alloc(&uart_slab, (void **)&buf, K_NO_WAIT);
	// uart_rx_enable(uart_dev, buf, BUF_SIZE, 10);
	char txbuf[7] = "Storro";

	struct uart_config u_cfg;

	uart_config_get(uart_dev, &u_cfg);

	printk("Baud: %d\n", u_cfg.baudrate);
	printk("Parity: %d\n", u_cfg.parity);
	printk("Stop bits: %d\n", u_cfg.stop_bits);
	printk("Data bits: %d\n", u_cfg.data_bits);
	printk("FC: %d\n", u_cfg.flow_ctrl);


	uart_irq_rx_disable(uart_dev);
	uart_irq_tx_disable(uart_dev);

	uart_irq_callback_set(uart_dev, uart_isr);

	uart_irq_rx_enable(uart_dev);


	while (1) {
		char txbuf[7] = "Storro";
		unsigned char out_char = 'a';

		for (size_t i = 0; i < sizeof(txbuf); i++)
		{
			uart_poll_out(uart_dev, txbuf[i]);
		}

		// uart_poll_out(uart_dev, out_char);

		k_sleep(K_MSEC(500));
	}

}
