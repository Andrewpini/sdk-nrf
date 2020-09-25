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
#include <errno.h>
#include "slip.h"

#define CHECKSUM_SIZE 2
#define MQTT_STATIC_SIZE 13
#define MQTT_PACKET_RECIEVED_SUCCESS 0

/* Uart device */
static struct device *uart_dev;

/* Tx thread for Uart */
static K_THREAD_STACK_DEFINE(tx_thread_stack, 1536);
static struct k_thread tx_thread_data;
static K_FIFO_DEFINE(tx_queue);

NET_BUF_POOL_VAR_DEFINE(uart_pool, 10, 1024, NULL);

static void uart_isr(struct device *unused, void *user_data)
{
	static uint8_t slip_buffer[128];
	static slip_t slip = {
			.state          = SLIP_STATE_DECODING,
			.p_buffer       = slip_buffer,
			.current_index  = 0,
			.buffer_len     = sizeof(slip_buffer)};

	ARG_UNUSED(unused);
	ARG_UNUSED(user_data);

	while (uart_irq_update(uart_dev) &&
	       uart_irq_is_pending(uart_dev)) {
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
		case MQTT_PACKET_RECIEVED_SUCCESS: {
			uint16_t checksum_local =
				crc16_ansi(slip.p_buffer,
					   slip.current_index - CHECKSUM_SIZE);
			uint16_t checksum_incoming;

			memcpy(&checksum_incoming,
			       slip.p_buffer +
				       (slip.current_index - CHECKSUM_SIZE),
			       sizeof(checksum_incoming));

			if (checksum_incoming == checksum_local) {
				printk("Correct Checksum\n");
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

static void mqtt_msg_parse(struct net_buf *buf, struct mqtt_publish_param *param)
{
	param->message.topic.qos = net_buf_pull_u8(buf);
	printk("QOS: %d\n", param->message.topic.qos);

	param->message_id = net_buf_pull_le16(buf);
	printk("Msg ID: %d\n", param->message_id);

	param->dup_flag = net_buf_pull_u8(buf);
	printk("DUP flag: %d\n", param->dup_flag);

	param->retain_flag = net_buf_pull_u8(buf);
	printk("Retain flag: %d\n", param->retain_flag);

	param->message.topic.topic.size = net_buf_pull_le32(buf);
	printk("Topic size: %d\n", param->message.topic.topic.size);

	param->message.topic.topic.utf8 = net_buf_pull_mem(buf, param->message.topic.topic.size);
	printk("Topic data: %s\n", param->message.topic.topic.utf8);

	param->message.payload.len = net_buf_pull_le32(buf);
	printk("Data size: %d\n", param->message.payload.len);

	param->message.payload.data = net_buf_pull_mem(buf, param->message.payload.len);
	printk("Data: %s\n", param->message.payload.data);

	printk("\n");
}

static void tx_thread(void *p1, void *p2, void *p3)
{
	while (1) {
		struct net_buf *get_buf = net_buf_get(&tx_queue, K_FOREVER);
		struct mqtt_publish_param param;

		mqtt_msg_parse(get_buf, &param);
		net_buf_unref(get_buf);
		k_yield();
	}
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
	uint8_t buf[MQTT_STATIC_SIZE + param.message.payload.len +
		    param.message.topic.topic.size + CHECKSUM_SIZE];
	uint8_t buf_idx = 0;

	/* Add QOS and message ID to outgoing packet buffer*/
	memcpy(buf + buf_idx, &param.message.topic.qos,
	       sizeof(param.message.topic.qos));
	buf_idx += sizeof(param.message.topic.qos);
	memcpy(buf + buf_idx, &param.message_id, sizeof(param.message_id));
	buf_idx += sizeof(param.message_id);

	/* Add Retain- and DUP flags to outgoing packet buffer*/
	buf[buf_idx] = param.dup_flag;
	buf_idx += sizeof(uint8_t);
	buf[buf_idx] = param.retain_flag;
	buf_idx += sizeof(uint8_t);

	/* Add Topic and Topic length to outgoing packet buffer*/
	memcpy(buf + buf_idx, &param.message.topic.topic.size,
	       sizeof(param.message.topic.topic.size));
	buf_idx += sizeof(param.message.topic.topic.size);
	memcpy(buf + buf_idx, param.message.topic.topic.utf8,
	       param.message.topic.topic.size);
	buf_idx += param.message.topic.topic.size;

	/* Add Data and Data length to outgoing packet buffer*/
	memcpy(buf + buf_idx, &param.message.payload.len,
	       sizeof(param.message.payload.len));
	buf_idx += sizeof(param.message.payload.len);
	memcpy(buf + buf_idx, param.message.payload.data,
	       param.message.payload.len);
	buf_idx += param.message.payload.len;

	/* Add Data and Data length to outgoing packet buffer*/
	uint16_t checksum = crc16_ansi(buf, sizeof(buf) - CHECKSUM_SIZE);

	memcpy(buf + buf_idx, &checksum, sizeof(checksum));
	buf_idx += sizeof(checksum);

	/* Wrap outgoing packet with SLIP */
	uint8_t slip_buf[sizeof(buf) * 2];
	uint32_t slip_buf_len;

	slip_encode(slip_buf, buf, sizeof(buf), &slip_buf_len);

	/* Send packet over UART */
	for (int i = 0; i < slip_buf_len; i++) {
		uart_poll_out(uart_dev, slip_buf[i]);
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

	param.message.topic.qos = 0;
	param.message.topic.topic.utf8 = "my/publish/topic";
	param.message.topic.topic.size = strlen("my/publish/topic");
	param.message.payload.data = "Erik_data";
	param.message.payload.len = strlen("Erik_data");
	param.message_id = 1000;
	param.dup_flag = 1;
	param.retain_flag = 1;

	while (1) {
		mqtt_send(param);
		k_sleep(K_MSEC(1000));
	}
}
