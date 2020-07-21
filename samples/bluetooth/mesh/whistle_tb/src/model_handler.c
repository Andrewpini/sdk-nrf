
/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <bluetooth/bluetooth.h>
#include <bluetooth/mesh/models.h>
#include <dk_buttons_and_leds.h>
#include "model_handler.h"


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

/* Set up a repeating delayed work to blink the DK's LEDs when attention is
 * requested.
 */
static struct k_delayed_work attention_blink_work;

static void attention_blink(struct k_work *work)
{
	static int idx;
	const uint8_t pattern[] = {
		BIT(0) | BIT(1),
		BIT(1) | BIT(2),
		BIT(2) | BIT(3),
		BIT(3) | BIT(0),
	};
	dk_set_leds(pattern[idx++ % ARRAY_SIZE(pattern)]);
	k_delayed_work_submit(&attention_blink_work, K_MSEC(30));
}

static void attention_on(struct bt_mesh_model *mod)
{
	k_delayed_work_submit(&attention_blink_work, K_NO_WAIT);
}

static void attention_off(struct bt_mesh_model *mod)
{
	k_delayed_work_cancel(&attention_blink_work);
	dk_set_leds(DK_NO_LEDS_MSK);
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
                                struct bt_mesh_msg_ctx *ctx,
                                uint16_t lvl)
{
        printk("Current lvl: %d\n", lvl);
}

void rgb_set_handler(struct bt_mesh_whistle_srv *srv,
                                struct bt_mesh_msg_ctx *ctx,
                                struct bt_mesh_whistle_rgb_msg rgb)
{
        printk("Current rgb: R:%d, G:%d, B:%d\n", rgb.red, rgb.green, rgb.blue);
}

void attention_set_handler(struct bt_mesh_whistle_srv *srv,
                                struct bt_mesh_msg_ctx *ctx,
                                bool onoff)
{
        printk("Attention:%d\n", onoff);
}

struct bt_mesh_whistle_cb handlers = {
	.attention_set_handler = attention_set_handler,
	.lvl_set_handler = lvl_set_handler,
	.rgb_set_handler = rgb_set_handler,
};

static struct bt_mesh_whistle_cli whistle_cli = BT_MESH_WHISTLE_CLI_INIT;
static struct bt_mesh_whistle_srv whistle_srv = BT_MESH_WHISTLE_SRV_INIT(&handlers);

static struct bt_mesh_elem elements[] = {
	BT_MESH_ELEM(
		1, BT_MESH_MODEL_LIST(
			BT_MESH_MODEL_CFG_SRV(&cfg_srv),
			BT_MESH_MODEL_HEALTH_SRV(&health_srv, &health_pub)),
		BT_MESH_MODEL_NONE),
	BT_MESH_ELEM(
		2, BT_MESH_MODEL_LIST(BT_MESH_MODEL_WHISTLE_CLI(&whistle_cli)),
		BT_MESH_MODEL_NONE),
	BT_MESH_ELEM(
		3, BT_MESH_MODEL_LIST(BT_MESH_MODEL_WHISTLE_SRV(&whistle_srv)),
		BT_MESH_MODEL_NONE),
};

static uint8_t inc = 0;

static void menu_inc(void){
	if (inc < 5)
	{
		inc++;
	} else {
		inc = 0;
	}
	printk("\nON MENU NR: %d\n",inc);
}

static void button_handler_cb(uint32_t pressed, uint32_t changed)
{
	if (!bt_mesh_is_provisioned()) {
		return;
	}
		if (pressed & changed & BIT(0)) {
			menu_inc();
		} else {
			switch (inc) {
			case 0: {
				if (pressed & changed & BIT(1)) {
					printk("Button 2\n");
					int err = bt_mesh_whistle_cli_lvl_set(
						&whistle_cli, NULL, UINT16_MAX);
					printk("Err: %d\n", err);
				}
				if (pressed & changed & BIT(2)) {
					printk("Button 3\n");
					struct bt_mesh_whistle_rgb_msg rgb = {
						.red = 4, .green = 8, .blue = 12
					};
					int err = bt_mesh_whistle_cli_rgb_set(
						&whistle_cli, NULL, &rgb);
					printk("Err: %d\n", err);
				}
				if (pressed & changed & BIT(3)) {
					printk("Button 4\n");
                                        static bool onoff = true;
					bt_mesh_whistle_cli_attention_set(
						&whistle_cli, NULL, onoff);
                                        onoff = !onoff;
				}
			} break;

			case 1:
				if (pressed & changed & BIT(1)) {
					printk("Button 2\n");
				}
				if (pressed & changed & BIT(2)) {
					printk("Button 3\n");
				}
				if (pressed & changed & BIT(3)) {
					printk("Button 4\n");
				}
			break;

			case 2:
				if (pressed & changed & BIT(1)) {
					printk("Button 2\n");
				}
				if (pressed & changed & BIT(2)) {
					printk("Button 3\n");
				}
				if (pressed & changed & BIT(3)) {
					printk("Button 4\n");
				}
			break;

			case 3:
				if (pressed & changed & BIT(1)) {
					printk("Button 2\n");
				}
				if (pressed & changed & BIT(2)) {
					printk("Button 3\n");
				}
				if (pressed & changed & BIT(3)) {
					printk("Button 4\n");
				}
			break;

			case 4:
				if (pressed & changed & BIT(1)) {
					printk("Button 2\n");
				}
				if (pressed & changed & BIT(2)) {
					printk("Button 3\n");
				}
				if (pressed & changed & BIT(3)) {
					printk("Button 4\n");
				}
			break;

			case 5:
				if (pressed & changed & BIT(1)) {
					printk("Button 2\n");
				}
				if (pressed & changed & BIT(2)) {
					printk("Button 3\n");
				}
				if (pressed & changed & BIT(3)) {
					printk("Button 4\n");
				}
			break;


			default:
				break;
			}
		}

}

static const struct bt_mesh_comp comp = {
	.cid = CONFIG_BT_COMPANY_ID,
	.elem = elements,
	.elem_count = ARRAY_SIZE(elements),
};


const struct bt_mesh_comp *model_handler_init(void)
{
	static struct button_handler button_handler = {
		.cb = button_handler_cb,
	};

	dk_button_handler_add(&button_handler);
	k_delayed_work_init(&attention_blink_work, attention_blink);


	return &comp;
}




