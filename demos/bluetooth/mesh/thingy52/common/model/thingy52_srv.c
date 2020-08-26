/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdlib.h>
#include "thingy52_srv.h"
#include "model_utils.h"
#include <bluetooth/mesh/models.h>

static void handle_rgb_set(struct bt_mesh_model *model,
			   struct bt_mesh_msg_ctx *ctx,
			   struct net_buf_simple *buf)
{
	struct bt_mesh_thingy52_srv *srv = model->user_data;
	struct bt_mesh_thingy52_rgb_msg rgb_message;

	rgb_message.ttl = net_buf_simple_pull_le16(buf);
	rgb_message.delay = net_buf_simple_pull_le16(buf);
	rgb_message.color.red = net_buf_simple_pull_u8(buf);
	rgb_message.color.green = net_buf_simple_pull_u8(buf);
	rgb_message.color.blue = net_buf_simple_pull_u8(buf);
	rgb_message.speaker_on = net_buf_simple_pull_u8(buf);

	srv->msg_callbacks->rgb_set_handler(srv, ctx, rgb_message);
}

const struct bt_mesh_model_op _bt_mesh_thingy52_srv_op[] = {
	{ BT_MESH_THINGY52_OP_RGB_SET, BT_MESH_THINGY52_MSG_LEN_RGB_SET,
	  handle_rgb_set },
};

static int bt_mesh_thingy52_srv_init(struct bt_mesh_model *model)
{
	struct bt_mesh_thingy52_srv *srv = model->user_data;

	srv->model = model;
	net_buf_simple_init(model->pub->msg, 0);
	return 0;
}

int bt_mesh_thingy52_srv_rgb_set(struct bt_mesh_thingy52_srv *srv,
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
	return model_send(srv->model, ctx, &msg);
}

const struct bt_mesh_model_cb _bt_mesh_thingy52_srv_cb = {
	.init = bt_mesh_thingy52_srv_init
};
