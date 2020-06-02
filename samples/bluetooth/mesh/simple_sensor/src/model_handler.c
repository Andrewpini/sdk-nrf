/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <bluetooth/bluetooth.h>
#include <bluetooth/mesh/models.h>
#include <dk_buttons_and_leds.h>
#include "model_handler.h"

#define TEMP_NODE DT_INST(0, nordic_nrf_temp)

#if DT_NODE_HAS_STATUS(TEMP_NODE, okay)
#define TEMP_NODE_LABEL DT_LABEL(TEMP_NODE)
#else
#define TEMP_NODE_LABEL "<none>"
#endif
static struct device *dev;

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
	const u8_t pattern[] = {
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

void sensor_cli_data_cb(struct bt_mesh_sensor_cli *cli, struct bt_mesh_msg_ctx *ctx, const struct bt_mesh_sensor_type *sensor, const struct sensor_value *value)
{
	printk("Received temperature: %d.%06d;\n", value->val1, value->val2);
}

const struct bt_mesh_sensor_cli_handlers bt_mesh_sensor_cli_handlers = {
	.data = sensor_cli_data_cb,
};

static struct bt_mesh_sensor_cli sensor_cli = BT_MESH_SENSOR_CLI_INIT(&bt_mesh_sensor_cli_handlers);

static int temp_get(struct bt_mesh_sensor *sensor,
                    struct bt_mesh_msg_ctx *ctx,
                    struct sensor_value *rsp)
{
	sensor_sample_fetch(dev);
	return sensor_channel_get(dev, SENSOR_CHAN_DIE_TEMP, rsp);
}

struct bt_mesh_sensor temp_sensor = {
    .type = &bt_mesh_sensor_present_dev_op_temp,
    .get = temp_get,
};

static struct bt_mesh_sensor* const sensors[] = {
    &temp_sensor,
};

static struct bt_mesh_sensor_srv sensor_srv = BT_MESH_SENSOR_SRV_INIT(sensors, ARRAY_SIZE(sensors));

static struct bt_mesh_elem elements[] = {
	BT_MESH_ELEM(
		1, BT_MESH_MODEL_LIST(
			BT_MESH_MODEL_CFG_SRV(&cfg_srv),
			BT_MESH_MODEL_HEALTH_SRV(&health_srv, &health_pub),
			BT_MESH_MODEL_SENSOR_SRV(&sensor_srv),
			BT_MESH_MODEL_SENSOR_CLI(&sensor_cli)),
		BT_MESH_MODEL_NONE),
};

static const struct bt_mesh_comp comp = {
	.cid = CONFIG_BT_COMPANY_ID,
	.elem = elements,
	.elem_count = ARRAY_SIZE(elements),
};

const struct bt_mesh_comp *model_handler_init(void)
{
	dev = device_get_binding(TEMP_NODE_LABEL);

	if (dev == NULL) {
		printk("No device \"%s\" found; did initialization fail?\n",
		       TEMP_NODE_LABEL);
	} else {
		printk("Found device \"%s\"\n", TEMP_NODE_LABEL);
	}

	k_delayed_work_init(&attention_blink_work, attention_blink);
	return &comp;
}
