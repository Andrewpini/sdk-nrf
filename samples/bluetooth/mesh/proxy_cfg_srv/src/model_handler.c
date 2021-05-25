#include <bluetooth/bluetooth.h>
#include <bluetooth/mesh/models.h>
#include <dk_buttons_and_leds.h>
#include "model_handler.h"

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


static void set(struct bt_mesh_gpc_srv *srv, struct bt_mesh_msg_ctx *ctx,
		const struct bt_mesh_gpc_set *set,
		struct bt_mesh_gpc_status *rsp)
{
	printk("Set\n");
}

static void get(struct bt_mesh_gpc_srv *srv, struct bt_mesh_msg_ctx *ctx,
		struct bt_mesh_gpc_status *rsp)
{
	printk("Get\n");
}

struct bt_mesh_gpc_srv_handlers gpc_handler = {
	.set = set,
	.get = get,
};

static struct bt_mesh_gpc_srv gpc_srv =
	BT_MESH_GPC_SRV_INIT(&gpc_handler);

BT_MESH_HEALTH_PUB_DEFINE(health_pub, 0);

static struct bt_mesh_elem elements[] = {


	BT_MESH_ELEM(
		1, BT_MESH_MODEL_LIST(
			BT_MESH_MODEL_CFG_SRV,
			BT_MESH_MODEL_HEALTH_SRV(&health_srv, &health_pub),
			BT_MESH_MODEL_GPC_SRV(&gpc_srv)),
					     BT_MESH_MODEL_NONE),
		// BT_MESH_MODEL_LIST(BT_MESH_MODEL_GPC_SRV(&gpc_srv))),


};

static const struct bt_mesh_comp comp = {
	.cid = CONFIG_BT_COMPANY_ID,
	.elem = elements,
	.elem_count = ARRAY_SIZE(elements),
};

const struct bt_mesh_comp *model_handler_init(void)
{
	k_delayed_work_init(&attention_blink_work, attention_blink);

	return &comp;
}
