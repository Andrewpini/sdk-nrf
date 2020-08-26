/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <bluetooth/bluetooth.h>
#include <bluetooth/mesh/models.h>
#include <drivers/gpio.h>
#include <drivers/gpio/gpio_sx1509b.h>
#include <drivers/pwm.h>
#include "model_handler.h"
#include "orientation_handler.h"
#include "thingy52_srv.h"

#define SPEAKER_PWM_FREQ 880
#define RGB_WORK_ITEM_MAX 5

void rgb_set_handler(struct bt_mesh_thingy52_srv *srv,
		     struct bt_mesh_msg_ctx *ctx,
		     struct bt_mesh_thingy52_rgb_msg rgb);

struct devices {
	struct device *io_expander;
	struct device *gpio_0;
	struct device *spkr_pwm;
	struct device *orientation;
};

struct rgb_work {
	struct k_delayed_work work;
	struct bt_mesh_thingy52_rgb_msg rgb_msg;
};

struct orientation_change_ctx {
	struct k_delayed_work work;
	uint8_t orient;
};

static struct devices dev;
static struct bt_mesh_thingy52_rgb_msg cur_rgb_msg;
static struct rgb_work rgb_work[RGB_WORK_ITEM_MAX];
static struct orientation_change_ctx orient_change_work;
struct bt_mesh_thingy52_cb handlers = {
	.rgb_set_handler = rgb_set_handler,
};
static struct bt_mesh_thingy52_srv thingy52_srv =
	BT_MESH_THINGY52_SRV_INIT(&handlers);

static int bind_devices(void)
{
	dev.spkr_pwm = device_get_binding(DT_PROP(DT_NODELABEL(pwm0), label));
	dev.gpio_0 = device_get_binding(DT_PROP(DT_NODELABEL(gpio0), label));
	dev.io_expander =
		device_get_binding(DT_PROP(DT_NODELABEL(sx1509b), label));
	dev.orientation =
		device_get_binding(DT_PROP(DT_NODELABEL(lis2dh12), label));

	if ((dev.gpio_0 == NULL) || (dev.spkr_pwm == NULL) ||
	    (dev.io_expander == NULL) || (dev.orientation == NULL)) {
		return -1;
	}

	return 0;
}

static struct k_delayed_work led_fade_work;

static void led_fade_work_handler(struct k_work *work)
{
	static bool do_fade;

	if (!cur_rgb_msg.color.red && !cur_rgb_msg.color.green &&
	    !cur_rgb_msg.color.blue) {
		do_fade = false;
		printk("LED fade complete\n");
	} else if (cur_rgb_msg.delay == 0xFFFF) {
		cur_rgb_msg.color.red = (cur_rgb_msg.color.red * 90) / 100;
		cur_rgb_msg.color.green = (cur_rgb_msg.color.green * 90) / 100;
		cur_rgb_msg.color.blue = (cur_rgb_msg.color.blue * 90) / 100;

		sx1509b_pwm_pin_set(dev.io_expander, RED_LED,
				    cur_rgb_msg.color.red);
		sx1509b_pwm_pin_set(dev.io_expander, GREEN_LED,
				    cur_rgb_msg.color.green);
		sx1509b_pwm_pin_set(dev.io_expander, BLUE_LED,
				    cur_rgb_msg.color.blue);

		k_delayed_work_submit(&led_fade_work, K_MSEC(20));
	} else if (cur_rgb_msg.delay > 0) {
		if (cur_rgb_msg.delay <= 300 || do_fade) {
			cur_rgb_msg.color.red =
				(cur_rgb_msg.color.red * 80) / 100;
			cur_rgb_msg.color.green =
				(cur_rgb_msg.color.green * 80) / 100;
			cur_rgb_msg.color.blue =
				(cur_rgb_msg.color.blue * 80) / 100;

			sx1509b_pwm_pin_set(dev.io_expander, RED_LED,
					    cur_rgb_msg.color.red);
			sx1509b_pwm_pin_set(dev.io_expander, GREEN_LED,
					    cur_rgb_msg.color.green);
			sx1509b_pwm_pin_set(dev.io_expander, BLUE_LED,
					    cur_rgb_msg.color.blue);

			k_delayed_work_submit(&led_fade_work, K_MSEC(20));
		} else {
			do_fade = true;
			k_delayed_work_submit(&led_fade_work,
					      K_MSEC(cur_rgb_msg.delay - 300));
		}
	}
}

static void set_rgb_led(struct bt_mesh_thingy52_rgb_msg rgb_msg)
{
	sx1509b_pwm_pin_set(dev.io_expander, RED_LED, rgb_msg.color.red);
	sx1509b_pwm_pin_set(dev.io_expander, GREEN_LED, rgb_msg.color.green);
	sx1509b_pwm_pin_set(dev.io_expander, BLUE_LED, rgb_msg.color.blue);

	cur_rgb_msg = rgb_msg;

	k_delayed_work_submit(&led_fade_work, K_NO_WAIT);
}

static void led_init(void)
{
	int err = 0;

	for (int i = GREEN_LED; i <= RED_LED; i++) {
		err |= sx1509b_pwm_pin_configure(dev.io_expander, i);
	}

	k_delayed_work_init(&led_fade_work, led_fade_work_handler);

	if (err) {
		printk("Initializing LEDs failed\n");
	}
}

static void speaker_init(uint32_t pwm_frequency)
{
	int err = 0;
	uint32_t pwm_period = 1000000U / pwm_frequency;

	err |= gpio_pin_configure(dev.gpio_0, SPKR_PWR,
				  GPIO_OUTPUT | GPIO_PULL_UP);
	err |= pwm_pin_set_usec(dev.spkr_pwm, SPKR_PWM, pwm_period,
				pwm_period / 2U, 0);

	if (err) {
		printk("Initializing speaker failed\n");
	}
}

static bool next_rgb_work_idx_get(uint8_t *idx)
{
	*idx = 0;
	uint16_t top_delay = 0xFFFF;
	bool active_work_present = false;

	for (int i = 0; i < ARRAY_SIZE(rgb_work); ++i) {
		int32_t rem = k_delayed_work_remaining_ticks(&rgb_work[i].work);

		if ((rem != 0) && (rgb_work[i].rgb_msg.delay < top_delay)) {
			*idx = i;
			top_delay = rgb_work[i].rgb_msg.delay;
			active_work_present = true;
		}
	}

	return active_work_present;
}

static void rgb_work_output_set(void)
{
	uint8_t idx;
	bool active_work_present = next_rgb_work_idx_get(&idx);

	if (!active_work_present) {
		k_delayed_work_cancel(&orient_change_work.work);
	}

	if (rgb_work[idx].rgb_msg.color.red ||
	    rgb_work[idx].rgb_msg.color.green ||
	    rgb_work[idx].rgb_msg.color.blue) {
		set_rgb_led(rgb_work[idx].rgb_msg);
	}

	if (rgb_work[idx].rgb_msg.speaker_on) {
		gpio_pin_set_raw(dev.gpio_0, SPKR_PWR, 1);
	} else {
		gpio_pin_set_raw(dev.gpio_0, SPKR_PWR, 0);
	}
}

static void rgb_msg_timeout_work_handler(struct k_work *work)
{
	struct rgb_work *rgb = CONTAINER_OF(work, struct rgb_work, work.work);
	uint8_t buffer_idx = rgb - &rgb_work[0];

	if (rgb_work[buffer_idx].rgb_msg.ttl) {
		rgb_work[buffer_idx].rgb_msg.ttl--;
		bt_mesh_thingy52_srv_rgb_set(&thingy52_srv, NULL,
					     &rgb_work[buffer_idx].rgb_msg);
	}

	memset(&rgb_work[buffer_idx].rgb_msg, 0,
	       sizeof(struct bt_mesh_thingy52_rgb_msg));
	rgb_work_output_set();
}

static void orient_change_work_handler(struct k_work *work)
{
	struct orientation_change_ctx *orient_change_work =
		CONTAINER_OF(work, struct orientation_change_ctx, work.work);
	uint8_t orient = orientation_get(dev.orientation);

	if (orient != orient_change_work->orient) {
		for (int i = 0; i < ARRAY_SIZE(rgb_work); ++i) {
			memset(&rgb_work[i].rgb_msg, 0,
			       sizeof(struct bt_mesh_thingy52_rgb_msg));
			k_delayed_work_cancel(&rgb_work[i].work);
			rgb_work_output_set();
		}
	}

	k_delayed_work_submit(&orient_change_work->work, K_MSEC(50));
}

void rgb_set_handler(struct bt_mesh_thingy52_srv *srv,
		     struct bt_mesh_msg_ctx *ctx,
		     struct bt_mesh_thingy52_rgb_msg rgb)
{
	if (rgb.delay == 0xFFFF) {
		set_rgb_led(rgb);
		return;
	}

	orient_change_work.orient = orientation_get(dev.orientation);
	k_delayed_work_submit(&orient_change_work.work, K_MSEC(50));

	for (int i = 0; i < ARRAY_SIZE(rgb_work); ++i) {
		int32_t rem = k_delayed_work_remaining_ticks(&rgb_work[i].work);

		if (rem == 0) {
			memcpy(&rgb_work[i].rgb_msg, &rgb,
			       sizeof(struct bt_mesh_thingy52_rgb_msg));
			k_delayed_work_submit(
				&rgb_work[i].work,
				K_MSEC(rgb_work[i].rgb_msg.delay));
			rgb_work_output_set();

			return;
		}
	}
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

static struct k_delayed_work device_attention_work;

static void device_attention_work_handler(struct k_work *work)
{
	static uint8_t idx;
	const struct bt_mesh_thingy52_rgb_msg colors[2] = {
		{ .color.red = 0, .color.green = 0, .color.blue = 0 },
		{ .color.red = 255, .color.green = 255, .color.blue = 255 },

	};

	set_rgb_led(colors[idx % (sizeof(colors) /
				  sizeof(struct bt_mesh_thingy52_rgb_msg))]);
	idx++;

	k_delayed_work_submit(&device_attention_work, K_MSEC(400));
}

static void attention_on(struct bt_mesh_model *mod)
{
	k_delayed_work_submit(&device_attention_work, K_NO_WAIT);
}

static void attention_off(struct bt_mesh_model *mod)
{
	k_delayed_work_cancel(&device_attention_work);
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
								 &health_pub)),
		     BT_MESH_MODEL_NONE),
	BT_MESH_ELEM(
		2, BT_MESH_MODEL_NONE,
		BT_MESH_MODEL_LIST(BT_MESH_MODEL_THINGY52_SRV(&thingy52_srv))),
};

static const struct bt_mesh_comp comp = {
	.cid = CONFIG_BT_COMPANY_ID,
	.elem = elements,
	.elem_count = ARRAY_SIZE(elements),
};

const struct bt_mesh_comp *model_handler_init(void)
{
	if (bind_devices()) {
		printk("Failure occurred while binding devices\n");
		return &comp;
	}

	k_delayed_work_init(&device_attention_work,
			    device_attention_work_handler);
	k_delayed_work_init(&orient_change_work.work,
			    orient_change_work_handler);
	for (int i = 0; i < ARRAY_SIZE(rgb_work); ++i) {
		k_delayed_work_init(&rgb_work[i].work,
				    rgb_msg_timeout_work_handler);
	}

	led_init();
	speaker_init(SPEAKER_PWM_FREQ);

	return &comp;
}
