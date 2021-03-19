
#include <string.h>
#include <bluetooth/mesh/gatt_cfg_cli.h>
#include "model_utils.h"

static int decode_status(struct net_buf_simple *buf,
			  struct bt_mesh_gatt_cfg_status *status)
{
	uint8_t on_off;

	on_off = net_buf_simple_pull_u8(buf);
	if (on_off > 1) {
		return -EINVAL;
	}
	status->present_on_off = on_off;

	if (buf->len == 2) {
		on_off = net_buf_simple_pull_u8(buf);
		if (on_off > 1) {
			return -EINVAL;
		}
		status->target_on_off = on_off;
		status->remaining_time =
			model_transition_decode(net_buf_simple_pull_u8(buf));
	} else {
		status->target_on_off = status->present_on_off;
		status->remaining_time = 0;
	}

	return 0;
}

static void handle_status(struct bt_mesh_model *model,
			  struct bt_mesh_msg_ctx *ctx,
			  struct net_buf_simple *buf)
{
	if (buf->len != BT_MESH_GATT_CFG_MSG_MINLEN_STATUS &&
	    buf->len != BT_MESH_GATT_CFG_MSG_MAXLEN_STATUS) {
		return;
	}

	struct bt_mesh_gatt_cfg_cli *cli = model->user_data;
	struct bt_mesh_gatt_cfg_status status;

	if (decode_status(buf, &status)) {
		return;
	}

	if (model_ack_match(&cli->ack_ctx, BT_MESH_GATT_CFG_OP_STATUS, ctx)) {
		struct bt_mesh_gatt_cfg_status *rsp =
			(struct bt_mesh_gatt_cfg_status *)cli->ack_ctx.user_data;

		*rsp = status;
		model_ack_rx(&cli->ack_ctx);
	}

	if (cli->status_handler) {
		cli->status_handler(cli, ctx, &status);
	}
}

static void handle_fetch_rsp(struct bt_mesh_model *model,
			  struct bt_mesh_msg_ctx *ctx,
			  struct net_buf_simple *buf)
{
	if (buf->len < BT_MESH_GATT_CFG_MSG_MINLEN_FETCH_RSP ||
	    buf->len > BT_MESH_GATT_CFG_MSG_MAXLEN_FETCH_RSP) {
		return;
	}

	struct bt_mesh_gatt_cfg_cli *cli = model->user_data;
	struct link_data_entry entry = {0};

	// uint16_t root_addr = net_buf_simple_pull_le16(buf);
	// printk("Root Addr: %d\n", root_addr);
	// entry.src = root_addr;
	entry.src = net_buf_simple_pull_le16(buf);

	// uint8_t itr = buf->len / sizeof(struct link_data);
	entry.entry_cnt = buf->len / sizeof(struct link_data);
	for (size_t i = 0; i < entry.entry_cnt; i++)
	{
		// uint16_t addr = net_buf_simple_pull_le16(buf);
		// uint8_t cnt = net_buf_simple_pull_u8(buf);
		// printk("\tAddr: %d, Cnt: %d\n", addr, cnt);
		// entry.data[i].root_addr = addr;
		// entry.data[i].received_cnt = cnt;
		entry.data[i].root_addr = net_buf_simple_pull_le16(buf);
		entry.data[i].received_cnt = net_buf_simple_pull_u8(buf);
	}

	if (model_ack_match(&cli->ack_ctx, BT_MESH_GATT_CFG_OP_LINK_FETCH_RSP, ctx)) {
		struct link_data_entry *rsp =
			(struct link_data_entry *)cli->ack_ctx.user_data;

		*rsp = entry;
		model_ack_rx(&cli->ack_ctx);
	}
}

const struct bt_mesh_model_op _bt_mesh_gatt_cfg_cli_op[] = {
	{ BT_MESH_GATT_CFG_OP_STATUS, BT_MESH_GATT_CFG_MSG_MINLEN_STATUS,
	  handle_status },
	{ BT_MESH_GATT_CFG_OP_LINK_FETCH_RSP, BT_MESH_GATT_CFG_MSG_MINLEN_FETCH_RSP,
	  handle_fetch_rsp },
	BT_MESH_MODEL_OP_END,
};

static int bt_mesh_gatt_cfg_cli_init(struct bt_mesh_model *model)
{
	struct bt_mesh_gatt_cfg_cli *cli = model->user_data;

	cli->model = model;
	cli->pub.msg = &cli->pub_buf;
	net_buf_simple_init_with_data(&cli->pub_buf, cli->pub_data,
				      sizeof(cli->pub_data));
	model_ack_init(&cli->ack_ctx);

	return 0;
}

static void bt_mesh_gatt_cfg_cli_reset(struct bt_mesh_model *model)
{
	struct bt_mesh_gatt_cfg_cli *cli = model->user_data;

	net_buf_simple_reset(cli->pub.msg);
	model_ack_reset(&cli->ack_ctx);
}

const struct bt_mesh_model_cb _bt_mesh_gatt_cfg_cli_cb = {
	.init = bt_mesh_gatt_cfg_cli_init,
	.reset = bt_mesh_gatt_cfg_cli_reset,
};

int bt_mesh_gatt_cfg_cli_adv_set(struct bt_mesh_gatt_cfg_cli *cli,
			  struct bt_mesh_msg_ctx *ctx,
			  struct bt_mesh_gatt_cfg_adv_set *set,
			  struct bt_mesh_gatt_cfg_status *rsp)
{
	BT_MESH_MODEL_BUF_DEFINE(msg, BT_MESH_GATT_CFG_OP_ADV_SET,
				 BT_MESH_GATT_CFG_MSG_LEN_ADV_SET);
	bt_mesh_model_msg_init(&msg, BT_MESH_GATT_CFG_OP_ADV_SET);

	net_buf_simple_add_u8(&msg, set->on_off);
	net_buf_simple_add_u8(&msg, set->net_id);

	return model_ackd_send(cli->model, ctx, &msg,
			       rsp ? &cli->ack_ctx : NULL,
			       BT_MESH_GATT_CFG_OP_STATUS, rsp);
}

int bt_mesh_gatt_cfg_cli_conn_set(struct bt_mesh_gatt_cfg_cli *cli,
			  struct bt_mesh_msg_ctx *ctx,
			  struct bt_mesh_gatt_cfg_conn_set *set,
			  struct bt_mesh_gatt_cfg_status *rsp)
{
	BT_MESH_MODEL_BUF_DEFINE(msg, BT_MESH_GATT_CFG_OP_CONN_SET,
				 BT_MESH_GATT_CFG_MSG_LEN_CONN_SET);
	bt_mesh_model_msg_init(&msg, BT_MESH_GATT_CFG_OP_CONN_SET);

	net_buf_simple_add_le16(&msg, set->addr);
	net_buf_simple_add_u8(&msg, set->net_id);

	return model_ackd_send(cli->model, ctx, &msg,
			       rsp ? &cli->ack_ctx : NULL,
			       BT_MESH_GATT_CFG_OP_STATUS, rsp);
}

int bt_mesh_gatt_cfg_cli_adv_enable(struct bt_mesh_gatt_cfg_cli *cli,
			  struct bt_mesh_msg_ctx *ctx,
			  bool onoff,
			  struct bt_mesh_gatt_cfg_status *rsp)
{
	BT_MESH_MODEL_BUF_DEFINE(msg, BT_MESH_GATT_CFG_OP_ADV_ENABLE,
				 BT_MESH_GATT_CFG_MSG_LEN_ADV_ENABLE);
	bt_mesh_model_msg_init(&msg, BT_MESH_GATT_CFG_OP_ADV_ENABLE);

	net_buf_simple_add_u8(&msg, onoff);

	return model_ackd_send(cli->model, ctx, &msg,
			       rsp ? &cli->ack_ctx : NULL,
			       BT_MESH_GATT_CFG_OP_STATUS, rsp);
}

int bt_mesh_gatt_cfg_cli_link_init(struct bt_mesh_gatt_cfg_cli *cli,
			  struct bt_mesh_msg_ctx *ctx)
{
	BT_MESH_MODEL_BUF_DEFINE(msg, BT_MESH_GATT_CFG_OP_LINK_INIT,
				 BT_MESH_GATT_CFG_MSG_LEN_LINK_INIT);
	bt_mesh_model_msg_init(&msg, BT_MESH_GATT_CFG_OP_LINK_INIT);

	return model_send(cli->model, ctx, &msg);
}

int bt_mesh_gatt_cfg_cli_link_fetch(struct bt_mesh_gatt_cfg_cli *cli,
			  struct bt_mesh_msg_ctx *ctx, struct link_data_entry *entry)
{
	BT_MESH_MODEL_BUF_DEFINE(msg, BT_MESH_GATT_CFG_OP_LINK_FETCH,
				 BT_MESH_GATT_CFG_MSG_LEN_LINK_FETCH);
	bt_mesh_model_msg_init(&msg, BT_MESH_GATT_CFG_OP_LINK_FETCH);

	return model_ackd_send(cli->model, ctx, &msg, &cli->ack_ctx,
			       BT_MESH_GATT_CFG_OP_LINK_FETCH_RSP, entry);
}