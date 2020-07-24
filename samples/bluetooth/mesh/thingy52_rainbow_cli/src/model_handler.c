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

static uint8_t color_idx;
static uint8_t cli_idx;

static const struct bt_mesh_whistle_rgb_msg colors[13] = {
        { 0,0,0}, //off
        { 255,0,0}, //red
        { 255,70,0}, //orange
        { 255,255,0}, //light yellow
        { 0,255,90}, //light green
        { 0,255,0}, //green
        { 50,140,80}, //neon teal
        { 0,255,255}, //teal
        { 0,0,255}, //blue
        { 255,0,255}, //light purple
        { 140,30,90}, //light pink
        { 255,0,25}, //pink
        { 255,255,255}, //white
};

static struct bt_mesh_whistle_cli whistle_cli[4] = {
        [0 ... 3] = BT_MESH_WHISTLE_CLI_INIT,
};

static struct device *io_expander;

static struct button_handler button_handler = {
	.cb = button_handler_cb,
};

static void button_and_led_init(void)
{
	int err = 0;

	io_expander = device_get_binding(DT_PROP(DT_NODELABEL(sx1509b), label));
	err |= dk_buttons_init(NULL);
	dk_button_handler_add(&button_handler);
	err |= sx1509b_led_drv_pin_init(io_expander, GREEN_LED);
	err |= sx1509b_led_drv_pin_init(io_expander, BLUE_LED);
	err |= sx1509b_led_drv_pin_init(io_expander, RED_LED);

	if (err || (io_expander == NULL)) {
		printk("GPIO configuration failed\n");
	}

}

static void led_set(struct bt_mesh_whistle_rgb_msg rgb)
{
        sx1509b_set_pwm(io_expander, RED_LED, rgb.red);
        sx1509b_set_pwm(io_expander, GREEN_LED, rgb.green);
        sx1509b_set_pwm(io_expander, BLUE_LED, rgb.blue);
}

static void button_handler_cb(u32_t pressed, u32_t changed)
{
	if ((pressed & changed & BIT(0))) {
		orientation_t orr = thingy52_orientation_get();
		printk("Orientation: %d\n", orr);
		switch (orr) {
		case THINGY_ORIENT_X_UP:
			bt_mesh_whistle_cli_rgb_set(&whistle_cli[cli_idx], NULL,
						    &colors[color_idx]);
			break;

		case THINGY_ORIENT_Y_UP:
		case THINGY_ORIENT_Y_DOWN:
			bt_mesh_whistle_cli_attention_set(&whistle_cli[cli_idx],
							  NULL, false);
			cli_idx = (cli_idx + 1) %
				  (sizeof(whistle_cli) /
				   sizeof(struct bt_mesh_whistle_cli));
			bt_mesh_whistle_cli_attention_set(&whistle_cli[cli_idx],
							  NULL, true);
			break;
		case THINGY_ORIENT_X_DOWN:
			break;
		case THINGY_ORIENT_Z_DOWN:
			color_idx = (color_idx + 1) %
				    (sizeof(colors) /
				     sizeof(struct bt_mesh_whistle_rgb_msg));
                        led_set(colors[color_idx]);
			break;
		case THINGY_ORIENT_Z_UP:
			color_idx = 0;
                        led_set(colors[color_idx]);
			bt_mesh_whistle_cli_attention_set(&whistle_cli[cli_idx],
							  NULL, false);
			break;
		default:
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




static struct bt_mesh_elem elements[] = {
	BT_MESH_ELEM(
		1, BT_MESH_MODEL_LIST(
			BT_MESH_MODEL_CFG_SRV(&cfg_srv),
			BT_MESH_MODEL_HEALTH_SRV(&health_srv, &health_pub),
                        BT_MESH_MODEL_WHISTLE_CLI(&whistle_cli[0])),
		BT_MESH_MODEL_NONE),
	BT_MESH_ELEM(
		2, BT_MESH_MODEL_LIST(BT_MESH_MODEL_WHISTLE_CLI(&whistle_cli[1])),
		BT_MESH_MODEL_NONE),
	BT_MESH_ELEM(
		3, BT_MESH_MODEL_LIST(BT_MESH_MODEL_WHISTLE_CLI(&whistle_cli[2])),
		BT_MESH_MODEL_NONE),
	BT_MESH_ELEM(
		4, BT_MESH_MODEL_LIST(BT_MESH_MODEL_WHISTLE_CLI(&whistle_cli[3])),
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

	return &comp;
}
