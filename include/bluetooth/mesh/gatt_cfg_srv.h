#ifndef BT_MESH_GATT_CFG_SRV_H__
#define BT_MESH_GATT_CFG_SRV_H__

#include <bluetooth/mesh/gatt_cfg.h>
#include <bluetooth/conn.h>


#ifdef __cplusplus
extern "C" {
#endif

struct bt_mesh_gatt_cfg_srv;

#define BT_MESH_GATT_CFG_SRV_INIT(_handlers)                                   \
	{                                                                      \
		.handlers = _handlers,                                         \
	}

#define BT_MESH_MODEL_GATT_CFG_SRV(_srv)                                       \
	BT_MESH_MODEL_CB(BT_MESH_GATT_CFG_SRV_VENDOR_MODEL_ID,                 \
			 _bt_mesh_gatt_cfg_srv_op, &(_srv)->pub,               \
			 BT_MESH_MODEL_USER_DATA(struct bt_mesh_gatt_cfg_srv,  \
						 _srv),                        \
			 &_bt_mesh_gatt_cfg_srv_cb)

struct bt_mesh_gatt_cfg_srv_handlers {
	void (*const set)(struct bt_mesh_gatt_cfg_srv *srv,
			  struct bt_mesh_msg_ctx *ctx,
			  const struct bt_mesh_gatt_cfg_set *set,
			  struct bt_mesh_gatt_cfg_status *rsp);

	void (*const get)(struct bt_mesh_gatt_cfg_srv *srv,
			  struct bt_mesh_msg_ctx *ctx,
			  struct bt_mesh_gatt_cfg_status *rsp);
};

struct bt_mesh_gatt_cfg_conn_entry {
	struct bt_mesh_gatt_cfg_conn_set ctx;
	bool is_active;
	struct bt_conn *conn;
};

struct bt_mesh_gatt_cfg_srv {
	/** Transaction ID tracker. */
	struct bt_mesh_tid_ctx prev_transaction;
	/** Handler function structure. */
	const struct bt_mesh_gatt_cfg_srv_handlers *handlers;
	/** Access model pointer. */
	struct bt_mesh_model *model;
	/** Publish parameters. */
	struct bt_mesh_model_pub pub;
	/* Publication buffer */
	struct net_buf_simple pub_buf;
	/* Publication data */
	uint8_t pub_data[BT_MESH_MODEL_BUF_LEN(
		BT_MESH_GATT_CFG_OP_STATUS, BT_MESH_GATT_CFG_MSG_LEN_STATUS)];

	struct k_delayed_work l_data_work;
	uint8_t l_data_msg_cnt;
	bool link_update_active;
	uint8_t l_data_idx;
	struct link_data l_data[32];

	struct k_delayed_work conn_entry_work;
	struct bt_mesh_gatt_cfg_conn_entry conn_list[8];
	uint8_t conn_list_idx;

	enum bt_mesh_proxy_cli_adv_state adv_state;
};

int32_t bt_mesh_gatt_cfg_srv_pub(struct bt_mesh_gatt_cfg_srv *srv,
			    struct bt_mesh_msg_ctx *ctx,
			    enum bt_mesh_gatt_cfg_status_type status,
			    uint8_t err_code);

/** @cond INTERNAL_HIDDEN */
extern const struct bt_mesh_model_op _bt_mesh_gatt_cfg_srv_op[];
extern const struct bt_mesh_model_cb _bt_mesh_gatt_cfg_srv_cb;
/** @endcond */

#ifdef __cplusplus
}
#endif

#endif /* BT_MESH_GATT_CFG_SRV_H__ */

/** @} */
