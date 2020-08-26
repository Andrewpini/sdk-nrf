/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include "thingy52_cli.h"
#include "model_utils.h"

static int bt_mesh_thingy52_cli_init(struct bt_mesh_model *model)
{
	struct bt_mesh_thingy52_cli *cli = model->user_data;

	cli->model = model;
	net_buf_simple_init(cli->pub.msg, 0);
	return 0;
}

const struct bt_mesh_model_cb _bt_mesh_thingy52_cli_cb = {
	.init = bt_mesh_thingy52_cli_init,
};

int bt_mesh_thingy52_cli_rgb_set(struct bt_mesh_thingy52_cli *cli,
				 struct bt_mesh_msg_ctx *ctx,
				 struct bt_mesh_thingy52_rgb_msg *rgb)
{
	BT_MESH_MODEL_BUF_DEFINE(msg, BT_MESH_THINGY52_OP_RGB_SET,
				 BT_MESH_THINGY52_MSG_LEN_RGB_SET);
	bt_mesh_model_msg_init(&msg, BT_MESH_THINGY52_OP_RGB_SET);

	net_buf_simple_add_le16(&msg, rgb->ttl);
	net_buf_simple_add_le16(&msg, rgb->delay);
	net_buf_simple_add_u8(&msg, rgb->color.red);
	net_buf_simple_add_u8(&msg, rgb->color.green);
	net_buf_simple_add_u8(&msg, rgb->color.blue);
	net_buf_simple_add_u8(&msg, rgb->speaker_on);
	return model_send(cli->model, ctx, &msg);
}
