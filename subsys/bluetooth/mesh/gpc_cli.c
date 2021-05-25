
#include <string.h>
#include <bluetooth/mesh/gpc_cli.h>
#include "model_utils.h"


static void handle_status(struct bt_mesh_model *model,
			  struct bt_mesh_msg_ctx *ctx,
			  struct net_buf_simple *buf)
{
	if (buf->len != BT_MESH_GPC_MSG_LEN_STATUS) {
		return;
	}

	struct bt_mesh_gpc_cli *cli = model->user_data;

	enum bt_mesh_gpc_status_type status = net_buf_simple_pull_u8(buf);
	uint8_t err_code = net_buf_simple_pull_u8(buf);

	if (cli->status_handler) {
		cli->status_handler(cli, ctx, status, err_code);
	}
}

static void handle_fetch_rsp(struct bt_mesh_model *model,
			  struct bt_mesh_msg_ctx *ctx,
			  struct net_buf_simple *buf)
{
	if (buf->len < BT_MESH_GPC_MSG_MINLEN_FETCH_RSP ||
	    buf->len > BT_MESH_GPC_MSG_MAXLEN_FETCH_RSP) {
		return;
	}

	struct bt_mesh_gpc_cli *cli = model->user_data;
	struct link_data_entry entry = {0};

	entry.src = net_buf_simple_pull_le16(buf);
	entry.entry_cnt = buf->len / sizeof(struct link_data);
	for (size_t i = 0; i < entry.entry_cnt; i++)
	{
		entry.data[i].root_addr = net_buf_simple_pull_le16(buf);
		entry.data[i].received_cnt = net_buf_simple_pull_u8(buf);
	}

	if (model_ack_match(&cli->ack_ctx, BT_MESH_GPC_OP_LINK_FETCH_RSP, ctx)) {
		struct link_data_entry *rsp =
			(struct link_data_entry *)cli->ack_ctx.user_data;

		*rsp = entry;
		model_ack_rx(&cli->ack_ctx);
	}
}

const struct bt_mesh_model_op _bt_mesh_gpc_cli_op[] = {
	{ BT_MESH_GPC_OP_STATUS, BT_MESH_GPC_MSG_LEN_STATUS,
	  handle_status },
	{ BT_MESH_GPC_OP_LINK_FETCH_RSP, BT_MESH_GPC_MSG_MINLEN_FETCH_RSP,
	  handle_fetch_rsp },
	BT_MESH_MODEL_OP_END,
};

static int bt_mesh_gpc_cli_init(struct bt_mesh_model *model)
{
	struct bt_mesh_gpc_cli *cli = model->user_data;

	cli->model = model;
	cli->pub.msg = &cli->pub_buf;
	net_buf_simple_init_with_data(&cli->pub_buf, cli->pub_data,
				      sizeof(cli->pub_data));
	model_ack_init(&cli->ack_ctx);

	return 0;
}

static void bt_mesh_gpc_cli_reset(struct bt_mesh_model *model)
{
	struct bt_mesh_gpc_cli *cli = model->user_data;

	net_buf_simple_reset(cli->pub.msg);
	model_ack_reset(&cli->ack_ctx);
}

const struct bt_mesh_model_cb _bt_mesh_gpc_cli_cb = {
	.init = bt_mesh_gpc_cli_init,
	.reset = bt_mesh_gpc_cli_reset,
};

int bt_mesh_gpc_cli_adv_set(struct bt_mesh_gpc_cli *cli,
			  struct bt_mesh_msg_ctx *ctx,
			  struct bt_mesh_gpc_adv_set *set)
{
	BT_MESH_MODEL_BUF_DEFINE(msg, BT_MESH_GPC_OP_ADV_SET,
				 BT_MESH_GPC_MSG_LEN_ADV_SET);
	bt_mesh_model_msg_init(&msg, BT_MESH_GPC_OP_ADV_SET);

	net_buf_simple_add_u8(&msg, set->on_off);
	net_buf_simple_add_u8(&msg, set->net_id);

	return model_send(cli->model, ctx, &msg);
}

int bt_mesh_gpc_cli_conn_set(struct bt_mesh_gpc_cli *cli,
			  struct bt_mesh_msg_ctx *ctx,
			  struct bt_mesh_gpc_conn_set *set)
{
	BT_MESH_MODEL_BUF_DEFINE(msg, BT_MESH_GPC_OP_CONN_SET,
				 BT_MESH_GPC_MSG_LEN_CONN_SET);
	bt_mesh_model_msg_init(&msg, BT_MESH_GPC_OP_CONN_SET);

	net_buf_simple_add_le16(&msg, set->addr);
	net_buf_simple_add_u8(&msg, set->net_id);

	return model_send(cli->model, ctx, &msg);
}

int bt_mesh_gpc_cli_adv_enable(struct bt_mesh_gpc_cli *cli,
			  struct bt_mesh_msg_ctx *ctx,
			  enum bt_mesh_proxy_cli_adv_state state)
{
	BT_MESH_MODEL_BUF_DEFINE(msg, BT_MESH_GPC_OP_ADV_ENABLE,
				 BT_MESH_GPC_MSG_LEN_ADV_ENABLE);
	bt_mesh_model_msg_init(&msg, BT_MESH_GPC_OP_ADV_ENABLE);

	net_buf_simple_add_u8(&msg, state);

	return model_send(cli->model, ctx, &msg);
}

int bt_mesh_gpc_cli_link_init(struct bt_mesh_gpc_cli *cli,
			  struct bt_mesh_msg_ctx *ctx, uint8_t msg_cnt)
{
	BT_MESH_MODEL_BUF_DEFINE(msg, BT_MESH_GPC_OP_LINK_INIT,
				 BT_MESH_GPC_MSG_LEN_LINK_INIT);
	bt_mesh_model_msg_init(&msg, BT_MESH_GPC_OP_LINK_INIT);

	net_buf_simple_add_u8(&msg, msg_cnt);

	return model_send(cli->model, ctx, &msg);
}

int bt_mesh_gpc_cli_link_fetch(struct bt_mesh_gpc_cli *cli,
			  struct bt_mesh_msg_ctx *ctx, struct link_data_entry *entry)
{
	BT_MESH_MODEL_BUF_DEFINE(msg, BT_MESH_GPC_OP_LINK_FETCH,
				 BT_MESH_GPC_MSG_LEN_LINK_FETCH);
	bt_mesh_model_msg_init(&msg, BT_MESH_GPC_OP_LINK_FETCH);

	return model_ackd_send(cli->model, ctx, &msg, &cli->ack_ctx,
			       BT_MESH_GPC_OP_LINK_FETCH_RSP, entry);
}

int bt_mesh_gpc_cli_conn_reset(struct bt_mesh_gpc_cli *cli,
			  struct bt_mesh_msg_ctx *ctx)
{
	BT_MESH_MODEL_BUF_DEFINE(msg, BT_MESH_GPC_OP_CONN_LIST_RESET,
				 BT_MESH_GPC_MSG_LEN_CONN_LIST_RESET);
	bt_mesh_model_msg_init(&msg, BT_MESH_GPC_OP_CONN_LIST_RESET);


	return model_send(cli->model, ctx, &msg);
}

int bt_mesh_gpc_cli_test_msg_init(struct bt_mesh_gpc_cli *cli,
			  struct bt_mesh_msg_ctx *ctx, bool onoff)
{
	BT_MESH_MODEL_BUF_DEFINE(msg, BT_MESH_GPC_OP_TEST_MSG_INIT,
				 BT_MESH_GPC_MSG_LEN_TEST_MSG_INIT);
	bt_mesh_model_msg_init(&msg, BT_MESH_GPC_OP_TEST_MSG_INIT);
	net_buf_simple_add_u8(&msg, onoff);

	return model_send(cli->model, ctx, &msg);
}