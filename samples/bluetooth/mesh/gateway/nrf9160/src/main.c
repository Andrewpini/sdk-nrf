/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>
#include <stdio.h>
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
#include "uart_simple.h"
#include "mqtt_serial.h"
#include "gw_nfc.h"
#include "gw_display_shield.h"

void mqtt_rx_callback(struct net_buf *get_buf);

static struct uart_channel_ctx mqtt_serial_chan = {
	.channel_id = 1,
	.rx_cb = mqtt_rx_callback,
};

/* Test thread */
static K_THREAD_STACK_DEFINE(mqtt_rx_thread_stack, 1536);
static struct k_thread mqtt_rx_thread_data;


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

/**@brief Function to print strings without null-termination
 */
static void data_print(uint8_t *prefix, uint8_t *data, size_t len)
{
	char buf[len + 1];

	memcpy(buf, data, len);
	buf[len] = 0;
	printk("%s%s\n", prefix, buf);
}

static int serial_publish(const struct mqtt_publish_param *param, uint8_t *data, size_t len)
{
	struct mqtt_publish_param out_param;

	out_param.message.topic.qos = param->message.topic.qos;
	out_param.message_id = param->message_id;
	out_param.dup_flag = param->dup_flag;
	out_param.retain_flag = param->retain_flag;
	out_param.message.topic.topic.size = param->message.topic.topic.size;
	out_param.message.topic.topic.utf8 = param->message.topic.topic.utf8;
	out_param.message.payload.len = len;
	out_param.message.payload.data = data;

	uint8_t out_buf[256];
	uint16_t msg_len;
	int err = mqtt_serial_msg_encode(&out_param, out_buf, sizeof(out_buf), &msg_len);

	if (!err) {
		uart_simple_send(&mqtt_serial_chan, out_buf, msg_len);
	}

	return 0;
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

			/* Send data over Uart */
			serial_publish(p, payload_buf, p->message.payload.len);

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
		err = poll(&fds, 1, mqtt_keepalive_time_left(&client));
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
			NULL, NULL, NULL, K_PRIO_COOP(6), 0, K_NO_WAIT);
	k_thread_name_set(&mqtt_rx_thread_data, "MQTT Rx");
}

void mqtt_rx_callback(struct net_buf *get_buf)
{

		struct mqtt_publish_param param;

		mqtt_serial_msg_decode(get_buf, &param);
		mqtt_publish(&client, &param);
}

void main(void)
{

	int err = 0;

	printk("MQTT bridge nrf9160 started\n");
	// modem_configure();

	// err |= client_init(&client);
	// err |= mqtt_connect(&client);
	// err |= fds_init(&client);

	// if (err != 0) {
	// 	printk("MQTT initialization failed\n");
	// 	return;
	// }

	// uart_simple_init(NULL);
	// uart_simple_channel_create(&mqtt_serial_chan);

	// mqtt_rx_thread_create();

	display_shield_init();

	display_set_cursor(0, 0);
	display_write_string("BT Mesh Gateway");
	display_set_cursor(0, 1);
	display_write_string("Press SELECT");

	printk("Written to display\n");

	// while (1) {

	// }
}
