/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef BT_MESH_THINGY52_CLI_H__
#define BT_MESH_THINGY52_CLI_H__

#include "thingy52.h"

#ifdef __cplusplus
extern "C" {
#endif

struct bt_mesh_thingy52_cli;

/** @def BT_MESH_THINGY52_CLI_INIT
 *
 * @brief Initialization parameters for a @ref bt_mesh_thingy52_cli instance.
 */
#define BT_MESH_THINGY52_CLI_INIT                                               \
	{                                                                      \
		.pub = {.msg = NET_BUF_SIMPLE(BT_MESH_MODEL_BUF_LEN(           \
				BT_MESH_THINGY52_OP_RGB_SET,                    \
				BT_MESH_THINGY52_MSG_LEN_RGB_SET)) }            \
	}

/** @def BT_MESH_MODEL_THINGY52_CLI
 *
 * @brief Thingy:52 Client model composition data entry.
 *
 * @param[in] _cli Pointer to a @ref bt_mesh_thingy52_cli instance.
 */
#define BT_MESH_MODEL_THINGY52_CLI(_cli)                                        \
	BT_MESH_MODEL_VND_CB(                                                  \
		BT_MESH_NORDIC_SEMI_COMPANY_ID, BT_MESH_MODEL_ID_THINGY52_CLI,  \
		NULL, &(_cli)->pub,                                            \
		BT_MESH_MODEL_USER_DATA(struct bt_mesh_thingy52_cli, _cli),     \
		&_bt_mesh_thingy52_cli_cb)

/**
 * Thingy:52 Client structure.
 *
 * Should be initialized with the @ref BT_MESH_THINGY52_CLI_INIT macro.
 */
struct bt_mesh_thingy52_cli {
	/** Response context for tracking acknowledged messages. */
	/** Publish parameters. */
	struct bt_mesh_model_pub pub;
	/** Access model pointer. */
	struct bt_mesh_model *model;
};

int bt_mesh_thingy52_cli_rgb_set(struct bt_mesh_thingy52_cli *cli,
				struct bt_mesh_msg_ctx *ctx,
				struct bt_mesh_thingy52_rgb_msg *rgb);

/** @cond INTERNAL_HIDDEN */
extern const struct bt_mesh_model_cb _bt_mesh_thingy52_cli_cb;
/** @endcond */

#ifdef __cplusplus
}
#endif

#endif /* BT_MESH_THINGY52_CLI_H__ */
