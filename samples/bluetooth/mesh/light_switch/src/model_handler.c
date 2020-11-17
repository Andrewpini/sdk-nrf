/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */
/**
 * @file
 * @brief Model handler for the light switch.
 *
 * Instantiates a Generic OnOff Client model for each button on the devkit, as
 * well as the standard Config and Health Server models. Handles all application
 * behavior related to the models.
 */
#include <bluetooth/bluetooth.h>
#include <bluetooth/mesh/models.h>
#include <dk_buttons_and_leds.h>
#include "model_handler.h"


struct bt_mesh_lvl_cli lvl_cli = BT_MESH_LVL_CLI_INIT(NULL);

struct bt_mesh_model_transition sat_transition;
struct bt_mesh_light_hsl_hue_set sat_data = {
	.transition = &sat_transition,
};

void sat_set(struct bt_mesh_light_hsl_sat_srv *srv, struct bt_mesh_msg_ctx *ctx,
	     const struct bt_mesh_light_hsl_hue_set *set,
	     struct bt_mesh_light_hsl_hue_status *rsp)
{
	sat_data.level = set->level;
	sat_transition.delay = set->transition->delay;
	sat_transition.time = set->transition->time;

	printk("SATURATION_SET -- Lvl: %d, Time: %d, Delay: %d\n", set->level, set->transition->time, set->transition->delay);

	rsp->current = sat_data.level;
	rsp->target = sat_data.level;
	rsp->remaining_time = sat_transition.delay + sat_transition.time;
}

void sat_get(struct bt_mesh_light_hsl_sat_srv *srv, struct bt_mesh_msg_ctx *ctx,
	     struct bt_mesh_light_hsl_hue_status *rsp)
{
	rsp->current = sat_data.level;
	rsp->target = sat_data.level;
	rsp->remaining_time = sat_transition.delay + sat_transition.time;
}

struct bt_mesh_light_hsl_sat_srv_handlers sat_handlers = {
	.set = sat_set,
	.get = sat_get,
};

struct bt_mesh_light_hsl_sat_srv sat_srv = BT_MESH_LIGHT_HSL_SAT_SRV_INIT(&sat_handlers);

/* ----------------------------- HUE SERVER ----------------------------- */
struct bt_mesh_model_transition hue_transition;
struct bt_mesh_light_hsl_hue_set hue_data = {
	.transition = &hue_transition,
};

void hue_set(struct bt_mesh_light_hsl_hue_srv *srv, struct bt_mesh_msg_ctx *ctx,
	     const struct bt_mesh_light_hsl_hue_set *set,
	     struct bt_mesh_light_hsl_hue_status *rsp)
{
	hue_data.level = set->level;
	hue_transition.delay = set->transition->delay;
	hue_transition.time = set->transition->time;

	printk("SATURATION_SET -- Lvl: %d, Time: %d, Delay: %d\n", set->level, set->transition->time, set->transition->delay);

	rsp->current = hue_data.level;
	rsp->target = hue_data.level;
	rsp->remaining_time = hue_transition.delay + hue_transition.time;
}

void hue_get(struct bt_mesh_light_hsl_hue_srv *srv, struct bt_mesh_msg_ctx *ctx,
	     struct bt_mesh_light_hsl_hue_status *rsp)
{
	rsp->current = hue_data.level;
	rsp->target = hue_data.level;
	rsp->remaining_time = hue_transition.delay + hue_transition.time;
}

struct bt_mesh_light_hsl_hue_srv_handlers hue_handlers = {
	.set = hue_set,
	.get = hue_get,
};

struct bt_mesh_light_hsl_hue_srv hue_srv = BT_MESH_LIGHT_HSL_HUE_SRV_INIT(&hue_handlers);

/** @def BT_MESH_MODEL_HSL_SAT_SRV
 *
 * @brief HSL Saturation Server model composition data entry.
 *
 * @param[in] _srv Pointer to a @ref bt_mesh_light_hsl_sat_srv instance.
 */
#define BT_MESH_MODEL_HSL_SAT_SRV(_srv)                                        \
	BT_MESH_MODEL_LVL_SRV(&(_srv)->lvl),                                   \
		BT_MESH_MODEL_CB(BT_MESH_MODEL_ID_LIGHT_HSL_SAT_SRV,           \
				 _bt_mesh_light_hsl_sat_srv_op, &(_srv)->pub,        \
				 BT_MESH_MODEL_USER_DATA(                      \
					 struct bt_mesh_light_hsl_sat_srv, _srv),    \
				 &_bt_mesh_light_hsl_sat_srv_cb)

/* ----------------------------------- HSL CLIENT ----------------------------------- */
struct bt_mesh_light_hsl_cli hsl_cli = BT_MESH_LIGHT_HSL_CLI_INIT(NULL);

/* Light switch behavior */

/** Context for a single light switch. */
struct button {
	/** Current light status of the corresponding server. */
	bool status;
	/** Generic OnOff client instance for this switch. */
	struct bt_mesh_onoff_cli client;
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
	struct button *button =
		CONTAINER_OF(cli, struct button, client);
	int index = button - &buttons[0];

	button->status = status->present_on_off;
	dk_set_led(index, status->present_on_off);

	printk("Button %d: Received response: %s\n", index + 1,
	       status->present_on_off ? "on" : "off");
}

static void button_handler_cb(uint32_t pressed, uint32_t changed)
{
	if (pressed & BIT(0)) {
		struct bt_mesh_model_transition transition = {
			.time = 100,
			.delay = 200,
		};
		struct bt_mesh_lvl_set lvl_set = {
			.lvl = 0,
			.new_transaction = true,
			.transition = &transition,
		};
		// bt_mesh_lvl_cli_set(&lvl_cli, NULL, &lvl_set, NULL);
		// lvl_set.lvl = INT16_MAX;
		// bt_mesh_lvl_cli_set(&lvl_cli, NULL, &lvl_set, NULL);
		// lvl_set.lvl = INT16_MIN;
		// bt_mesh_lvl_cli_set(&lvl_cli, NULL, &lvl_set, NULL);
		// lvl_set.lvl = 0;
		// bt_mesh_lvl_cli_set(&lvl_cli, NULL, &lvl_set, NULL);

		struct bt_mesh_lvl_delta_set delta_set = {
			.delta = 1000,
			.new_transaction = true,
			.transition = &transition,
		};
		// lvl_set.lvl = 0;
		// bt_mesh_lvl_cli_set(&lvl_cli, NULL, &lvl_set, NULL);
		// bt_mesh_lvl_cli_delta_set(&lvl_cli, NULL, &delta_set, NULL);

		// lvl_set.lvl = 0;
		// delta_set.delta = INT16_MAX;
		// bt_mesh_lvl_cli_set(&lvl_cli, NULL, &lvl_set, NULL);
		// bt_mesh_lvl_cli_delta_set(&lvl_cli, NULL, &delta_set, NULL);

		// lvl_set.lvl = 0;
		// delta_set.delta = INT16_MIN;
		// bt_mesh_lvl_cli_set(&lvl_cli, NULL, &lvl_set, NULL);
		// bt_mesh_lvl_cli_delta_set(&lvl_cli, NULL, &delta_set, NULL);

		// lvl_set.lvl = 5000;
		// delta_set.delta = INT16_MAX;
		// bt_mesh_lvl_cli_set(&lvl_cli, NULL, &lvl_set, NULL);
		// bt_mesh_lvl_cli_delta_set(&lvl_cli, NULL, &delta_set, NULL);

		// lvl_set.lvl = -5000;
		// delta_set.delta = INT16_MIN;
		// bt_mesh_lvl_cli_set(&lvl_cli, NULL, &lvl_set, NULL);
		// bt_mesh_lvl_cli_delta_set(&lvl_cli, NULL, &delta_set, NULL);

		// lvl_set.lvl = 0;
		// delta_set.new_transaction = false;
		// bt_mesh_lvl_cli_set(&lvl_cli, NULL, &lvl_set, NULL);

		// delta_set.delta = 1000;
		// bt_mesh_lvl_cli_delta_set(&lvl_cli, NULL, &delta_set, NULL);
		// delta_set.delta = -1000;
		// bt_mesh_lvl_cli_delta_set(&lvl_cli, NULL, &delta_set, NULL);

		// struct bt_mesh_lvl_move_set move_set = {
		// 	.delta = 1000,
		// 	.new_transaction = true,
		// 	.transition = &transition,
		// };
		// lvl_set.lvl = 0;
		// bt_mesh_lvl_cli_set(&lvl_cli, NULL, &lvl_set, NULL);
		// bt_mesh_lvl_cli_move_set(&lvl_cli, NULL, &move_set, NULL);

		// move_set.delta = -1000;
		// bt_mesh_lvl_cli_move_set(&lvl_cli, NULL, &move_set, NULL);

		// move_set.delta = 2000;
		// bt_mesh_lvl_cli_move_set(&lvl_cli, NULL, &move_set, NULL);

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

static struct bt_mesh_elem elements[] = {
	BT_MESH_ELEM(1,
		     BT_MESH_MODEL_LIST(
			     BT_MESH_MODEL_CFG_SRV(&cfg_srv),
			     BT_MESH_MODEL_HEALTH_SRV(&health_srv, &health_pub),
			     BT_MESH_MODEL_LIGHT_HSL_CLI(&hsl_cli)),
		     BT_MESH_MODEL_NONE),
	BT_MESH_ELEM(2,
		     BT_MESH_MODEL_LIST(
			     BT_MESH_MODEL_HSL_SAT_SRV(&sat_srv)),
		     BT_MESH_MODEL_NONE),
	BT_MESH_ELEM(3,
		     BT_MESH_MODEL_LIST(
			     BT_MESH_MODEL_LVL_CLI(&lvl_cli)),
		     BT_MESH_MODEL_NONE),
	BT_MESH_ELEM(4,
		     BT_MESH_MODEL_LIST(
			     BT_MESH_MODEL_HSL_HUE_SRV(&hue_srv)),
		     BT_MESH_MODEL_NONE),
};

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
