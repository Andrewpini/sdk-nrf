/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef BT_MESH_THINGY52_SRV_H__
#define BT_MESH_THINGY52_SRV_H__

#include "thingy52.h"

#ifdef __cplusplus
extern "C" {
#endif

struct bt_mesh_thingy52_srv;

/** @def BT_MESH_THINGY52_SRV_INIT
 *
 * @brief Init parameters for a @ref bt_mesh_thingy52_srv instance.
 *
 * @param[in] _cb_handlers Level handler to use in the model instance.
 */
#define BT_MESH_THINGY52_SRV_INIT(_cb_handlers)                                 \
	{                                                                      \
		.msg_callbacks = _cb_handlers,                                 \
		.pub = {                                                       \
			.msg = NET_BUF_SIMPLE(BT_MESH_MODEL_BUF_LEN(           \
				BT_MESH_ONOFF_OP_STATUS,                       \
				BT_MESH_ONOFF_MSG_MAXLEN_STATUS)),             \
		},                                                             \
	}

/** @def BT_MESH_MODEL_THINGY52_SRV
 *
 * @brief Thingy:52 Server model composition data entry.
 *
 * @param[in] _srv Pointer to a @ref bt_mesh_thingy52_srv instance.
 */
#define BT_MESH_MODEL_THINGY52_SRV(_srv)                                        \
	BT_MESH_MODEL_VND_CB(                                                  \
		BT_MESH_NORDIC_SEMI_COMPANY_ID, BT_MESH_MODEL_ID_THINGY52_SRV,  \
		_bt_mesh_thingy52_srv_op, &(_srv)->pub,                         \
		BT_MESH_MODEL_USER_DATA(struct bt_mesh_thingy52_srv, _srv),     \
		&_bt_mesh_thingy52_srv_cb)

struct bt_mesh_thingy52_cb {
	/** RGB message handler. */
	void (*const rgb_set_handler)(struct bt_mesh_thingy52_srv *srv,
				      struct bt_mesh_msg_ctx *ctx,
				      struct bt_mesh_thingy52_rgb_msg rgb);
};

struct bt_mesh_thingy52_rgb_work_ctx {
	uint16_t delay;
	struct bt_mesh_thingy52_rgb_msg rgb;
	struct k_delayed_work work;
};

/**
 * Thingy:52 Server instance. Should primarily be initialized with the
 * @ref BT_MESH_THINGY52_SRV_INIT macro.
 */
struct bt_mesh_thingy52_srv {
	/** Message callbacks */
	struct bt_mesh_thingy52_cb *msg_callbacks;
	/** Access model pointer. */
	struct bt_mesh_model *model;
	/** Publish parameters. */
	struct bt_mesh_model_pub pub;
};

int bt_mesh_thingy52_srv_rgb_set(struct bt_mesh_thingy52_srv *srv,
				struct bt_mesh_msg_ctx *ctx,
				struct bt_mesh_thingy52_rgb_msg *rgb);

/** @cond INTERNAL_HIDDEN */
extern const struct bt_mesh_model_op _bt_mesh_thingy52_srv_op[];
extern const struct bt_mesh_model_cb _bt_mesh_thingy52_srv_cb;
/** @endcond */

#ifdef __cplusplus
}
#endif

#endif /* BT_MESH_THINGY52_SRV_H__ */
