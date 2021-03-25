#ifndef BT_MESH_GATT_CFG_CLI_H__
#define BT_MESH_GATT_CFG_CLI_H__

#include <bluetooth/mesh/gatt_cfg.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_mesh_gatt_cfg_cli;

#define BT_MESH_GATT_CFG_CLI_INIT(_status_handler)                             \
	{                                                                      \
		.status_handler = _status_handler,                             \
	}

#define BT_MESH_MODEL_GATT_CFG_CLI(_cli)                                       \
	BT_MESH_MODEL_VND_CB(                                                  \
		BT_MESH_GATT_CFG_VENDOR_COMPANY_ID,                            \
		BT_MESH_GATT_CFG_CLI_VENDOR_MODEL_ID,                          \
		_bt_mesh_gatt_cfg_cli_op, &(_cli)->pub,                        \
		BT_MESH_MODEL_USER_DATA(struct bt_mesh_gatt_cfg_cli, _cli),    \
		&_bt_mesh_gatt_cfg_cli_cb)

struct bt_mesh_gatt_cfg_cli {
	void (*const status_handler)(struct bt_mesh_gatt_cfg_cli *cli,
				     struct bt_mesh_msg_ctx *ctx,
				     enum bt_mesh_gatt_cfg_status_type status);
	/** Current Transaction ID. */
	uint8_t tid;
	/** Response context for tracking acknowledged messages. */
	struct bt_mesh_model_ack_ctx ack_ctx;
	/** Publish parameters. */
	struct bt_mesh_model_pub pub;
	/* Publication buffer */
	struct net_buf_simple pub_buf;
	/* Publication data */
	uint8_t pub_data[BT_MESH_MODEL_BUF_LEN(BT_MESH_GATT_CFG_OP_SET,
					       BT_MESH_GATT_CFG_MSG_MAXLEN_SET)];
	/** Access model pointer. */
	struct bt_mesh_model *model;
};


int bt_mesh_gatt_cfg_cli_get(struct bt_mesh_gatt_cfg_cli *cli,
			  struct bt_mesh_msg_ctx *ctx,
			  struct bt_mesh_gatt_cfg_status *rsp);


int bt_mesh_gatt_cfg_cli_set(struct bt_mesh_gatt_cfg_cli *cli,
			  struct bt_mesh_msg_ctx *ctx,
			  const struct bt_mesh_gatt_cfg_set *set,
			  struct bt_mesh_gatt_cfg_status *rsp);


int bt_mesh_gatt_cfg_cli_set_unack(struct bt_mesh_gatt_cfg_cli *cli,
				struct bt_mesh_msg_ctx *ctx,
				const struct bt_mesh_gatt_cfg_set *set);

int bt_mesh_gatt_cfg_cli_adv_set(struct bt_mesh_gatt_cfg_cli *cli,
			  struct bt_mesh_msg_ctx *ctx,
			  struct bt_mesh_gatt_cfg_adv_set *set,
			  struct bt_mesh_gatt_cfg_status *rsp);

int bt_mesh_gatt_cfg_cli_conn_set(struct bt_mesh_gatt_cfg_cli *cli,
			  struct bt_mesh_msg_ctx *ctx,
			  struct bt_mesh_gatt_cfg_conn_set *set,
			  struct bt_mesh_gatt_cfg_status *rsp);

int bt_mesh_gatt_cfg_cli_adv_enable(struct bt_mesh_gatt_cfg_cli *cli,
			  struct bt_mesh_msg_ctx *ctx,
			  bool onoff,
			  struct bt_mesh_gatt_cfg_status *rsp);

int bt_mesh_gatt_cfg_cli_link_init(struct bt_mesh_gatt_cfg_cli *cli,
			  struct bt_mesh_msg_ctx *ctx, uint8_t msg_cnt);

int bt_mesh_gatt_cfg_cli_link_fetch(struct bt_mesh_gatt_cfg_cli *cli,
			  struct bt_mesh_msg_ctx *ctx, struct link_data_entry *entry);

int bt_mesh_gatt_cfg_cli_link_echo(struct bt_mesh_gatt_cfg_cli *cli,
			  struct bt_mesh_msg_ctx *ctx);

/** @cond INTERNAL_HIDDEN */
extern const struct bt_mesh_model_op _bt_mesh_gatt_cfg_cli_op[];
extern const struct bt_mesh_model_cb _bt_mesh_gatt_cfg_cli_cb;
/** @endcond */

#ifdef __cplusplus
}
#endif

#endif /* BT_MESH_GATT_CFG_CLI_H__ */

/** @} */
