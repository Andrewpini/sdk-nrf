/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <bluetooth/bluetooth.h>
#include <bluetooth/mesh/models.h>
#include <drivers/gpio.h>
#include <dk_buttons_and_leds.h>
#include "model_handler.h"
#include <thingy52_orientation_handler.h>
#include <drivers/gpio/gpio_sx1509b.h>

static struct bt_mesh_whistle_rgb_msg colors[3] = {
	{ .red = 0, .green = 0, .blue = 0 },
	{ .red = 255, .green = 255, .blue = 255 },
	{ .red = 0, .green = 0, .blue = 0 },
};

static struct device *io_expander;

static void led_init(void)
{
	int err = 0;

	io_expander = device_get_binding(DT_PROP(DT_NODELABEL(sx1509b), label));
	err |= sx1509b_led_drv_pin_init(io_expander, GREEN_LED);
	err |= sx1509b_led_drv_pin_init(io_expander, BLUE_LED);
	err |= sx1509b_led_drv_pin_init(io_expander, RED_LED);

	if (err || (io_expander == NULL)) {
		printk("GPIO configuration failed\n");
	}
}

static struct k_delayed_work device_attention_work;

static void device_attention(struct k_work *work)
{
	static uint8_t idx;
	sx1509b_set_pwm(io_expander, RED_LED, colors[idx % 3].red);
	sx1509b_set_pwm(io_expander, GREEN_LED, colors[idx % 3].green);
	sx1509b_set_pwm(io_expander, BLUE_LED, colors[idx % 3].blue);
	idx++;
	k_delayed_work_submit(&device_attention_work, K_MSEC(400));
}

/** Configuration server definition */
static struct bt_mesh_cfg_srv cfg_srv = {
	.relay = IS_ENABLED(CONFIG_BT_MESH_RELAY),
	.beacon = BT_MESH_BEACON_ENABLED,
	.frnd = IS_ENABLED(CONFIG_BT_MESH_FRIEND),
	.gatt_proxy = IS_ENABLED(CONFIG_BT_MESH_GATT_PROXY),
	.default_ttl = 7,

	/* 3 transmissions with 20ms interval */
	.net_transmit = BT_MESH_TRANSMIT(2, 20),
	.relay_retransmit = BT_MESH_TRANSMIT(2, 20),
};

static struct k_delayed_work attention_blink_work;

static void attention_blink(struct k_work *work)
{
	static int idx;
	const u8_t pattern[] = {
		GREEN_LED,
		BLUE_LED,
		RED_LED,
	};

	gpio_pin_toggle(io_expander, pattern[idx++ % ARRAY_SIZE(pattern)]);
	k_delayed_work_submit(&attention_blink_work, K_MSEC(30));
}

static void attention_on(struct bt_mesh_model *mod)
{
	k_delayed_work_submit(&attention_blink_work, K_NO_WAIT);
}

static void attention_off(struct bt_mesh_model *mod)
{
	k_delayed_work_cancel(&attention_blink_work);
}

static const struct bt_mesh_health_srv_cb health_srv_cb = {
	.attn_on = attention_on,
	.attn_off = attention_off,
};

static struct bt_mesh_health_srv health_srv = {
	.cb = &health_srv_cb,
};

BT_MESH_HEALTH_PUB_DEFINE(health_pub, 0);

void lvl_set_handler(struct bt_mesh_whistle_srv *srv,
		     struct bt_mesh_msg_ctx *ctx, uint16_t lvl)
{
	printk("Current lvl: %d\n", lvl);
}

void rgb_set_handler(struct bt_mesh_whistle_srv *srv,
		     struct bt_mesh_msg_ctx *ctx,
		     struct bt_mesh_whistle_rgb_msg rgb)
{
	printk("Current rgb: R:%d, G:%d, B:%d\n", rgb.red, rgb.green, rgb.blue);
	memcpy(&colors[2], &rgb, sizeof(struct bt_mesh_whistle_rgb_msg));
	sx1509b_set_pwm(io_expander, RED_LED, rgb.red);
	sx1509b_set_pwm(io_expander, GREEN_LED, rgb.green);
	sx1509b_set_pwm(io_expander, BLUE_LED, rgb.blue);
}

void attention_set_handler(struct bt_mesh_whistle_srv *srv,
			   struct bt_mesh_msg_ctx *ctx, bool onoff)
{
	printk("Attention:%d\n", onoff);
	if (onoff) {
		k_delayed_work_submit(&device_attention_work, K_NO_WAIT);
	} else {
		k_delayed_work_cancel(&device_attention_work);
                sx1509b_set_pwm(io_expander, RED_LED, colors[2].red);
                sx1509b_set_pwm(io_expander, GREEN_LED, colors[2].green);
                sx1509b_set_pwm(io_expander, BLUE_LED, colors[2].blue);
	}
}

struct bt_mesh_whistle_cb handlers = {
	.attention_set_handler = attention_set_handler,
	.lvl_set_handler = lvl_set_handler,
	.rgb_set_handler = rgb_set_handler,
};

static struct bt_mesh_whistle_srv whistle_srv =
	BT_MESH_WHISTLE_SRV_INIT(&handlers);

static struct bt_mesh_elem elements[] = {
	BT_MESH_ELEM(1,
		     BT_MESH_MODEL_LIST(BT_MESH_MODEL_CFG_SRV(&cfg_srv),
					BT_MESH_MODEL_HEALTH_SRV(&health_srv,
								 &health_pub)),
		     BT_MESH_MODEL_NONE),
	BT_MESH_ELEM(
		2, BT_MESH_MODEL_LIST(BT_MESH_MODEL_WHISTLE_SRV(&whistle_srv)),
		BT_MESH_MODEL_NONE),
};

static const struct bt_mesh_comp comp = {
	.cid = CONFIG_BT_COMPANY_ID,
	.elem = elements,
	.elem_count = ARRAY_SIZE(elements),
};

const struct bt_mesh_comp *model_handler_init(void)
{
	led_init();
	k_delayed_work_init(&attention_blink_work, attention_blink);
	k_delayed_work_init(&device_attention_work, device_attention);

	return &comp;
}
