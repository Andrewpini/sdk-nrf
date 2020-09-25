/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>
#include <stdio.h>
#include <string.h>
#include <drivers/uart.h>
#include <net/buf.h>
#include <net/mqtt.h>
#include <sys/crc.h>

#define CHECKSUM_SIZE 2

/* Uart device */
static struct device *uart_dev;

/* Tx thread for Uart */
static K_THREAD_STACK_DEFINE(tx_thread_stack, 1536);
static struct k_thread tx_thread_data;
static K_FIFO_DEFINE(tx_queue);

NET_BUF_POOL_VAR_DEFINE(uart_pool, 10, 1024, NULL);

static int uart_read(struct device *uart, uint8_t *buf,
		   size_t len, size_t min)
{
	int total = 0;

	while (len) {
		int rx;

		rx = uart_fifo_read(uart, buf, len);
		if (rx == 0) {
			if (total < min) {
				continue;
			}
			break;
		}

		len -= rx;
		total += rx;
		buf += rx;
	}

	return total;
}

static void uart_isr(struct device *unused, void *user_data)
{
	static struct net_buf *buf;
	static uint8_t remaining;

	ARG_UNUSED(unused);
	ARG_UNUSED(user_data);

	while (uart_irq_update(uart_dev) &&
	       uart_irq_is_pending(uart_dev)) {
		int read;
		uint16_t checksum;
		if (!uart_irq_rx_ready(uart_dev)) {
			if (uart_irq_tx_ready(uart_dev)) {
				printk("transmit ready\n");
			} else {
				printk("spurious interrupt\n");
			}
			/* Only the UART RX path is interrupt-enabled */
			break;
		}

		/* Beginning of a new packet */
		if (!remaining) {
			uart_read(uart_dev, &remaining, 1, 0);
			buf = net_buf_alloc_len(&uart_pool, remaining - CHECKSUM_SIZE,
						    K_NO_WAIT);
			printk("Rem: %d\n", remaining);
		}

		if (remaining > CHECKSUM_SIZE) {
			read = uart_read(uart_dev, net_buf_tail(buf), remaining,
					 0);
			buf->len += read;
			remaining -= read;
		} else if (remaining > 0) {
			read = uart_read(uart_dev, &checksum, sizeof(checksum),
					 2);
			remaining -= read;
		}

		if (!remaining) {
			if (crc16_ansi(buf->data, buf->len) == checksum) {
				printk("Correct Checksum\n");
				net_buf_put(&tx_queue, buf);
				buf = NULL;

			} else {
				net_buf_unref(buf);
				printk("Invalid Checksum\n");
				buf = NULL;
			}
		}
	}
}

static void tx_thread(void *p1, void *p2, void *p3)
{
	while (1) {
		struct net_buf *get_buf = net_buf_get(&tx_queue, K_FOREVER);

		for (size_t i = get_buf->len; i > 0; i--) {
			printk("%d-", net_buf_pull_u8(get_buf));
		}
		printk("\n");
		printk("\n");

		net_buf_unref(get_buf);
		k_yield();
	}
}

static void uart_cfg_read(void)
{
	struct uart_config u_cfg;
	uart_config_get(uart_dev, &u_cfg);
	printk("Baud: %d\n", u_cfg.baudrate);
	printk("Parity: %d\n", u_cfg.parity);
	printk("Stop bits: %d\n", u_cfg.stop_bits);
	printk("Data bits: %d\n", u_cfg.data_bits);
	printk("FC: %d\n", u_cfg.flow_ctrl);
}

static void uart_irq_init(void)
{
	uart_irq_rx_disable(uart_dev);
	uart_irq_tx_disable(uart_dev);
	uart_irq_callback_set(uart_dev, uart_isr);
	uart_irq_rx_enable(uart_dev);
}

static void tx_thread_create(void)
{
	k_thread_create(&tx_thread_data, tx_thread_stack,
			K_THREAD_STACK_SIZEOF(tx_thread_stack), tx_thread,
			NULL, NULL, NULL, K_PRIO_COOP(7), 0, K_NO_WAIT);
	k_thread_name_set(&tx_thread_data, "Uart TX");
}

static void mqtt_send(struct mqtt_publish_param param)
{
	uint8_t size = 5 + 8 + param.message.payload.len +
		       param.message.topic.topic.size + CHECKSUM_SIZE;
	printk("%d\n", size);
	uint8_t buf[size + 1];
	uint8_t buf_idx = 0;

	memcpy(buf, &size, sizeof(size));
	buf_idx += sizeof(size);

	memcpy(buf+buf_idx, &param.message.topic.qos, sizeof(param.message.topic.qos));
	buf_idx += sizeof(param.message.topic.qos);
	memcpy(buf+buf_idx, &param.message_id, sizeof(param.message_id));
	buf_idx += sizeof(param.message_id);

	buf[buf_idx] = param.dup_flag;
	buf_idx += sizeof(uint8_t);
	buf[buf_idx] = param.retain_flag;
	buf_idx += sizeof(uint8_t);

	// memcpy(buf+buf_idx, &param.dup_flag, sizeof(uint8_t));
	// buf_idx += sizeof(uint8_t);
	// memcpy(buf+buf_idx, &param.retain_flag, sizeof(uint8_t));
	// buf_idx += sizeof(uint8_t);

	memcpy(buf+buf_idx, &param.message.topic.topic.size, sizeof(param.message.topic.topic.size));
	buf_idx += sizeof(param.message.topic.topic.size);
	memcpy(buf+buf_idx, param.message.topic.topic.utf8, param.message.topic.topic.size);
	buf_idx += param.message.topic.topic.size;

	memcpy(buf+buf_idx, &param.message.payload.len, sizeof(param.message.payload.len));
	buf_idx += sizeof(param.message.payload.len);
	memcpy(buf+buf_idx, param.message.payload.data, param.message.payload.len);
	buf_idx += param.message.payload.len;

	uint16_t checksum = crc16_ansi(buf + sizeof(size), size - CHECKSUM_SIZE);
	printk("Checksum: %d\n", checksum);

	memcpy(buf+buf_idx, &checksum, sizeof(checksum));
	buf_idx += sizeof(checksum);

	for (size_t i = 0; i < sizeof(buf); i++) {
		printk("%d-", buf[i]);
	}
	printk("\n");
	// for (size_t i = 0; i < sizeof(buf); i++) {
	// 	printk("%c-", buf[i]);
	// }
	// printk("\n");

	for (int i = 0; i < sizeof(buf); i++) {
		uart_poll_out(uart_dev, buf[i]);
	}
}

void main(void)
{
	printk("The Uart test sample started\n");

	uart_dev = device_get_binding("UART_1");

	uart_cfg_read();
	uart_irq_init();
	tx_thread_create();

	struct mqtt_publish_param param;

	param.message.topic.qos = 1;
	param.message.topic.topic.utf8 = "my/publish/topicqweasd";
	param.message.topic.topic.size = strlen("my/publish/topicqweasd");
	param.message.payload.data = "my_data";
	param.message.payload.len = strlen("my_data");
	param.message_id = 99;
	param.dup_flag = 0;
	param.retain_flag = 0;

	while (1) {
		mqtt_send(param);

		// const struct {
		// 	const uint8_t tot_size;
		// 	const char topic_size;
		// 	const char topic[7];
		// 	const char data_size;
		// 	const char data[15];
		// } __packed mqtt_pkg2 = {
		// 	.tot_size = 24,
		// 	.topic_size = 7,
		// 	.topic = "Anders",
		// 	.data_size = 15,
		// 	.data = "Hestvik Storro",

		// };


		// for (int i = 0; i < sizeof(mqtt_pkg2); i++) {
		// 	uart_poll_out(uart_dev,
		// 		      *(((const uint8_t *)&mqtt_pkg2)+i));
		// }

		// k_sleep(K_MSEC(500));

		// const struct {
		// 	const uint8_t tot_size;
		// 	const char topic_size;
		// 	const char topic[5];
		// 	const char data_size;
		// 	const char data[8];
		// } __packed mqtt_pkg = {
		// 	.tot_size = 15,
		// 	.topic_size = 5,
		// 	.topic = "Erik",
		// 	.data_size = 8,
		// 	.data = "Robstad",

		// };

		// for (int i = 0; i < sizeof(mqtt_pkg); i++) {
		// 	uart_poll_out(uart_dev,
		// 		      *(((const uint8_t *)&mqtt_pkg)+i));
		// }


		k_sleep(K_MSEC(1000));
	}
}
