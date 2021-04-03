#include <string.h>
#include <stdlib.h>
#include <bluetooth/mesh/gatt_cfg_srv.h>
#include "model_utils.h"
#include "mesh/subnet.h"
#include "mesh/access.h"
#include "mesh/proxy.h"
#include "mesh/proxy_client.h"

static int32_t link_update_send(struct bt_mesh_gatt_cfg_srv *srv);
static int net_id_adv_set(struct bt_mesh_gatt_cfg_srv *srv,
			  uint16_t dst_addr,
			  struct bt_mesh_gatt_cfg_adv_set *set,
			  struct bt_mesh_gatt_cfg_status *rsp);


struct bt_mesh_gatt_cfg_srv *p_srv;

/** Persistent storage handling */
struct bt_mesh_gatt_cfg_srv_settings_data {
	uint8_t conn_list_idx;
	struct bt_mesh_gatt_cfg_conn_set conns[8];
} __packed;


static int store_state(struct bt_mesh_gatt_cfg_srv *srv)
{
	if (!IS_ENABLED(CONFIG_BT_SETTINGS)) {
		return 0;
	}

	struct bt_mesh_gatt_cfg_srv_settings_data data = {
		.conn_list_idx = srv->conn_list_idx,
	};

	for (size_t i = 0; i < ARRAY_SIZE(srv->conn_list); i++) {
		data.conns[i] = srv->conn_list[i].ctx;
	}

	return bt_mesh_model_data_store(srv->model, false, NULL,
					&data, sizeof(data));
}

static void l_data_print(struct bt_mesh_gatt_cfg_srv *srv)
{
	for (size_t i = 0; i < (srv->l_data_idx); i++)
	{
		printk("Addr: %d, Cnt: %d\n", srv->l_data[i].root_addr, srv->l_data[i].received_cnt);
	}
}

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

static void handle_node_id_adv_set(struct bt_mesh_model *model,
			     struct bt_mesh_msg_ctx *ctx,
			     struct net_buf_simple *buf)
{
	if (buf->len != BT_MESH_GATT_CFG_MSG_LEN_ADV_SET) {
		return;
	}

	struct bt_mesh_gatt_cfg_adv_set set;

	uint8_t on_off = net_buf_simple_pull_u8(buf);
	if (on_off > 1) {
		return;
	}

	set.on_off = on_off;
	set.net_id = net_buf_simple_pull_u8(buf);

	struct bt_mesh_subnet *sub;
	sub = bt_mesh_subnet_get(set.net_id);

	if (set.on_off) {
		bt_mesh_proxy_identity_start(sub);
	} else {
		bt_mesh_proxy_identity_stop(sub);
	}

	printk("Turning node ID advertising %s for net_id %d\n",
	       (set.on_off ? "On" : "OFF"), set.net_id);
}

static void conn_put(struct bt_mesh_gatt_cfg_srv *srv,
		     struct bt_mesh_gatt_cfg_conn_set set)
{
	for (size_t i = 0; i < ARRAY_SIZE(srv->conn_list); i++) {
		if ((set.addr == srv->conn_list[i].ctx.addr) &&
		    (set.net_id == srv->conn_list[i].ctx.net_id)) {
			printk("Already present in connection entries\n");
			return;
		}
	}
	if (srv->conn_list_idx <= (ARRAY_SIZE(srv->conn_list) - 1)) {
		srv->conn_list[srv->conn_list_idx].ctx = set;
		srv->conn_list_idx++;
	} else {
		printk("Connection entry buffer is full\n");
	}
}

static struct bt_mesh_gatt_cfg_conn_entry *conn_get(
		     uint16_t addr, uint8_t net_id)
{
	for (size_t i = 0; i < ARRAY_SIZE(p_srv->conn_list); i++) {
		if ((addr == p_srv->conn_list[i].ctx.addr) &&
		    (net_id == p_srv->conn_list[i].ctx.net_id)) {
			return &p_srv->conn_list[i];
		}
	}

	return NULL;
}

static void conn_link_handle(void)
{
	for (size_t i = 0; i < ARRAY_SIZE(p_srv->conn_list); i++) {
		if (p_srv->conn_list[i].ctx.addr)
		{
			if (!p_srv->conn_list[i].is_active)
			{
				struct bt_mesh_gatt_cfg_conn_set conn_set = {
					.addr = p_srv->conn_list[i].ctx.addr,
					.net_id = p_srv->conn_list[i].ctx.net_id,
				};
				struct bt_mesh_gatt_cfg_adv_set adv_set = {
					.on_off = true,
					.net_id = p_srv->conn_list[i].ctx.net_id,
				};

				bt_mesh_proxy_cli_node_id_ctx_set((struct node_id_lookup *)&conn_set);
				net_id_adv_set(p_srv, p_srv->conn_list[i].ctx.addr, &adv_set, NULL);
			}

		}

	}
}

static void handle_conn_set(struct bt_mesh_model *model,
			     struct bt_mesh_msg_ctx *ctx,
			     struct net_buf_simple *buf)
{
	if (buf->len != BT_MESH_GATT_CFG_MSG_LEN_CONN_SET) {
		return;
	}

	struct bt_mesh_gatt_cfg_srv *srv = model->user_data;
	struct bt_mesh_gatt_cfg_conn_set set;

	set.addr = net_buf_simple_pull_le16(buf);
	set.net_id = net_buf_simple_pull_u8(buf);
	conn_put(srv, set);
	store_state(srv);
	conn_link_handle();

	printk("Received a new connection request to node %d on net_id %d\n", set.addr, set.net_id);
}

static void handle_adv_enable(struct bt_mesh_model *model,
			     struct bt_mesh_msg_ctx *ctx,
			     struct net_buf_simple *buf)
{
	if (buf->len != BT_MESH_GATT_CFG_MSG_LEN_ADV_ENABLE) {
		return;
	}

	uint8_t on_off = net_buf_simple_pull_u8(buf);
	if (on_off > 1) {
		return;
	}

	// TODO: Handle turning the advertising on and off.

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

static void handle_conn_list_reset(struct bt_mesh_model *model,
			     struct bt_mesh_msg_ctx *ctx,
			     struct net_buf_simple *buf)
{
	if (buf->len != BT_MESH_GATT_CFG_MSG_LEN_CONN_LIST_RESET) {
		return;
	}

	struct bt_mesh_gatt_cfg_srv *srv = model->user_data;

	memset(srv->conn_list, 0, sizeof(srv->conn_list));
	srv->conn_list_idx = 0;
	int err = store_state(srv);
	printk("Storing status: %d\n", err);
}

const struct bt_mesh_model_op _bt_mesh_gatt_cfg_srv_op[] = {
	{ BT_MESH_GATT_CFG_OP_ADV_SET, BT_MESH_GATT_CFG_MSG_LEN_ADV_SET,
	  handle_node_id_adv_set },
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
	{ BT_MESH_GATT_CFG_OP_CONN_LIST_RESET, BT_MESH_GATT_CFG_MSG_LEN_CONN_LIST_RESET,
	  handle_conn_list_reset },
	BT_MESH_MODEL_OP_END,
};

static int update_handler(struct bt_mesh_model *model)
{
	return 0;
}

void gatt_connected_cb(struct bt_conn *conn,
		       struct node_id_lookup *addr_ctx, uint8_t reason)
{

	struct bt_mesh_gatt_cfg_conn_entry * entry;
	entry = conn_get(addr_ctx->addr, addr_ctx->net_idx);

	if (entry) {
		entry->is_active = true;
	}

	printk("NODE: %d CONNECTED\n", addr_ctx->addr);
	conn_link_handle();
}

void gatt_disconnected_cb(struct bt_conn *conn,
			  struct node_id_lookup *addr_ctx, uint8_t reason)
{

	struct bt_mesh_gatt_cfg_conn_entry * entry;
	entry = conn_get(addr_ctx->addr, addr_ctx->net_idx);

	if (entry) {
		entry->is_active = false;
	}

	printk("NODE: %d DISCONNECTED\n", addr_ctx->addr);
	conn_link_handle();
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
	bt_mesh_proxy_cli_conn_cb_set(gatt_connected_cb, gatt_disconnected_cb);
	p_srv = srv;
	return 0;
}

static void bt_mesh_gatt_cfg_srv_reset(struct bt_mesh_model *model)
{
	net_buf_simple_reset(model->pub->msg);
}

#ifdef CONFIG_BT_SETTINGS
static int bt_mesh_gatt_cfg_srv_settings_set(struct bt_mesh_model *model,
					      const char *name, size_t len_rd,
					      settings_read_cb read_cb,
					      void *cb_arg)
{

	struct bt_mesh_gatt_cfg_srv *srv = model->user_data;
	struct bt_mesh_gatt_cfg_srv_settings_data data;
	ssize_t result;

	if (name) {
		return -ENOENT;
	}

	result = read_cb(cb_arg, &data, sizeof(data));
	if (result <= 0) {
		return result;
	} else if (result < sizeof(data)) {
		return -EINVAL;
	}

	srv->conn_list_idx = data.conn_list_idx;

	for (size_t i = 0; i < ARRAY_SIZE(srv->conn_list); i++) {
		srv->conn_list[i].ctx = data.conns[i];
		printk("addr: %d, net_id: %d\n", srv->conn_list[i].ctx.addr, srv->conn_list[i].ctx.net_id);
	}

	return 0;
}
#endif

static int bt_mesh_gatt_cfg_srv_start(struct bt_mesh_model *model)
{

	bt_mesh_subnet_foreach(bt_mesh_proxy_identity_start);
	conn_link_handle();
	return 0;
}

const struct bt_mesh_model_cb _bt_mesh_gatt_cfg_srv_cb = {
	.init = bt_mesh_gatt_cfg_srv_init,
	.reset = bt_mesh_gatt_cfg_srv_reset,
#ifdef CONFIG_BT_SETTINGS
	.settings_set = bt_mesh_gatt_cfg_srv_settings_set,
	.start = bt_mesh_gatt_cfg_srv_start,
#endif
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

static int net_id_adv_set(struct bt_mesh_gatt_cfg_srv *srv,
			  uint16_t dst_addr,
			  struct bt_mesh_gatt_cfg_adv_set *set,
			  struct bt_mesh_gatt_cfg_status *rsp)
{
	BT_MESH_MODEL_BUF_DEFINE(msg, BT_MESH_GATT_CFG_OP_ADV_SET,
				 BT_MESH_GATT_CFG_MSG_LEN_ADV_SET);
	bt_mesh_model_msg_init(&msg, BT_MESH_GATT_CFG_OP_ADV_SET);

	net_buf_simple_add_u8(&msg, set->on_off);
	net_buf_simple_add_u8(&msg, set->net_id);

	struct bt_mesh_msg_ctx ctx = {
		.addr = dst_addr,
		.send_ttl = 0,
		.send_rel = srv->pub.send_rel,
		.app_idx = srv->pub.key,
	};

	return model_send(srv->model, &ctx, &msg);
}