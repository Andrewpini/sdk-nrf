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
#include <dk_buttons_and_leds.h>
#include <thingy52_orientation_handler.h>
#include "model_handler.h"

#define DEFAULT_RBG_TTL 100
#define DEFAULT_RBG_MIN_SPEED_DELAY 200
#define DEFAULT_RBG_MAX_SPEED_DELAY 2000

struct devices {
	struct device *io_expander;
	struct device *gpio_0;
};

struct whistle_cli_ctrl {
	struct k_delayed_work rgb_pick_work;
	struct k_delayed_work delay_pick_work;
	struct k_delayed_work blink_work;
	struct k_delayed_work auto_send_work;
	struct bt_mesh_whistle_rgb msg_rgb_color;
	uint16_t msg_relay_delay;
	bool msg_speaker_on;
};

static struct devices dev;
static struct whistle_cli_ctrl cli_ctrl = {
	.msg_relay_delay = DEFAULT_RBG_MIN_SPEED_DELAY
};
static struct bt_mesh_whistle_cli whistle_cli = BT_MESH_WHISTLE_CLI_INIT;

static void set_large_rgb_led(struct bt_mesh_whistle_rgb rgb)
{
	sx1509b_led_set_pwm_value(dev.io_expander, RED_LED, rgb.red);
	sx1509b_led_set_pwm_value(dev.io_expander, GREEN_LED, rgb.green);
	sx1509b_led_set_pwm_value(dev.io_expander, BLUE_LED, rgb.blue);
}

static void set_small_rgb_led(bool onoff)
{
	uint8_t color_val = onoff ? 255 : 0;

	for (int i = SMALL_RED_LED; i <= SMALL_BLUE_LED; i++) {
		sx1509b_led_set_pwm_value(dev.io_expander, i, color_val);
	}
}

static void pick_rgb(uint16_t input_val, struct bt_mesh_whistle_rgb *rgb)
{
	uint16_t raw_val = input_val % (6 * 256);
	uint8_t rgb_array[3];

	if (!raw_val) {
		memset(rgb_array, 0, sizeof(rgb_array));
		goto end;
	}

	uint8_t domain_idx = ((raw_val - (raw_val % 256)) / 256);
	uint8_t zero_idx = (((4 + domain_idx) - ((4 + domain_idx) % 2)) / 2) %
			   sizeof(rgb_array);
	uint8_t max_idx = (((1 + domain_idx) - ((1 + domain_idx) % 2)) / 2) %
			  sizeof(rgb_array);
	uint8_t working_idx = (1 + (2 * domain_idx)) % sizeof(rgb_array);

	rgb_array[zero_idx] = 0;
	rgb_array[max_idx] = 255;

	if ((domain_idx + 1) % 2) {
		rgb_array[working_idx] = raw_val % 256;
	} else {
		rgb_array[working_idx] = 255 - (raw_val % 256);
	}

end:
	rgb->red = rgb_array[0];
	rgb->green = rgb_array[1];
	rgb->blue = rgb_array[2];
}

static int bind_devices(void)
{
	dev.gpio_0 = device_get_binding(DT_PROP(DT_NODELABEL(gpio0), label));
	dev.io_expander =
		device_get_binding(DT_PROP(DT_NODELABEL(sx1509b), label));

	if ((dev.gpio_0 == NULL) || (dev.io_expander == NULL)) {
		return -1;
	}

	return 0;
}

static void button_handler_cb(u32_t pressed, u32_t changed)
{
	switch (thingy52_orientation_get()) {
	case THINGY_ORIENT_Y_UP:
		if ((pressed & BIT(0))) {
			cli_ctrl.msg_speaker_on = !cli_ctrl.msg_speaker_on;
			set_small_rgb_led(cli_ctrl.msg_speaker_on);
		}
		break;
	case THINGY_ORIENT_Y_DOWN:
		if ((pressed & BIT(0))) {
			static bool onoff = true;

			if (onoff) {
				k_delayed_work_submit(&cli_ctrl.auto_send_work,
						      K_NO_WAIT);
			} else {
				k_delayed_work_cancel(&cli_ctrl.auto_send_work);
			}

			onoff = !onoff;
		}
		break;
	case THINGY_ORIENT_X_DOWN:
		if ((pressed & BIT(0))) {
			k_delayed_work_submit(&cli_ctrl.rgb_pick_work,
					      K_NO_WAIT);
		} else if ((!pressed & BIT(0))) {
			k_delayed_work_cancel(&cli_ctrl.rgb_pick_work);
		}
		break;
	case THINGY_ORIENT_Z_DOWN:
		if ((pressed & BIT(0))) {
			cli_ctrl.msg_relay_delay = DEFAULT_RBG_MIN_SPEED_DELAY;
			k_delayed_work_submit(&cli_ctrl.blink_work, K_NO_WAIT);
			k_delayed_work_submit(&cli_ctrl.delay_pick_work,
					      K_NO_WAIT);

		} else if ((!pressed & BIT(0))) {
			k_delayed_work_cancel(&cli_ctrl.blink_work);
			k_delayed_work_cancel(&cli_ctrl.delay_pick_work);
			set_large_rgb_led(cli_ctrl.msg_rgb_color);

			struct bt_mesh_whistle_rgb_msg msg;
			msg.ttl = DEFAULT_RBG_TTL;
			msg.delay = cli_ctrl.msg_relay_delay;
			memcpy(&msg.color, &cli_ctrl.msg_rgb_color,
			       sizeof(msg.color));
			msg.speaker_on = cli_ctrl.msg_speaker_on;

			bt_mesh_whistle_cli_rgb_set(&whistle_cli, NULL, &msg);
		}
		break;
	default:
		return;
	}
}

static struct button_handler button_handler = {
	.cb = button_handler_cb,
};

static void button_and_led_init(void)
{
	int err = 0;

	err |= dk_buttons_init(NULL);
	dk_button_handler_add(&button_handler);

	for (int i = GREEN_LED; i <= RED_LED; i++) {
		err |= sx1509b_pin_configure(dev.io_expander, i, SX1509B_PWM);
	}
	for (int i = SMALL_RED_LED; i <= SMALL_BLUE_LED; i++) {
		err |= sx1509b_pin_configure(dev.io_expander, i, SX1509B_PWM);
	}

	if (err) {
		printk("Initializing buttons and leds failed.\n");
	}
}

static void auto_send_work_handler(struct k_work *work)
{
	struct bt_mesh_whistle_rgb rand_rgb;
	struct bt_mesh_whistle_rgb_msg msg;

	pick_rgb((uint16_t)sys_rand32_get(), &rand_rgb);
	msg.ttl = DEFAULT_RBG_TTL;
	msg.delay = DEFAULT_RBG_MIN_SPEED_DELAY +
		    (sys_rand32_get() % (DEFAULT_RBG_MAX_SPEED_DELAY -
					 DEFAULT_RBG_MIN_SPEED_DELAY));
	memcpy(&msg.color, &rand_rgb, sizeof(msg.color));
	msg.speaker_on = cli_ctrl.msg_speaker_on;

	bt_mesh_whistle_cli_rgb_set(&whistle_cli, NULL, &msg);

	k_delayed_work_submit(&cli_ctrl.auto_send_work,
			      K_MSEC(DEFAULT_RBG_MAX_SPEED_DELAY));
}

static void rgb_picker_work_handler(struct k_work *work)
{
	struct whistle_cli_ctrl *rgb =
		CONTAINER_OF(work, struct whistle_cli_ctrl, rgb_pick_work.work);
	static uint16_t val;

	pick_rgb(val, &rgb->msg_rgb_color);
	set_large_rgb_led(rgb->msg_rgb_color);
	val += 16;
	k_delayed_work_submit(&cli_ctrl.rgb_pick_work,
			      K_MSEC(DEFAULT_RBG_MIN_SPEED_DELAY));
}

static void speed_picker_work_handler(struct k_work *work)
{
	struct whistle_cli_ctrl *rgb = CONTAINER_OF(
		work, struct whistle_cli_ctrl, delay_pick_work.work);

	if (rgb->msg_relay_delay < DEFAULT_RBG_MAX_SPEED_DELAY) {
		rgb->msg_relay_delay += 50;
	}

	k_delayed_work_submit(&cli_ctrl.delay_pick_work,
			      K_MSEC(DEFAULT_RBG_MIN_SPEED_DELAY));
}

static void blink_work_handler(struct k_work *work)
{
	struct whistle_cli_ctrl *rgb =
		CONTAINER_OF(work, struct whistle_cli_ctrl, blink_work.work);
	static bool onoff;

	if (onoff) {
		set_large_rgb_led(rgb->msg_rgb_color);
	} else {
		struct bt_mesh_whistle_rgb rgb_off = { 0, 0, 0 };
		set_large_rgb_led(rgb_off);
	}

	onoff = !onoff;
	k_delayed_work_submit(&cli_ctrl.blink_work,
			      K_MSEC(rgb->msg_relay_delay / 2));
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

static void attention_on(struct bt_mesh_model *mod)
{
	for (int i = GREEN_LED; i <= RED_LED; i++) {
		sx1509b_led_set_pwm_value(dev.io_expander, i, 255);
	}
}

static void attention_off(struct bt_mesh_model *mod)
{
	for (int i = GREEN_LED; i <= RED_LED; i++) {
		sx1509b_led_set_pwm_value(dev.io_expander, i, 0);
	}
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
		BT_MESH_MODEL_LIST(BT_MESH_MODEL_WHISTLE_CLI(&whistle_cli))),
};

static const struct bt_mesh_comp comp = {
	.cid = CONFIG_BT_COMPANY_ID,
	.elem = elements,
	.elem_count = ARRAY_SIZE(elements),
};

const struct bt_mesh_comp *model_handler_init(void)
{
	if (bind_devices()) {
		printk("Failure occured while binding devices\n");
		return &comp;
	}

	button_and_led_init();
	thingy52_orientation_handler_init();
	k_delayed_work_init(&cli_ctrl.rgb_pick_work, rgb_picker_work_handler);
	k_delayed_work_init(&cli_ctrl.delay_pick_work,
			    speed_picker_work_handler);
	k_delayed_work_init(&cli_ctrl.blink_work, blink_work_handler);
	k_delayed_work_init(&cli_ctrl.auto_send_work, auto_send_work_handler);

	return &comp;
}
