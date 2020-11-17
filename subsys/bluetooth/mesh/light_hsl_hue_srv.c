/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdlib.h>
#include <bluetooth/mesh/light_hsl_hue_srv.h>
#include <bluetooth/mesh/gen_dtt_srv.h>
#include "light_hsl_internal.h"
#include "model_utils.h"

static void encode_status(struct net_buf_simple *buf,
			  struct bt_mesh_light_hsl_hue_status *status)
{
	bt_mesh_model_msg_init(buf, BT_MESH_LIGHT_HSL_HUE_STATUS);
	net_buf_simple_add_le16(buf, status->current);

	if (status->remaining_time != 0) {
		net_buf_simple_add_le16(buf, status->target);
		net_buf_simple_add_u8(
			buf, model_transition_encode(status->remaining_time));
	}
}

static void rsp_status(struct bt_mesh_model *model,
		       struct bt_mesh_msg_ctx *rx_ctx,
		       struct bt_mesh_light_hsl_hue_status *status)
{
	BT_MESH_MODEL_BUF_DEFINE(msg, BT_MESH_LIGHT_HSL_HUE_STATUS,
				 BT_MESH_LIGHT_HSL_MSG_MAXLEN_HUE_STATUS);
	encode_status(&msg, status);

	(void)bt_mesh_model_send(model, rx_ctx, &msg, NULL, NULL);
}

static void hue_set(struct bt_mesh_model *model,
			   struct bt_mesh_msg_ctx *ctx,
			   struct net_buf_simple *buf, bool ack)
{
	if (buf->len != BT_MESH_LIGHT_HSL_MSG_MINLEN_HUE &&
	    buf->len != BT_MESH_LIGHT_HSL_MSG_MAXLEN_HUE) {
		return;
	}

	struct bt_mesh_light_hsl_hue_srv *srv = model->user_data;
	struct bt_mesh_light_hsl_hue_set set;
	struct bt_mesh_light_hsl_hue_status status = { 0 };
	struct bt_mesh_model_transition transition;

	set_hue(srv, net_buf_simple_pull_le16(buf));
	set.level = set_hue(srv, net_buf_simple_pull_le16(buf));
	uint8_t tid = net_buf_simple_pull_u8(buf);

	if (tid_check_and_update(&srv->prev_transaction, tid, ctx) != 0) {
		/* If this is the same transaction, we don't need to send it
		 * to the app, but we still have to respond with a status.
		 */
		srv->handlers->get(srv, NULL, &status);
		goto respond;
	}

	if (buf->len == 2) {
		model_transition_buf_pull(buf, &transition);
	} else {
		bt_mesh_dtt_srv_transition_get(srv->model, &transition);
	}

	set.transition = &transition;
	srv->hue_last = set.level;
	srv->handlers->set(srv, ctx, &set, &status);

	if (IS_ENABLED(CONFIG_BT_MESH_SCENE_SRV)) {
		bt_mesh_scene_invalidate(&srv->lvl.scene);
	}

	struct bt_mesh_lvl_status lvl_status = {
		.current = status.current - 32768,
		.target = status.target - 32768,
		.remaining_time = status.remaining_time,
	};

	(void)bt_mesh_light_hsl_hue_srv_pub(srv, NULL, &status);
	(void)bt_mesh_lvl_srv_pub(&srv->lvl, NULL, &lvl_status);
respond:
	if (ack) {
		rsp_status(model, ctx, &status);
	}
}

static void hue_get_handle(struct bt_mesh_model *model,
				  struct bt_mesh_msg_ctx *ctx,
				  struct net_buf_simple *buf)
{
	if (buf->len != BT_MESH_LIGHT_HSL_MSG_LEN_GET) {
		return;
	}

	struct bt_mesh_light_hsl_hue_srv *srv = model->user_data;
	struct bt_mesh_light_hsl_hue_status status = { 0 };

	srv->handlers->get(srv, ctx, &status);
	rsp_status(model, ctx, &status);
}

static void hue_set_handle(struct bt_mesh_model *model,
				  struct bt_mesh_msg_ctx *ctx,
				  struct net_buf_simple *buf)
{
	hue_set(model, ctx, buf, true);
}

static void hue_set_unack_handle(struct bt_mesh_model *model,
					struct bt_mesh_msg_ctx *ctx,
					struct net_buf_simple *buf)
{
	hue_set(model, ctx, buf, false);
}

const struct bt_mesh_model_op _bt_mesh_light_hsl_hue_srv_op[] = {
	{ BT_MESH_LIGHT_HSL_HUE_GET, BT_MESH_LIGHT_HSL_MSG_LEN_GET,
	  hue_get_handle },
	{ BT_MESH_LIGHT_HSL_HUE_SET, BT_MESH_LIGHT_HSL_MSG_MINLEN_HUE,
	  hue_set_handle },
	{ BT_MESH_LIGHT_HSL_HUE_SET_UNACK, BT_MESH_LIGHT_HSL_MSG_MINLEN_HUE,
	  hue_set_unack_handle },
	BT_MESH_MODEL_OP_END,
};

static void lvl_get(struct bt_mesh_lvl_srv *lvl_srv,
		    struct bt_mesh_msg_ctx *ctx, struct bt_mesh_lvl_status *rsp)
{
	struct bt_mesh_light_hsl_hue_srv *srv =
		CONTAINER_OF(lvl_srv, struct bt_mesh_light_hsl_hue_srv, lvl);
	struct bt_mesh_light_hsl_hue_status status = { 0 };

	srv->handlers->get(srv, NULL, &status);

	rsp->current = HUE_TO_LVL(status.current);
	rsp->target = HUE_TO_LVL(status.target);
	rsp->remaining_time = status.remaining_time;
}

static void lvl_set(struct bt_mesh_lvl_srv *lvl_srv,
		    struct bt_mesh_msg_ctx *ctx,
		    const struct bt_mesh_lvl_set *lvl_set,
		    struct bt_mesh_lvl_status *rsp)
{
	struct bt_mesh_light_hsl_hue_srv *srv =
		CONTAINER_OF(lvl_srv, struct bt_mesh_light_hsl_hue_srv, lvl);
	struct bt_mesh_light_hsl_hue_set set;
	struct bt_mesh_light_hsl_hue_status status = { 0 };

	uint16_t hue = LVL_TO_HUE(lvl_set->lvl);

	set.level = hue;
	set.transition = lvl_set->transition;

	if (lvl_set->new_transaction) {
		srv->handlers->set(srv, NULL, &set, &status);
		srv->hue_last = hue;
	} else if (rsp) {
		srv->handlers->get(srv, NULL, &status);
	}

	(void)bt_mesh_light_hsl_hue_srv_pub(srv, NULL, &status);

	if (rsp) {
		rsp->current = HUE_TO_LVL(status.current);
		rsp->target = HUE_TO_LVL(status.target);
		rsp->remaining_time = status.remaining_time;
	}
}

static void lvl_delta_set(struct bt_mesh_lvl_srv *lvl_srv,
			  struct bt_mesh_msg_ctx *ctx,
			  const struct bt_mesh_lvl_delta_set *delta_set,
			  struct bt_mesh_lvl_status *rsp)
{
	struct bt_mesh_light_hsl_hue_srv *srv =
		CONTAINER_OF(lvl_srv, struct bt_mesh_light_hsl_hue_srv, lvl);
	struct bt_mesh_light_hsl_hue_set set;
	struct bt_mesh_light_hsl_hue_status status = { 0 };

	int16_t start_value = HUE_TO_LVL(srv->hue_last);

	if (delta_set->new_transaction) {
		srv->handlers->get(srv, NULL, &status);
		start_value = HUE_TO_LVL(status.current);
	}

	int16_t target_value = start_value + delta_set->delta;

	set.level = LVL_TO_HUE(target_value);
	set.transition = delta_set->transition;
	srv->handlers->set(srv, ctx, &set, &status);

	/* Override "hue_last" value to be able to make corrective deltas when
	 * new_transaction is false. Note that the "hue_last" value in
	 * persistent storage will still be the target value, allowing us to
	 * recover Scorrectly on power loss.
	 */
	srv->hue_last = LVL_TO_HUE(start_value);

	(void)bt_mesh_light_hsl_hue_srv_pub(srv, NULL, &status);

	if (rsp) {
		rsp->current = HUE_TO_LVL(status.current);
		rsp->target = HUE_TO_LVL(status.target);
		rsp->remaining_time = status.remaining_time;
	}
}

static void lvl_move_set(struct bt_mesh_lvl_srv *lvl_srv,
			 struct bt_mesh_msg_ctx *ctx,
			 const struct bt_mesh_lvl_move_set *move_set,
			 struct bt_mesh_lvl_status *rsp)
{
	struct bt_mesh_light_hsl_hue_srv *srv =
		CONTAINER_OF(lvl_srv, struct bt_mesh_light_hsl_hue_srv, lvl);
	struct bt_mesh_light_hsl_hue_status status = { 0 };

	srv->handlers->get(srv, NULL, &status);

	uint16_t target = status.current + LVL_TO_HUE(move_set->delta);

	struct bt_mesh_model_transition transition;
	struct bt_mesh_light_hsl_hue_set set = {
		.level = target,
		.transition = &transition,
	};

	if (move_set->delta != 0 && move_set->transition) {
		uint32_t distance = abs(target - status.current);
		uint32_t time_to_edge = ((uint64_t)distance *
					 (uint64_t)move_set->transition->time) /
					abs(move_set->delta);

		if (time_to_edge > 0) {
			transition.delay = move_set->transition->delay;
			transition.time = time_to_edge;
		}
	}

	srv->handlers->set(srv, ctx, &set, &status);

	if (rsp) {
		rsp->current = HUE_TO_LVL(status.current);
		rsp->target = HUE_TO_LVL(status.target);
		rsp->remaining_time = status.remaining_time;
	}
}

const struct bt_mesh_lvl_srv_handlers _bt_mesh_light_hsl_hue_srv_lvl_handlers = {
	.get = lvl_get,
	.set = lvl_set,
	.delta_set = lvl_delta_set,
	.move_set = lvl_move_set,
};

static int bt_mesh_light_hsl_hue_srv_init(struct bt_mesh_model *model)
{
	struct bt_mesh_light_hsl_hue_srv *srv = model->user_data;

	srv->model = model;
	net_buf_simple_init(srv->pub.msg, 0);

	if (IS_ENABLED(CONFIG_BT_MESH_MODEL_EXTENSIONS)) {
		bt_mesh_model_extend(model, srv->lvl.model);
	}

	return 0;
}

const struct bt_mesh_model_cb _bt_mesh_light_hsl_hue_srv_cb = {
	.init = bt_mesh_light_hsl_hue_srv_init,
};

int32_t bt_mesh_light_hsl_hue_srv_pub(struct bt_mesh_light_hsl_hue_srv *srv,
				struct bt_mesh_msg_ctx *ctx,
				struct bt_mesh_light_hsl_hue_status *status)
{
	BT_MESH_MODEL_BUF_DEFINE(msg, BT_MESH_LIGHT_HSL_HUE_STATUS,
				 BT_MESH_LIGHT_HSL_MSG_MAXLEN_HUE_STATUS);
	encode_status(&msg, status);
	return model_send(srv->model, ctx, &msg);
}
