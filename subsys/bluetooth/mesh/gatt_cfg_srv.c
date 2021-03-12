#include <string.h>
#include <bluetooth/mesh/gatt_cfg_srv.h>
#include "model_utils.h"
#include "mesh/subnet.h"
#include "mesh/proxy.h"
#include "mesh/proxy_client.h"



static void encode_status(struct net_buf_simple *buf,
			  const struct bt_mesh_gatt_cfg_status *status)
{
	bt_mesh_model_msg_init(buf, BT_MESH_GATT_CFG_OP_STATUS);
	net_buf_simple_add_u8(buf, !!status->present_on_off);

	if (status->remaining_time != 0) {
		net_buf_simple_add_u8(buf, status->target_on_off);
		net_buf_simple_add_u8(
			buf, model_transition_encode(status->remaining_time));
	}
}

static void rsp_status(struct bt_mesh_model *model,
		       struct bt_mesh_msg_ctx *rx_ctx,
		       const struct bt_mesh_gatt_cfg_status *status)
{
	BT_MESH_MODEL_BUF_DEFINE(msg, BT_MESH_GATT_CFG_OP_STATUS,
				 BT_MESH_GATT_CFG_MSG_MAXLEN_STATUS);
	encode_status(&msg, status);

	(void)bt_mesh_model_send(model, rx_ctx, &msg, NULL, NULL);
}

static void handle_get(struct bt_mesh_model *model, struct bt_mesh_msg_ctx *ctx,
		       struct net_buf_simple *buf)
{
	if (buf->len != BT_MESH_GATT_CFG_MSG_LEN_GET) {
		return;
	}

	struct bt_mesh_gatt_cfg_srv *srv = model->user_data;
	struct bt_mesh_gatt_cfg_status status = { 0 };

	srv->handlers->get(srv, ctx, &status);

	rsp_status(model, ctx, &status);
}

static void gatt_cfg_set(struct bt_mesh_model *model, struct bt_mesh_msg_ctx *ctx,
		      struct net_buf_simple *buf, bool ack)
{
	if (buf->len != BT_MESH_GATT_CFG_MSG_MINLEN_SET &&
	    buf->len != BT_MESH_GATT_CFG_MSG_MAXLEN_SET) {
		return;
	}

	struct bt_mesh_gatt_cfg_srv *srv = model->user_data;
	struct bt_mesh_gatt_cfg_status status = { 0 };
	struct bt_mesh_model_transition transition;
	struct bt_mesh_gatt_cfg_set set;

	uint8_t on_off = net_buf_simple_pull_u8(buf);
	uint8_t tid = net_buf_simple_pull_u8(buf);
	printk("\n Incoming Set Message gatt_cfg: %s\n\n", (on_off ? "ON" : "OFF"));
	if (on_off > 1) {
		return;
	}

	set.on_off = on_off;

	if (tid_check_and_update(&srv->prev_transaction, tid, ctx) != 0) {
		/* If this is the same transaction, we don't need to send it
		 * to the app, but we still have to respond with a status.
		 */
		srv->handlers->get(srv, NULL, &status);
		goto respond;
	}

	if (buf->len == 2) {
		model_transition_buf_pull(buf, &transition);
	}

	set.transition = &transition;

	srv->handlers->set(srv, ctx, &set, &status);

	(void)bt_mesh_gatt_cfg_srv_pub(srv, NULL, &status);

respond:
	if (ack) {
		rsp_status(model, ctx, &status);
	}
}

static void handle_set(struct bt_mesh_model *model, struct bt_mesh_msg_ctx *ctx,
		       struct net_buf_simple *buf)
{
	gatt_cfg_set(model, ctx, buf, true);
}

static void handle_set_unack(struct bt_mesh_model *model,
			     struct bt_mesh_msg_ctx *ctx,
			     struct net_buf_simple *buf)
{
	gatt_cfg_set(model, ctx, buf, false);
}

static void handle_adv_set(struct bt_mesh_model *model,
			     struct bt_mesh_msg_ctx *ctx,
			     struct net_buf_simple *buf)
{
	if (buf->len != BT_MESH_GATT_CFG_MSG_LEN_ADV_SET) {
		return;
	}

	struct bt_mesh_gatt_cfg_srv *srv = model->user_data;
	struct bt_mesh_gatt_cfg_status status = { 0 };
	struct bt_mesh_gatt_cfg_adv_set set;

	uint8_t on_off = net_buf_simple_pull_u8(buf);
	if (on_off > 1) {
		return;
	}

	set.on_off = on_off;
	set.net_id = net_buf_simple_pull_u8(buf);

	// srv->handlers->set(srv, ctx, &set, &status);
	const struct bt_mesh_subnet *sub;
	sub = bt_mesh_subnet_get(set.net_id);

	if (set.on_off) {
		bt_mesh_proxy_identity_start(sub);
	} else {
		bt_mesh_proxy_identity_stop(sub);
	}

	(void)bt_mesh_gatt_cfg_srv_pub(srv, NULL, &status);

	rsp_status(model, ctx, &status);

	printk("Setting the advertising value, net_id:%d, onoff: %d\n", set.net_id, set.on_off);
}

static void handle_conn_set(struct bt_mesh_model *model,
			     struct bt_mesh_msg_ctx *ctx,
			     struct net_buf_simple *buf)
{
	if (buf->len != BT_MESH_GATT_CFG_MSG_LEN_CONN_SET) {
		return;
	}

	struct bt_mesh_gatt_cfg_srv *srv = model->user_data;
	struct bt_mesh_gatt_cfg_status status = { 0 };
	struct bt_mesh_gatt_cfg_conn_set set;

	set.addr = net_buf_simple_pull_le16(buf);
	set.net_id = net_buf_simple_pull_u8(buf);

	bt_mesh_proxy_cli_node_id_ctx_set((struct node_id_lookup *)&set);

	printk("CONNECTION\n");
	(void)bt_mesh_gatt_cfg_srv_pub(srv, NULL, &status);

	rsp_status(model, ctx, &status);

}

static void handle_adv_enable(struct bt_mesh_model *model,
			     struct bt_mesh_msg_ctx *ctx,
			     struct net_buf_simple *buf)
{
	if (buf->len != BT_MESH_GATT_CFG_MSG_LEN_ADV_ENABLE) {
		return;
	}

	struct bt_mesh_gatt_cfg_srv *srv = model->user_data;
	struct bt_mesh_gatt_cfg_status status = { 0 };

	uint8_t on_off = net_buf_simple_pull_u8(buf);
	if (on_off > 1) {
		return;
	}
	bt_mesh_proxy_cli_adv_set(on_off);

	(void)bt_mesh_gatt_cfg_srv_pub(srv, NULL, &status);

	rsp_status(model, ctx, &status);

	printk("Turning the advertiser %s\n", (on_off ? "On" : "OFF"));
}

const struct bt_mesh_model_op _bt_mesh_gatt_cfg_srv_op[] = {
	{ BT_MESH_GATT_CFG_OP_GET, BT_MESH_GATT_CFG_MSG_LEN_GET, handle_get },
	{ BT_MESH_GATT_CFG_OP_SET, BT_MESH_GATT_CFG_MSG_MINLEN_SET, handle_set },
	{ BT_MESH_GATT_CFG_OP_SET_UNACK, BT_MESH_GATT_CFG_MSG_MINLEN_SET,
	  handle_set_unack },
	{ BT_MESH_GATT_CFG_OP_ADV_SET, BT_MESH_GATT_CFG_MSG_LEN_ADV_SET,
	  handle_adv_set },
	{ BT_MESH_GATT_CFG_OP_CONN_SET, BT_MESH_GATT_CFG_MSG_LEN_CONN_SET,
	  handle_conn_set },
	{ BT_MESH_GATT_CFG_OP_ADV_ENABLE, BT_MESH_GATT_CFG_MSG_LEN_ADV_ENABLE,
	  handle_adv_enable },
	BT_MESH_MODEL_OP_END,
};

static int update_handler(struct bt_mesh_model *model)
{
	struct bt_mesh_gatt_cfg_srv *srv = model->user_data;
	struct bt_mesh_gatt_cfg_status status = { 0 };

	srv->handlers->get(srv, NULL, &status);
	encode_status(model->pub->msg, &status);

	return 0;
}

static int bt_mesh_gatt_cfg_srv_init(struct bt_mesh_model *model)
{
	struct bt_mesh_gatt_cfg_srv *srv = model->user_data;

	srv->model = model;
	srv->pub.msg = &srv->pub_buf;
	srv->pub.update = update_handler;
	net_buf_simple_init_with_data(&srv->pub_buf, srv->pub_data,
				      sizeof(srv->pub_data));

	return 0;
}

static void bt_mesh_gatt_cfg_srv_reset(struct bt_mesh_model *model)
{
	net_buf_simple_reset(model->pub->msg);
}

const struct bt_mesh_model_cb _bt_mesh_gatt_cfg_srv_cb = {
	.init = bt_mesh_gatt_cfg_srv_init,
	.reset = bt_mesh_gatt_cfg_srv_reset,
};

int32_t bt_mesh_gatt_cfg_srv_pub(struct bt_mesh_gatt_cfg_srv *srv,
			    struct bt_mesh_msg_ctx *ctx,
			    const struct bt_mesh_gatt_cfg_status *status)
{
	BT_MESH_MODEL_BUF_DEFINE(msg, BT_MESH_GATT_CFG_OP_STATUS,
				 BT_MESH_GATT_CFG_MSG_MAXLEN_STATUS);
	encode_status(&msg, status);
	return model_send(srv->model, ctx, &msg);
}
