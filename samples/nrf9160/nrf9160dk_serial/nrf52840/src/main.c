/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>
#include <stdio.h>
#include <net/buf.h>
#include "uart_simple.h"

void mqtt_rx_callback(struct net_buf *get_buf);

static struct uart_channel_ctx mqtt_serial_chan = {
	.channel_id = 1,
	.rx_cb = mqtt_rx_callback,
};

void mqtt_rx_callback(struct net_buf *get_buf)
{
	int len = get_buf->len;
	char *msg = net_buf_pull_mem(get_buf, get_buf->len);

	for (size_t i = 0; i < len; i++) {
		printk("%c", msg[i]);
	}
	uart_simple_send(&mqtt_serial_chan, (uint8_t*)msg, len);
}

void main(void)
{
	printk("nrf9160DK_52840 serial sample started\n");

	uart_simple_init();
	uart_simple_channel_create(&mqtt_serial_chan);
}
