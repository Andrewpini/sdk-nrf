/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>
#include <stdio.h>
#include <drivers/uart.h>
#include <string.h>
#include <random/rand32.h>
#include <net/mqtt.h>
#include <net/socket.h>
#include <modem/lte_lc.h>
#if defined(CONFIG_MODEM_KEY_MGMT)
#include <modem/modem_key_mgmt.h>
#endif
#if defined(CONFIG_LWM2M_CARRIER)
#include <lwm2m_carrier.h>
#endif

#include "certificates.h"



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
static K_THREAD_STACK_DEFINE(uart_tx_thread_stack, 1536);
static struct k_thread uart_tx_thread_data;
static K_FIFO_DEFINE(tx_queue);

/* Test thread */
static K_THREAD_STACK_DEFINE(mqtt_rx_thread_stack, 1536);
static struct k_thread mqtt_rx_thread_data;

NET_BUF_POOL_VAR_DEFINE(uart_pool, 10, 1024, NULL);


/* Buffers for MQTT client. */
static uint8_t rx_buffer[CONFIG_MQTT_MESSAGE_BUFFER_SIZE];
static uint8_t tx_buffer[CONFIG_MQTT_MESSAGE_BUFFER_SIZE];
static uint8_t payload_buf[CONFIG_MQTT_PAYLOAD_BUFFER_SIZE];

struct mqtt_utf8 password = {
    .utf8 = CONFIG_MQTT_PW,
    .size = sizeof(CONFIG_MQTT_PW) - 1,
};

struct mqtt_utf8 user = {
    .utf8 = CONFIG_MQTT_USER,
    .size = sizeof(CONFIG_MQTT_USER) - 1,
};

/* The mqtt client struct */
static struct mqtt_client client;

/* MQTT Broker details. */
static struct sockaddr_storage broker;

/* File descriptor */
static struct pollfd fds;

/* TEST */
static uint16_t sent;
static uint16_t recived;
K_MUTEX_DEFINE(my_mutex);

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
	param->message_id = net_buf_pull_le16(buf);
	param->dup_flag = net_buf_pull_u8(buf);
	param->retain_flag = net_buf_pull_u8(buf);
	param->message.topic.topic.size = net_buf_pull_le32(buf);
	param->message.topic.topic.utf8 = net_buf_pull_mem(buf, param->message.topic.topic.size);
	param->message.payload.len = net_buf_pull_le32(buf);
	param->message.payload.data = net_buf_pull_mem(buf, param->message.payload.len);
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

	recived++;
	printk("Sent: %d, Recived: %d\n\n", sent, recived);
	/* Send packet over UART */
	for (int i = 0; i < slip_buf_len; i++) {
		uart_poll_out(uart_dev, slip_buf[i]);
	}
}

/**@brief Function to print strings without null-termination
 */
static void data_print(uint8_t *prefix, uint8_t *data, size_t len)
{
	char buf[len + 1];

	memcpy(buf, data, len);
	buf[len] = 0;
	printk("%s%s\n", prefix, buf);
}

/**@brief Function to publish data on the configured topic
 */
static int data_publish(struct mqtt_client *c, enum mqtt_qos qos,
	uint8_t *data, size_t len)
{
	struct mqtt_publish_param param;

	param.message.topic.qos = qos;
	param.message.topic.topic.utf8 = CONFIG_MQTT_PUB_TOPIC;
	param.message.topic.topic.size = strlen(CONFIG_MQTT_PUB_TOPIC);
	param.message.payload.data = data;
	param.message.payload.len = len;
	param.message_id = sys_rand32_get();
	param.dup_flag = 0;
	param.retain_flag = 0;

	data_print("Publishing: ", data, len);
	printk("to topic: %s len: %u\n",
		CONFIG_MQTT_PUB_TOPIC,
		(unsigned int)strlen(CONFIG_MQTT_PUB_TOPIC));
	mqtt_send(param);
	// return mqtt_publish(c, &param);
}

static void test_send_over_uart(const struct mqtt_publish_param *p)
{
	struct mqtt_publish_param param;
	memcpy(&param, p, sizeof(param));

	// param.message.topic.qos = qos;
	// param.message.topic.topic.utf8 = CONFIG_MQTT_PUB_TOPIC;
	// param.message.topic.topic.size = strlen(CONFIG_MQTT_PUB_TOPIC);
	// param.message.payload.data = data;
	// param.message.payload.len = len;
	// param.message_id = sys_rand32_get();
	// param.dup_flag = 0;
	// param.retain_flag = 0;

	// mqtt_send(param);
}

/**@brief Function to read the published payload.
 */
static int publish_get_payload(struct mqtt_client *c, size_t length)
{
	uint8_t *buf = payload_buf;
	uint8_t *end = buf + length;

	if (length > sizeof(payload_buf)) {
		return -EMSGSIZE;
	}

	while (buf < end) {
		int ret = mqtt_read_publish_payload(c, buf, end - buf);

		if (ret < 0) {
			int err;

			if (ret != -EAGAIN) {
				return ret;
			}

			printk("mqtt_read_publish_payload: EAGAIN\n");

			err = poll(&fds, 1,
				   CONFIG_MQTT_KEEPALIVE * MSEC_PER_SEC);
			if (err > 0 && (fds.revents & POLLIN) == POLLIN) {
				continue;
			} else {
				return -EIO;
			}
		}

		if (ret == 0) {
			return -EIO;
		}

		buf += ret;
	}

	return 0;
}

/**@brief MQTT client event handler
 */
void mqtt_evt_handler(struct mqtt_client *const c, const struct mqtt_evt *evt)
{
	int err;

	switch (evt->type) {
	case MQTT_EVT_CONNACK: {
		if (evt->result == 0) {
			printk("MQTT client connected!\n");

			struct mqtt_topic subscribe_topic = {
				.topic = { .utf8 = CONFIG_MQTT_SUB_TOPIC,
					   .size = strlen(
						   CONFIG_MQTT_SUB_TOPIC) },
				.qos = MQTT_QOS_1_AT_LEAST_ONCE
			};

			const struct mqtt_subscription_list subscription_list = {
				.list = &subscribe_topic,
				.list_count = 1,
				.message_id = 1234
			};

			mqtt_subscribe(&client, &subscription_list);
		}

		break;
	}

	case MQTT_EVT_PUBLISH: {
		const struct mqtt_publish_param *p = &evt->param.publish;

		printk("[%s:%d] MQTT PUBLISH result=%d len=%d\n", __func__,
		       __LINE__, evt->result, p->message.payload.len);
		err = publish_get_payload(c, p->message.payload.len);
		if (err >= 0) {
			data_print("Received: ", payload_buf,
				p->message.payload.len);
			/* Echo back received data */
			// test_send_over_uart(p);
			data_publish(&client, MQTT_QOS_1_AT_LEAST_ONCE,
				payload_buf, p->message.payload.len);
		} else {
			printk("mqtt_read_publish_payload: Failed! %d\n", err);
			printk("Disconnecting MQTT client...\n");

			err = mqtt_disconnect(c);
			if (err) {
				printk("Could not disconnect: %d\n", err);
			}
		}
		break;
	}

	case MQTT_EVT_DISCONNECT:
	case MQTT_EVT_PUBACK:
	case MQTT_EVT_SUBACK:
	case MQTT_EVT_PINGRESP:
	default:
		if (evt->result != 0) {
			printk("SOMETHING WENT WRONG\n");
		}
		break;
	}
}

/**@brief Resolves the configured hostname and
 * initializes the MQTT broker structure
 */
static int broker_init(void)
{
	int err;
	struct addrinfo *result;
	struct addrinfo *addr;
	struct addrinfo hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM
	};

	err = getaddrinfo(CONFIG_MQTT_BROKER_HOSTNAME, NULL, &hints, &result);
	if (err) {
		printk("ERROR: getaddrinfo failed %d\n", err);
		return -ECHILD;
	}

	addr = result;

	/* Look for address of the broker. */
	while (addr != NULL) {
		/* IPv4 Address. */
		if (addr->ai_addrlen == sizeof(struct sockaddr_in)) {
			struct sockaddr_in *broker4 =
				((struct sockaddr_in *)&broker);
			char ipv4_addr[NET_IPV4_ADDR_LEN];

			broker4->sin_addr.s_addr =
				((struct sockaddr_in *)addr->ai_addr)
				->sin_addr.s_addr;
			broker4->sin_family = AF_INET;
			broker4->sin_port = htons(CONFIG_MQTT_BROKER_PORT);

			inet_ntop(AF_INET, &broker4->sin_addr.s_addr,
				  ipv4_addr, sizeof(ipv4_addr));
			printk("IPv4 Address found %s\n", ipv4_addr);
			break;
		} else {
			printk("ai_addrlen = %u should be %u or %u\n",
				(unsigned int)addr->ai_addrlen,
				(unsigned int)sizeof(struct sockaddr_in),
				(unsigned int)sizeof(struct sockaddr_in6));
		}

		addr = addr->ai_next;
	}

	/* Free the address. */
	freeaddrinfo(result);

	return err;
}

/**@brief Initialize the MQTT client structure
 */
static int client_init(struct mqtt_client *client)
{
	int err;

	mqtt_client_init(client);

	err = broker_init();
	if (err) {
		printk("Failed to initialize broker connection\n");
		return err;
	}

	/* MQTT client configuration */
	client->broker = &broker;
	client->evt_cb = mqtt_evt_handler;
	client->client_id.utf8 = (uint8_t *)CONFIG_MQTT_CLIENT_ID;
	client->client_id.size = strlen(CONFIG_MQTT_CLIENT_ID);
	client->password = &password;
	client->user_name = &user;
	client->protocol_version = MQTT_VERSION_3_1_1;

	/* MQTT buffers configuration */
	client->rx_buf = rx_buffer;
	client->rx_buf_size = sizeof(rx_buffer);
	client->tx_buf = tx_buffer;
	client->tx_buf_size = sizeof(tx_buffer);

	/* MQTT transport configuration */
	client->transport.type = MQTT_TRANSPORT_NON_SECURE;

	return err;
}

/**@brief Initialize the file descriptor structure used by poll.
 */
static int fds_init(struct mqtt_client *c)
{
	if (c->transport.type == MQTT_TRANSPORT_NON_SECURE) {
		fds.fd = c->transport.tcp.sock;
	} else {
		return -ENOTSUP;
	}

	fds.events = POLLIN;
	return 0;
}

/**@brief Configures modem to provide LTE link. Blocks until link is
 * successfully established.
 */
static void modem_configure(void)
{
	int err;

	printk("LTE Link Connecting ...\n");
	err = lte_lc_init_and_connect();
	__ASSERT(err == 0, "LTE link could not be established.");
	printk("LTE Link Connected!\n");
}

static void mqtt_rx_thread(void *p1, void *p2, void *p3)
{
	int err = 0;
	while (1) {
		err = poll(&fds, 1, 10000);
		if (err < 0) {
			printk("ERROR: poll %d\n", errno);
			break;
		}

		err = mqtt_live(&client);
		if ((err != 0) && (err != -EAGAIN)) {
			printk("ERROR: mqtt_live %d\n", err);
			break;
		}

		if ((fds.revents & POLLIN) == POLLIN) {
			err = mqtt_input(&client);
			if (err != 0) {
				printk("ERROR: mqtt_input %d\n", err);
				break;
			}
		}

		if ((fds.revents & POLLERR) == POLLERR) {
			printk("POLLERR\n");
			break;
		}

		if ((fds.revents & POLLNVAL) == POLLNVAL) {
			printk("POLLNVAL\n");
			break;
		}
		k_yield();
	}

	printk("Disconnecting MQTT client...\n");

	err = mqtt_disconnect(&client);
	if (err) {
		printk("Could not disconnect MQTT client. Error: %d\n", err);
	}
}

static void mqtt_rx_thread_create(void)
{
	k_thread_create(&mqtt_rx_thread_data, mqtt_rx_thread_stack,
			K_THREAD_STACK_SIZEOF(mqtt_rx_thread_stack), mqtt_rx_thread,
			NULL, NULL, NULL, K_PRIO_COOP(7), 0, K_NO_WAIT);
	k_thread_name_set(&mqtt_rx_thread_data, "MQTT Rx");
}

static void uart_tx_thread(void *p1, void *p2, void *p3)
{
	while (1) {
		struct net_buf *get_buf = net_buf_get(&tx_queue, K_FOREVER);
		struct mqtt_publish_param param;

		mqtt_msg_parse(get_buf, &param);
		mqtt_publish(&client, &param);
		net_buf_unref(get_buf);
		k_yield();
	}
}

static void uart_tx_thread_create(void)
{
	k_thread_create(&uart_tx_thread_data, uart_tx_thread_stack,
			K_THREAD_STACK_SIZEOF(uart_tx_thread_stack), uart_tx_thread,
			NULL, NULL, NULL, K_PRIO_COOP(7), 0, K_NO_WAIT);
	k_thread_name_set(&uart_tx_thread_data, "Uart TX");
}

void main(void)
{
	int err = 0;

	printk("The MQTT simple sample started\n");
	modem_configure();

	err |= client_init(&client);
	err |= mqtt_connect(&client);
	err |= fds_init(&client);

	if (err != 0) {
		printk("MQTT initialization failed\n");
		return;
	}

	uart_dev = device_get_binding("UART_1");

	uart_cfg_read();
	uart_irq_init();
	uart_tx_thread_create();
	mqtt_rx_thread_create();
}
