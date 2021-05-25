#ifndef BT_MESH_GPC_H__
#define BT_MESH_GPC_H__

#include <bluetooth/mesh/model_types.h>
#include "mesh/proxy_client.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BT_MESH_GPC_ERR_SUCCESS 0x00

#define BT_MESH_GPC_VENDOR_COMPANY_ID 0x0059
#define BT_MESH_GPC_CLI_VENDOR_MODEL_ID 0x1500
#define BT_MESH_GPC_SRV_VENDOR_MODEL_ID 0x1501

/** @cond INTERNAL_HIDDEN */

#define BT_MESH_GPC_OP_STATUS BT_MESH_MODEL_OP_2(0x82, 0x0D)
#define BT_MESH_GPC_OP_ADV_SET BT_MESH_MODEL_OP_2(0x82, 0x0E)
#define BT_MESH_GPC_OP_CONN_SET BT_MESH_MODEL_OP_2(0x82, 0x0F)
#define BT_MESH_GPC_OP_ADV_ENABLE BT_MESH_MODEL_OP_2(0x82, 0x10)
#define BT_MESH_GPC_OP_LINK_UPDATE BT_MESH_MODEL_OP_2(0x82, 0x11)
#define BT_MESH_GPC_OP_LINK_INIT BT_MESH_MODEL_OP_2(0x82, 0x12)
#define BT_MESH_GPC_OP_LINK_FETCH BT_MESH_MODEL_OP_2(0x82, 0x13)
#define BT_MESH_GPC_OP_LINK_FETCH_RSP BT_MESH_MODEL_OP_2(0x82, 0x14)
#define BT_MESH_GPC_OP_CONN_LIST_RESET BT_MESH_MODEL_OP_2(0x82, 0x15)
#define BT_MESH_GPC_OP_TEST_MSG_INIT BT_MESH_MODEL_OP_2(0x82, 0x16)
#define BT_MESH_GPC_OP_TEST_MSG BT_MESH_MODEL_OP_2(0x82, 0x17)

#define BT_MESH_GPC_MSG_LEN_ADV_SET 2
#define BT_MESH_GPC_MSG_LEN_CONN_SET 3
#define BT_MESH_GPC_MSG_LEN_ADV_ENABLE 1
#define BT_MESH_GPC_MSG_LEN_LINK_UPDATE 2
#define BT_MESH_GPC_MSG_LEN_LINK_INIT 1
#define BT_MESH_GPC_MSG_LEN_LINK_FETCH 0
#define BT_MESH_GPC_MSG_MINLEN_FETCH_RSP 2
#define BT_MESH_GPC_MSG_MAXLEN_FETCH_RSP 98
#define BT_MESH_GPC_MSG_LEN_CONN_LIST_RESET 0
#define BT_MESH_GPC_MSG_LEN_STATUS 2
#define BT_MESH_GPC_MSG_LEN_TEST_MSG_INIT 1
#define BT_MESH_GPC_MSG_LEN_TEST_MSG 1

/** @endcond */

enum bt_mesh_gpc_status_type {
	BT_MESH_GPC_LINK_UPDATE_STARTED = 0,
	BT_MESH_GPC_LINK_UPDATE_ENDED = 1,
	BT_MESH_GPC_CONN_ADD = 2,
	BT_MESH_GPC_CONN_RESET = 3,
};

struct link_data {
	uint16_t root_addr;
	uint8_t received_cnt;
} __packed;

struct link_data_entry {
	uint16_t src;
	uint16_t entry_cnt;
	struct link_data data[32];
};

struct bt_mesh_gpc_adv_set {
	bool on_off;
	uint8_t net_id;
};

struct bt_mesh_gpc_conn_set {
	uint16_t addr;
	uint8_t net_id;
};

#ifdef __cplusplus
}
#endif

#endif /* BT_MESH_GPC_H__ */

/** @} */
