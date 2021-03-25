#include <string.h>
#include <bluetooth/mesh/gatt_cfg_srv.h>
#include "model_utils.h"
#include "mesh/subnet.h"
#include "mesh/access.h"
// #include "mesh/proxy.h"
// #include "mesh/proxy_client.h"

static int32_t link_update_send(struct bt_mesh_gatt_cfg_srv *srv);
static void l_data_print(struct bt_mesh_gatt_cfg_srv *srv);


static void l_data_cb(struct k_work *work)
{
	struct bt_mesh_gatt_cfg_srv *srv =
		CONTAINER_OF(work, struct bt_mesh_gatt_cfg_srv, l_data_work);
	if (srv->l_data_msg_cnt) {
		link_update_send(srv);
		srv->l_data_msg_cnt--;
		k_delayed_work_submit(&srv->l_data_work, K_MSEC(200));
	} else {
		l_data_print(srv);
		bt_mesh_gatt_cfg_srv_pub(srv, NULL, BT_MESH_GATT_CFG_LINK_UPDATE_ENDED);
	}
}


static void l_data_put(struct bt_mesh_gatt_cfg_srv *srv, uint16_t addr)
{
	for (size_t i = 0; i < ARRAY_SIZE(srv->l_data); i++)
	{
		if (addr == srv->l_data[i].root_addr)
		{
			srv->l_data[i].received_cnt ++;
			return;
		}
	}

	srv->l_data[srv->l_data_idx].root_addr = addr;
	srv->l_data[srv->l_data_idx].received_cnt ++;
	srv->l_data_idx ++;
}

static void l_data_print(struct bt_mesh_gatt_cfg_srv *srv)
{
	for (size_t i = 0; i < (srv->l_data_idx); i++)
	{
		printk("Addr: %d, Cnt: %d\n", srv->l_data[i].root_addr, srv->l_data[i].received_cnt);
	}
}

static void encode_status(struct net_buf_simple *buf,
			  enum bt_mesh_gatt_cfg_status_type status)
{
	bt_mesh_model_msg_init(buf, BT_MESH_GATT_CFG_OP_STATUS);
	net_buf_simple_add_u8(buf, status);
}

static void rsp_status(struct bt_mesh_model *model,
		       struct bt_mesh_msg_ctx *rx_ctx,
		       enum bt_mesh_gatt_cfg_status_type status)
{
	BT_MESH_MODEL_BUF_DEFINE(msg, BT_MESH_GATT_CFG_OP_STATUS,
				 BT_MESH_GATT_CFG_MSG_LEN_STATUS);
	encode_status(&msg, status);

	(void)bt_mesh_model_send(model, rx_ctx, &msg, NULL, NULL);
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

	// if (set.on_off) {
	// 	bt_mesh_proxy_identity_start(sub);
	// } else {
	// 	bt_mesh_proxy_identity_stop(sub);
	// }

	// (void)bt_mesh_gatt_cfg_srv_pub(srv, NULL, &status);

	// rsp_status(model, ctx, &status);

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

	// bt_mesh_proxy_cli_node_id_ctx_set((struct node_id_lookup *)&set);

	printk("CONNECTION\n");
	// (void)bt_mesh_gatt_cfg_srv_pub(srv, NULL, &status);

	// rsp_status(model, ctx, &status);

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
	// bt_mesh_proxy_cli_adv_set(on_off);

	// (void)bt_mesh_gatt_cfg_srv_pub(srv, NULL, &status);

	// rsp_status(model, ctx, &status);

	printk("Turning the advertiser %s\n", (on_off ? "On" : "OFF"));
}

static void handle_link_update(struct bt_mesh_model *model,
			     struct bt_mesh_msg_ctx *ctx,
			     struct net_buf_simple *buf)
{
	if (buf->len != BT_MESH_GATT_CFG_MSG_LEN_LINK_UPDATE) {
		return;
	}

	struct bt_mesh_gatt_cfg_srv *srv = model->user_data;
	uint16_t addr = net_buf_simple_pull_le16(buf);


	if (srv->link_update_active && (addr != bt_mesh_primary_addr()))
	{
		l_data_put(srv, addr);
	}

}

static void handle_link_init(struct bt_mesh_model *model,
			     struct bt_mesh_msg_ctx *ctx,
			     struct net_buf_simple *buf)
{
	if (buf->len != BT_MESH_GATT_CFG_MSG_LEN_LINK_INIT) {
		return;
	}

	struct bt_mesh_gatt_cfg_srv *srv = model->user_data;

	memset(srv->l_data, 0, sizeof(srv->l_data));
	srv->l_data_msg_cnt = net_buf_simple_pull_u8(buf);
	srv->link_update_active = true;
	srv->l_data_idx = 0;

	k_delayed_work_submit(&srv->l_data_work, K_MSEC(1000));

	rsp_status(model, ctx, BT_MESH_GATT_CFG_LINK_UPDATE_STARTED);
}

static void handle_link_fetch(struct bt_mesh_model *model,
			     struct bt_mesh_msg_ctx *ctx,
			     struct net_buf_simple *buf)
{
	if (buf->len != BT_MESH_GATT_CFG_MSG_LEN_LINK_FETCH) {
		return;
	}

	struct bt_mesh_gatt_cfg_srv *srv = model->user_data;

	BT_MESH_MODEL_BUF_DEFINE(msg, BT_MESH_GATT_CFG_OP_LINK_FETCH_RSP,
				 2 + (srv->l_data_idx * sizeof(struct link_data)));


	bt_mesh_model_msg_init(&msg, BT_MESH_GATT_CFG_OP_LINK_FETCH_RSP);

	net_buf_simple_add_le16(&msg, bt_mesh_primary_addr());
	for (size_t i = 0; i < srv->l_data_idx; i++)
	{
		net_buf_simple_add_le16(&msg, srv->l_data[i].root_addr);
		net_buf_simple_add_u8(&msg, srv->l_data[i].received_cnt);
	}

	(void)bt_mesh_model_send(srv->model, ctx, &msg, NULL, NULL);

}

static void handle_echo(struct bt_mesh_model *model,
			     struct bt_mesh_msg_ctx *ctx,
			     struct net_buf_simple *buf)
{
	if (buf->len != BT_MESH_GATT_CFG_MSG_LEN_ECHO) {
		return;
	}

	struct bt_mesh_gatt_cfg_srv *srv = model->user_data;

	// BT_MESH_MODEL_BUF_DEFINE(msg, BT_MESH_GATT_CFG_OP_LINK_FETCH_RSP,
	// 			 2 + (srv->l_data_idx * sizeof(struct link_data)));


	// bt_mesh_model_msg_init(&msg, BT_MESH_GATT_CFG_OP_LINK_FETCH_RSP);

	// net_buf_simple_add_le16(&msg, bt_mesh_primary_addr());
	// for (size_t i = 0; i < srv->l_data_idx; i++)
	// {
	// 	net_buf_simple_add_le16(&msg, srv->l_data[i].root_addr);
	// 	net_buf_simple_add_u8(&msg, srv->l_data[i].received_cnt);
	// }

	// (void)bt_mesh_model_send(srv->model, ctx, &msg, NULL, NULL);

}

const struct bt_mesh_model_op _bt_mesh_gatt_cfg_srv_op[] = {
	// { BT_MESH_GATT_CFG_OP_GET, BT_MESH_GATT_CFG_MSG_LEN_GET, handle_get },
	// { BT_MESH_GATT_CFG_OP_SET, BT_MESH_GATT_CFG_MSG_MINLEN_SET, handle_set },
	// { BT_MESH_GATT_CFG_OP_SET_UNACK, BT_MESH_GATT_CFG_MSG_MINLEN_SET,
	//   handle_set_unack },
	{ BT_MESH_GATT_CFG_OP_ADV_SET, BT_MESH_GATT_CFG_MSG_LEN_ADV_SET,
	  handle_adv_set },
	{ BT_MESH_GATT_CFG_OP_CONN_SET, BT_MESH_GATT_CFG_MSG_LEN_CONN_SET,
	  handle_conn_set },
	{ BT_MESH_GATT_CFG_OP_ADV_ENABLE, BT_MESH_GATT_CFG_MSG_LEN_ADV_ENABLE,
	  handle_adv_enable },
	{ BT_MESH_GATT_CFG_OP_LINK_UPDATE, BT_MESH_GATT_CFG_MSG_LEN_LINK_UPDATE,
	  handle_link_update },
	{ BT_MESH_GATT_CFG_OP_LINK_INIT, BT_MESH_GATT_CFG_MSG_LEN_LINK_INIT,
	  handle_link_init },
	{ BT_MESH_GATT_CFG_OP_LINK_FETCH, BT_MESH_GATT_CFG_MSG_LEN_LINK_FETCH,
	  handle_link_fetch },
	{ BT_MESH_GATT_CFG_OP_ECHO, BT_MESH_GATT_CFG_MSG_LEN_ECHO,
	  handle_echo },
	BT_MESH_MODEL_OP_END,
};

static int update_handler(struct bt_mesh_model *model)
{
	struct bt_mesh_gatt_cfg_srv *srv = model->user_data;
	struct bt_mesh_gatt_cfg_status status = { 0 };

	// srv->handlers->get(srv, NULL, &status);
	// encode_status(model->pub->msg, &status);

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
	k_delayed_work_init(&srv->l_data_work, l_data_cb);

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
			    enum bt_mesh_gatt_cfg_status_type status)
{
	BT_MESH_MODEL_BUF_DEFINE(msg, BT_MESH_GATT_CFG_OP_STATUS,
				 BT_MESH_GATT_CFG_MSG_LEN_STATUS);
	encode_status(&msg, status);
	return model_send(srv->model, ctx, &msg);
}

static int32_t link_update_send(struct bt_mesh_gatt_cfg_srv *srv)
{
	BT_MESH_MODEL_BUF_DEFINE(msg, BT_MESH_GATT_CFG_OP_LINK_UPDATE,
				 BT_MESH_GATT_CFG_MSG_LEN_LINK_UPDATE);
	bt_mesh_model_msg_init(&msg, BT_MESH_GATT_CFG_OP_LINK_UPDATE);
	net_buf_simple_add_le16(&msg, bt_mesh_primary_addr());

	struct bt_mesh_msg_ctx ctx = {
		.addr = srv->pub.addr,
		.send_ttl = 0,
		.send_rel = srv->pub.send_rel,
		.app_idx = srv->pub.key,
	};

	return model_send(srv->model, &ctx, &msg);
}

static int32_t link_update_ctx_send(struct bt_mesh_gatt_cfg_srv *srv)
{
	BT_MESH_MODEL_BUF_DEFINE(msg, BT_MESH_GATT_CFG_OP_LINK_UPDATE,
				 BT_MESH_GATT_CFG_MSG_LEN_LINK_UPDATE);
	bt_mesh_model_msg_init(&msg, BT_MESH_GATT_CFG_OP_LINK_UPDATE);
	net_buf_simple_add_le16(&msg, bt_mesh_primary_addr());

	struct bt_mesh_msg_ctx ctx = {
		.addr = srv->pub.addr,
		.send_ttl = 0,
		.send_rel = srv->pub.send_rel,
		.app_idx = srv->pub.key,
	};

	return model_send(srv->model, &ctx, &msg);
}