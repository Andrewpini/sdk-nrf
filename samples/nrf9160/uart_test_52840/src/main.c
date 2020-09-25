/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>
#include <stdio.h>
#include <drivers/uart.h>

static struct device *uart_dev;

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
	printk("The Uart test 52840 sample started\n");

	uart_dev = device_get_binding("UART_1");
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
		char txbuf[7] = "Anders";
		unsigned char out_char = 'a';

		for (size_t i = 0; i < sizeof(txbuf); i++)
		{
			uart_poll_out(uart_dev, txbuf[i]);
		}

		// uart_poll_out(uart_dev, out_char);

		k_sleep(K_MSEC(500));
	}

}
