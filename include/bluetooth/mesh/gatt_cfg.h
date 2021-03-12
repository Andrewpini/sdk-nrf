#ifndef BT_MESH_GATT_CFG_H__
#define BT_MESH_GATT_CFG_H__

#include <bluetooth/mesh/model_types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BT_MESH_GATT_CFG_VENDOR_COMPANY_ID 0x0059
#define BT_MESH_GATT_CFG_CLI_VENDOR_MODEL_ID 0x000A
#define BT_MESH_GATT_CFG_SRV_VENDOR_MODEL_ID 0x000B

/** @cond INTERNAL_HIDDEN */
#define BT_MESH_GATT_CFG_OP_GET                                                 \
	BT_MESH_MODEL_OP_3(0x0A, BT_MESH_GATT_CFG_VENDOR_COMPANY_ID)
#define BT_MESH_GATT_CFG_OP_SET                                                 \
	BT_MESH_MODEL_OP_3(0x0B, BT_MESH_GATT_CFG_VENDOR_COMPANY_ID)
#define BT_MESH_GATT_CFG_OP_SET_UNACK                                           \
	BT_MESH_MODEL_OP_3(0x0C, BT_MESH_GATT_CFG_VENDOR_COMPANY_ID)
#define BT_MESH_GATT_CFG_OP_STATUS                                              \
	BT_MESH_MODEL_OP_3(0x0D, BT_MESH_GATT_CFG_VENDOR_COMPANY_ID)

#define BT_MESH_GATT_CFG_OP_ADV_SET                                             \
	BT_MESH_MODEL_OP_3(0x0E, BT_MESH_GATT_CFG_VENDOR_COMPANY_ID)
#define BT_MESH_GATT_CFG_OP_CONN_SET                                             \
	BT_MESH_MODEL_OP_3(0x0F, BT_MESH_GATT_CFG_VENDOR_COMPANY_ID)
#define BT_MESH_GATT_CFG_OP_ADV_ENABLE                                             \
	BT_MESH_MODEL_OP_3(0x10, BT_MESH_GATT_CFG_VENDOR_COMPANY_ID)

#define BT_MESH_GATT_CFG_MSG_LEN_GET 0
#define BT_MESH_GATT_CFG_MSG_MINLEN_SET 2
#define BT_MESH_GATT_CFG_MSG_MAXLEN_SET 4
#define BT_MESH_GATT_CFG_MSG_MINLEN_STATUS 1
#define BT_MESH_GATT_CFG_MSG_MAXLEN_STATUS 3

#define BT_MESH_GATT_CFG_MSG_LEN_ADV_SET 2
#define BT_MESH_GATT_CFG_MSG_LEN_CONN_SET 3
#define BT_MESH_GATT_CFG_MSG_LEN_ADV_ENABLE 1
/** @endcond */

struct bt_mesh_gatt_cfg_set {
	/** State to set. */
	bool on_off;
	/** Transition parameters. */
	const struct bt_mesh_model_transition *transition;
};

struct bt_mesh_gatt_cfg_adv_set {
	bool on_off;
	uint8_t net_id;
};

struct bt_mesh_gatt_cfg_conn_set {
	uint16_t addr;
	uint8_t net_id;
};

struct bt_mesh_gatt_cfg_status {
	/** The present value of the Generic OnOff state. */
	bool present_on_off;
	/** The target value of the Generic OnOff state (optional). */
	bool target_on_off;
	/** Remaining time value in milliseconds. */
	int32_t remaining_time;
};

#ifdef __cplusplus
}
#endif

#endif /* BT_MESH_GATT_CFG_H__ */

/** @} */
