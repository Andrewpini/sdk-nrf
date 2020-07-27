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

static void button_handler_cb(u32_t pressed, u32_t changed);
struct button {
	/** Current light status of the corresponding server. */
	bool status;
	/** Generic OnOff client instance for this switch. */
	struct bt_mesh_onoff_cli client;
};

struct rgb {
        uint8_t red;
        uint8_t green;
        uint8_t blue;
};

static const struct rgb colors[13] = {
        { 0,0,0}, //off
        { 255,0,0}, //red
        { 255,70,0}, //orange
        { 255,255,0}, //light yellow
        { 0,255,80}, //light green
        { 0,255,0}, //green
        { 25,100,55}, //neon teal
        { 0,255,255}, //teal
        { 0,0,255}, //blue
        { 255,0,255}, //light purple
        { 80,10,45}, //light pink
        { 255,0,25}, //pink
        { 255,255,255}, //white
};


static void status_handler(struct bt_mesh_onoff_cli *cli,
			   struct bt_mesh_msg_ctx *ctx,
			   const struct bt_mesh_onoff_status *status);

static struct button buttons[4] = {
	[0 ... 3] = { .client = BT_MESH_ONOFF_CLI_INIT(&status_handler) },
};

static void status_handler(struct bt_mesh_onoff_cli *cli,
			   struct bt_mesh_msg_ctx *ctx,
			   const struct bt_mesh_onoff_status *status)
{
	struct button *button = CONTAINER_OF(cli, struct button, client);
	int index = button - &buttons[0];

	button->status = status->present_on_off;

	printk("Button %d: Received response: %s\n", index + 1,
	       status->present_on_off ? "on" : "off");
}

static void send_onoff(uint8_t idx)
{
	struct bt_mesh_onoff_set set = {
		.on_off = !buttons[idx].status,
	};
	int err;

	/* As we can't know how many nodes are in a group, it doesn't
		 * make sense to send acknowledged messages to group addresses -
		 * we won't be able to make use of the responses anyway.
		 */
	if (bt_mesh_model_pub_is_unicast(buttons[idx].client.model)) {
		err = bt_mesh_onoff_cli_set(&buttons[idx].client, NULL, &set,
					    NULL);
	} else {
		err = bt_mesh_onoff_cli_set_unack(&buttons[idx].client, NULL,
						  &set);
		if (!err) {
			/* There'll be no response status for the
				 * unacked message. Set the state immediately.
				 */
			buttons[idx].status = set.on_off;
		}
	}

	if (err) {
		printk("OnOff %d set failed: %d\n", idx + 1, err);
	}
}

static struct button_handler button_handler = {
	.cb = button_handler_cb,
};


static struct device *io_expander;

static void button_and_led_init(void)
{
	int err = 0;

	io_expander = device_get_binding(DT_PROP(DT_NODELABEL(sx1509b), label));
	err |= dk_buttons_init(NULL);
	dk_button_handler_add(&button_handler);
        // err |= sx1509b_led_drv_init(io_expander);
        err |= sx1509b_led_drv_pin_init(io_expander, GREEN_LED);
        err |= sx1509b_led_drv_pin_init(io_expander, BLUE_LED);
        err |= sx1509b_led_drv_pin_init(io_expander, RED_LED);

	if (err || (io_expander == NULL)) {
		printk("GPIO configuration failed\n");
	}
}

static uint8_t color[3];
static uint8_t select = 0;

static void button_handler_cb(u32_t pressed, u32_t changed)
{
	if ((pressed & changed & BIT(0))) {
		orientation_t orr = thingy52_orientation_get();
		printk("Orientation: %d\n", orr);
		uint8_t idx;
		switch (orr) {
		case THINGY_ORIENT_X_UP:
			if (color[select] == 255) {
				color[select] = 0;
			} else {
				color[select] += 5;
			}
			break;
		case THINGY_ORIENT_X_DOWN:
			if (color[select] == 0) {
				color[select] = 255;
			} else {
				color[select] -= 5;
			}
			break;
		case THINGY_ORIENT_Y_DOWN:
		case THINGY_ORIENT_Y_UP:
                        for (size_t i = 0; i < 3; i++)
                        {
                                color[i] = 0;
                        }

			break;
		case THINGY_ORIENT_Z_DOWN:
		case THINGY_ORIENT_Z_UP:
			select++;
			if (select == 3) {
				select = 0;
			}
                        printk("Idx: %d\n", select);

			break;
		default:
			return;
		}
                printk("Red: %d, Green: %d, Blue: %d\n", color[0], color[1], color[2]);
                sx1509b_set_pwm(io_expander, RED_LED, color[0]);
                sx1509b_set_pwm(io_expander, GREEN_LED, color[1]);
                sx1509b_set_pwm(io_expander, BLUE_LED, color[2]);
	}
}




static struct k_delayed_work orentiation_led_work;

static void leds_off(void)
{
	gpio_pin_set_raw(io_expander, BLUE_LED, 1);
	gpio_pin_set_raw(io_expander, GREEN_LED, 1);
	gpio_pin_set_raw(io_expander, RED_LED, 1);
}

static void leds_on(bool r, bool g, bool b)
{
	gpio_pin_set_raw(io_expander, RED_LED, r);
	gpio_pin_set_raw(io_expander, GREEN_LED, g);
	gpio_pin_set_raw(io_expander, BLUE_LED, b);
}

static void orentiation_led(struct k_work *work)
{
        static uint8_t idx;
        printk("red: %d\n", colors[idx % 13].red);
        sx1509b_set_pwm(io_expander, RED_LED, colors[idx % 13].red);
        sx1509b_set_pwm(io_expander, GREEN_LED, colors[idx % 13].green);
        sx1509b_set_pwm(io_expander, BLUE_LED, colors[idx % 13].blue);
        idx++;
	// orientation_t orr = thingy52_orientation_get();

	// switch (orr) {
	// case THINGY_ORIENT_X_UP:
	// 	if (buttons[0].status) {
	// 		leds_on(0, 1, 1);
	// 	} else {
	// 		leds_off();
	// 	}
	// 	break;
	// case THINGY_ORIENT_X_DOWN:
	// 	if (buttons[2].status) {
	// 		leds_on(1, 0, 1);
	// 	} else {
	// 		leds_off();
	// 	}
	// 	break;
	// case THINGY_ORIENT_Y_UP:
	// 	if (buttons[1].status) {
	// 		leds_on(1, 1, 0);
	// 	} else {
	// 		leds_off();
	// 	}
	// 	break;
	// case THINGY_ORIENT_Y_DOWN:
	// 	if (buttons[3].status) {
	// 		leds_on(0, 1, 0);
	// 	} else {
	// 		leds_off();
	// 	}
	// 	break;
	// default:
	// 	leds_off();
	// 	break;
	// }
	k_delayed_work_submit(&orentiation_led_work, K_MSEC(700));
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
		     BT_MESH_MODEL_LIST(
			     BT_MESH_MODEL_CFG_SRV(&cfg_srv),
			     BT_MESH_MODEL_HEALTH_SRV(&health_srv, &health_pub),
			     BT_MESH_MODEL_ONOFF_CLI(&buttons[0].client)),
		     BT_MESH_MODEL_NONE),
	BT_MESH_ELEM(
		2,
		BT_MESH_MODEL_LIST(BT_MESH_MODEL_ONOFF_CLI(&buttons[1].client)),
		BT_MESH_MODEL_NONE),
	BT_MESH_ELEM(
		3,
		BT_MESH_MODEL_LIST(BT_MESH_MODEL_ONOFF_CLI(&buttons[2].client)),
		BT_MESH_MODEL_NONE),
	BT_MESH_ELEM(
		4,
		BT_MESH_MODEL_LIST(BT_MESH_MODEL_ONOFF_CLI(&buttons[3].client)),
		BT_MESH_MODEL_NONE),
};

static const struct bt_mesh_comp comp = {
	.cid = CONFIG_BT_COMPANY_ID,
	.elem = elements,
	.elem_count = ARRAY_SIZE(elements),
};

const struct bt_mesh_comp *model_handler_init(void)
{
	button_and_led_init();
	thingy52_orientation_handler_init();
	k_delayed_work_init(&attention_blink_work, attention_blink);
	k_delayed_work_init(&orentiation_led_work, orentiation_led);
	k_delayed_work_submit(&orentiation_led_work, K_NO_WAIT);

	return &comp;
}
