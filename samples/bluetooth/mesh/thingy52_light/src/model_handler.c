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

static void led_set(struct bt_mesh_onoff_srv *srv, struct bt_mesh_msg_ctx *ctx,
		    const struct bt_mesh_onoff_set *set,
		    struct bt_mesh_onoff_status *rsp);

static void led_get(struct bt_mesh_onoff_srv *srv, struct bt_mesh_msg_ctx *ctx,
		    struct bt_mesh_onoff_status *rsp);

static const struct bt_mesh_onoff_srv_handlers onoff_handlers = {
	.set = led_set,
	.get = led_get,
};

struct led_ctx {
	struct bt_mesh_onoff_srv srv;
	bool value;
};

static struct led_ctx led_ctx = {
	.srv = BT_MESH_ONOFF_SRV_INIT(&onoff_handlers),
};

static void led_status(struct led_ctx *led, struct bt_mesh_onoff_status *status)
{
	status->remaining_time = 0;
	status->target_on_off = led->value;
	status->present_on_off = led->value;
}

static void led_set(struct bt_mesh_onoff_srv *srv, struct bt_mesh_msg_ctx *ctx,
		    const struct bt_mesh_onoff_set *set,
		    struct bt_mesh_onoff_status *rsp)
{
	struct led_ctx *led = CONTAINER_OF(srv, struct led_ctx, srv);

	if (set->on_off == led->value) {
		goto respond;
	}

	led->value = set->on_off;

respond:
	if (rsp) {
		led_status(led, rsp);
	}
}

static void led_get(struct bt_mesh_onoff_srv *srv, struct bt_mesh_msg_ctx *ctx,
		    struct bt_mesh_onoff_status *rsp)
{
	struct led_ctx *led = CONTAINER_OF(srv, struct led_ctx, srv);

	led_status(led, rsp);
}

static struct device *io_expander;

static void led_init(void)
{
	int err = 0;

	io_expander = device_get_binding(DT_PROP(DT_NODELABEL(sx1509b), label));
	err |= gpio_pin_configure(io_expander, GREEN_LED, GPIO_OUTPUT);
	err |= gpio_pin_configure(io_expander, BLUE_LED, GPIO_OUTPUT);
	err |= gpio_pin_configure(io_expander, RED_LED, GPIO_OUTPUT);

	if (err || (io_expander == NULL)) {
		printk("GPIO configuration failed\n");
	}
}

static struct k_delayed_work orentiation_led_work;

static void leds_off(void)
{
	gpio_pin_set_raw(io_expander, BLUE_LED, 1);
	gpio_pin_set_raw(io_expander, GREEN_LED, 1);
	gpio_pin_set_raw(io_expander, RED_LED, 1);
}

static void leds_on(uint8_t val)
{
	gpio_pin_set_raw(io_expander, RED_LED, (((val % 8) >> 2) & 1));
	gpio_pin_set_raw(io_expander, GREEN_LED, (((val % 8) >> 1) & 1));
	gpio_pin_set_raw(io_expander, BLUE_LED, (((val % 8) >> 0) & 1));
}

static void orentiation_led(struct k_work *work)
{
	if (led_ctx.value) {
		static uint8_t cnt;
		leds_on(cnt++);
	} else {
		leds_off();
	}
	k_delayed_work_submit(&orentiation_led_work, K_MSEC(250));
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
	k_delayed_work_cancel(&orentiation_led_work);
	k_delayed_work_submit(&attention_blink_work, K_NO_WAIT);
}

static void attention_off(struct bt_mesh_model *mod)
{
	k_delayed_work_cancel(&attention_blink_work);
	k_delayed_work_submit(&orentiation_led_work, K_NO_WAIT);
}

static const struct bt_mesh_health_srv_cb health_srv_cb = {
	.attn_on = attention_on,
	.attn_off = attention_off,
};

static struct bt_mesh_health_srv health_srv = {
	.cb = &health_srv_cb,
};

BT_MESH_HEALTH_PUB_DEFINE(health_pub, 0);

static struct bt_mesh_elem elements[] = {
	BT_MESH_ELEM(1,
		     BT_MESH_MODEL_LIST(BT_MESH_MODEL_CFG_SRV(&cfg_srv),
					BT_MESH_MODEL_HEALTH_SRV(&health_srv,
								 &health_pub),
					BT_MESH_MODEL_ONOFF_SRV(&led_ctx.srv)),
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
	k_delayed_work_init(&orentiation_led_work, orentiation_led);
	k_delayed_work_submit(&orentiation_led_work, K_NO_WAIT);

	return &comp;
}
