/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <bluetooth/mesh/light_hsl_cli.h>
#include "model_utils.h"

static void status_decode(struct bt_mesh_light_hsl_cli *cli,
			  struct bt_mesh_msg_ctx *ctx,
			  struct net_buf_simple *buf, uint32_t opcode,
			  struct bt_mesh_light_hsl_status *status)
{
	if (buf->len != BT_MESH_LIGHT_HSL_MSG_MINLEN_STATUS &&
	    buf->len != BT_MESH_LIGHT_HSL_MSG_MAXLEN_STATUS) {
		return;
	}

	status->params.lightness = net_buf_simple_pull_le16(buf);
	status->params.hue = net_buf_simple_pull_le16(buf);
	status->params.saturation = net_buf_simple_pull_le16(buf);
	status->remaining_time =
		(buf->len == 1) ?
			model_transition_decode(net_buf_simple_pull_u8(buf)) :
			0;

	if (model_ack_match(&cli->ack_ctx, opcode, ctx)) {
		struct bt_mesh_light_hsl_status *rsp =
			(struct bt_mesh_light_hsl_status *)
				cli->ack_ctx.user_data;
		*rsp = *status;
		model_ack_rx(&cli->ack_ctx);
	}
}

static void hue_status_decode(struct bt_mesh_light_hsl_cli *cli,
			  struct bt_mesh_msg_ctx *ctx,
			  struct net_buf_simple *buf, uint32_t opcode,
			  struct bt_mesh_light_hsl_hue_status *status)
{
	if (buf->len != BT_MESH_LIGHT_HSL_MSG_MINLEN_HUE_STATUS &&
	    buf->len != BT_MESH_LIGHT_HSL_MSG_MAXLEN_HUE_STATUS) {
		return;
	}

	status->current = net_buf_simple_pull_le16(buf);
	status->target = net_buf_simple_pull_le16(buf);
	status->remaining_time =
		(buf->len == 1) ?
			model_transition_decode(net_buf_simple_pull_u8(buf)) :
			0;

	if (model_ack_match(&cli->ack_ctx, opcode, ctx)) {
		struct bt_mesh_light_hsl_hue_status *rsp =
			(struct bt_mesh_light_hsl_hue_status *)
				cli->ack_ctx.user_data;
		*rsp = *status;
		model_ack_rx(&cli->ack_ctx);
	}
}

// static void xyl_status_handle(struct bt_mesh_model *model,
// 			      struct bt_mesh_msg_ctx *ctx,
// 			      struct net_buf_simple *buf)
// {
// 	struct bt_mesh_light_xyl_cli *cli = model->user_data;
// 	struct bt_mesh_light_xyl_status status;

// 	status_decode(cli, ctx, buf, BT_MESH_LIGHT_XYL_STATUS, &status);

// 	if (cli->handlers->xyl_status) {
// 		cli->handlers->xyl_status(cli, ctx, &status);
// 	}
// }

// static void target_status_handle(struct bt_mesh_model *model,
// 				 struct bt_mesh_msg_ctx *ctx,
// 				 struct net_buf_simple *buf)
// {
// 	struct bt_mesh_light_xyl_cli *cli = model->user_data;
// 	struct bt_mesh_light_xyl_status status;

// 	status_decode(cli, ctx, buf, BT_MESH_LIGHT_XYL_TARGET_STATUS, &status);

// 	if (cli->handlers->target_status) {
// 		cli->handlers->target_status(cli, ctx, &status);
// 	}
// }

// static void default_status_handle(struct bt_mesh_model *model,
// 				  struct bt_mesh_msg_ctx *ctx,
// 				  struct net_buf_simple *buf)
// {
// 	if (buf->len != BT_MESH_LIGHT_XYL_MSG_LEN_DEFAULT) {
// 		return;
// 	}

// 	struct bt_mesh_light_xyl_cli *cli = model->user_data;
// 	struct bt_mesh_light_xyl status;

// 	status.lightness = net_buf_simple_pull_le16(buf);
// 	status.x = net_buf_simple_pull_le16(buf);
// 	status.y = net_buf_simple_pull_le16(buf);

// 	if (model_ack_match(&cli->ack_ctx, BT_MESH_LIGHT_XYL_DEFAULT_STATUS,
// 			    ctx)) {
// 		struct bt_mesh_light_xyl *rsp =
// 			(struct bt_mesh_light_xyl *)cli->ack_ctx.user_data;
// 		*rsp = status;
// 		model_ack_rx(&cli->ack_ctx);
// 	}

// 	if (cli->handlers->default_status) {
// 		cli->handlers->default_status(cli, ctx, &status);
// 	}
// }

// static void range_status_handle(struct bt_mesh_model *model,
// 				struct bt_mesh_msg_ctx *ctx,
// 				struct net_buf_simple *buf)
// {
// 	if (buf->len != BT_MESH_LIGHT_XYL_MSG_LEN_RANGE_STATUS) {
// 		return;
// 	}

// 	struct bt_mesh_light_xyl_cli *cli = model->user_data;
// 	struct bt_mesh_light_xyl_range_status status;

// 	status.status_code = net_buf_simple_pull_u8(buf);
// 	status.range.x.min = net_buf_simple_pull_le16(buf);
// 	status.range.x.max = net_buf_simple_pull_le16(buf);
// 	status.range.y.min = net_buf_simple_pull_le16(buf);
// 	status.range.y.max = net_buf_simple_pull_le16(buf);

// 	if (model_ack_match(&cli->ack_ctx, BT_MESH_LIGHT_XYL_RANGE_STATUS,
// 			    ctx)) {
// 		struct bt_mesh_light_xyl_range_status *rsp =
// 			(struct bt_mesh_light_xyl_range_status *)
// 				cli->ack_ctx.user_data;
// 		*rsp = status;
// 		model_ack_rx(&cli->ack_ctx);
// 	}

// 	if (cli->handlers->range_status) {
// 		cli->handlers->range_status(cli, ctx, &status);
// 	}
// }

static void hsl_status_handle(struct bt_mesh_model *model,
				  struct bt_mesh_msg_ctx *ctx,
				  struct net_buf_simple *buf)
{
	struct bt_mesh_light_hsl_cli *cli = model->user_data;
	struct bt_mesh_light_hsl_status status;

	status_decode(cli, ctx, buf, BT_MESH_LIGHT_HSL_STATUS, &status);

	if (cli->handlers->hsl_status) {
		cli->handlers->hsl_status(cli, ctx, &status);
	}
}

static void hsl_target_status_handle(struct bt_mesh_model *model,
				  struct bt_mesh_msg_ctx *ctx,
				  struct net_buf_simple *buf)
{
	struct bt_mesh_light_hsl_cli *cli = model->user_data;
	struct bt_mesh_light_hsl_status status;

	status_decode(cli, ctx, buf, BT_MESH_LIGHT_HSL_TARGET_STATUS, &status);

	if (cli->handlers->hsl_target_status) {
		cli->handlers->hsl_target_status(cli, ctx, &status);
	}
}

static void default_status_handle(struct bt_mesh_model *model,
				  struct bt_mesh_msg_ctx *ctx,
				  struct net_buf_simple *buf)
{
	if (buf->len != BT_MESH_LIGHT_HSL_DEFAULT_STATUS) {
		return;
	}

	struct bt_mesh_light_hsl_cli *cli = model->user_data;
	struct bt_mesh_light_hsl status;

	status.lightness = net_buf_simple_pull_le16(buf);
	status.hue = net_buf_simple_pull_le16(buf);
	status.saturation = net_buf_simple_pull_le16(buf);

	if (model_ack_match(&cli->ack_ctx, BT_MESH_LIGHT_HSL_DEFAULT_STATUS,
			    ctx)) {
		struct bt_mesh_light_hsl *rsp =
			(struct bt_mesh_light_hsl *)cli->ack_ctx.user_data;
		*rsp = status;
		model_ack_rx(&cli->ack_ctx);
	}

	if (cli->handlers->default_status) {
		cli->handlers->default_status(cli, ctx, &status);
	}
}

static void range_status_handle(struct bt_mesh_model *model,
				  struct bt_mesh_msg_ctx *ctx,
				  struct net_buf_simple *buf)
{
	if (buf->len != BT_MESH_LIGHT_HSL_MSG_LEN_RANGE_STATUS) {
		return;
	}

	struct bt_mesh_light_hsl_cli *cli = model->user_data;
	struct bt_mesh_light_hsl_range_status status;

	status.status_code = net_buf_simple_pull_u8(buf);
	status.hue.min = net_buf_simple_pull_le16(buf);
	status.hue.max = net_buf_simple_pull_le16(buf);
	status.saturation.min = net_buf_simple_pull_le16(buf);
	status.saturation.max = net_buf_simple_pull_le16(buf);

	if (model_ack_match(&cli->ack_ctx, BT_MESH_LIGHT_HSL_MSG_LEN_RANGE_STATUS,
			    ctx)) {
		struct bt_mesh_light_hsl_range_status *rsp =
			(struct bt_mesh_light_hsl_range_status *)
				cli->ack_ctx.user_data;
		*rsp = status;
		model_ack_rx(&cli->ack_ctx);
	}

	if (cli->handlers->range_status) {
		cli->handlers->range_status(cli, ctx, &status);
	}
}

static void hue_status_handle(struct bt_mesh_model *model,
				  struct bt_mesh_msg_ctx *ctx,
				  struct net_buf_simple *buf)
{
	struct bt_mesh_light_hsl_cli *cli = model->user_data;
	struct bt_mesh_light_hsl_hue_status status;

	hue_status_decode(cli, ctx, buf, BT_MESH_LIGHT_HSL_HUE_STATUS, &status);

	if (cli->handlers->hue_status) {
		cli->handlers->hue_status(cli, ctx, &status);
	}
}

static void saturation_status_handle(struct bt_mesh_model *model,
				  struct bt_mesh_msg_ctx *ctx,
				  struct net_buf_simple *buf)
{
	struct bt_mesh_light_hsl_cli *cli = model->user_data;
	struct bt_mesh_light_hsl_hue_status status;

	hue_status_decode(cli, ctx, buf, BT_MESH_LIGHT_HSL_SATURATION_STATUS, &status);

	if (cli->handlers->saturation_status) {
		cli->handlers->saturation_status(cli, ctx, &status);
	}
}


const struct bt_mesh_model_op _bt_mesh_light_hsl_cli_op[] = {
	{ BT_MESH_LIGHT_HSL_STATUS, BT_MESH_LIGHT_HSL_MSG_MINLEN_STATUS,
	  hsl_status_handle },
	{ BT_MESH_LIGHT_HSL_TARGET_STATUS, BT_MESH_LIGHT_HSL_MSG_MINLEN_STATUS,
	  hsl_target_status_handle },
	{ BT_MESH_LIGHT_HSL_DEFAULT_STATUS, BT_MESH_LIGHT_HSL_MSG_LEN_DEFAULT,
	  default_status_handle },
	{ BT_MESH_LIGHT_HSL_RANGE_STATUS, BT_MESH_LIGHT_HSL_MSG_LEN_RANGE_STATUS,
	  range_status_handle },
	{ BT_MESH_LIGHT_HSL_HUE_STATUS, BT_MESH_LIGHT_HSL_MSG_MINLEN_HUE_STATUS,
	  hue_status_handle },
	{ BT_MESH_LIGHT_HSL_SATURATION_STATUS, BT_MESH_LIGHT_HSL_MSG_MINLEN_HUE_STATUS,
	  saturation_status_handle },
	BT_MESH_MODEL_OP_END,
};

static int bt_mesh_light_hsl_cli_init(struct bt_mesh_model *model)
{
	struct bt_mesh_light_hsl_cli *cli = model->user_data;

	cli->model = model;
	net_buf_simple_init(cli->pub.msg, 0);
	model_ack_init(&cli->ack_ctx);

	return 0;
}

const struct bt_mesh_model_cb _bt_mesh_light_hsl_cli_cb = {
	.init = bt_mesh_light_hsl_cli_init,
};

static int get_msg(struct bt_mesh_light_hsl_cli *cli,
		   struct bt_mesh_msg_ctx *ctx, void *rsp, uint16_t opcode,
		   uint16_t ret_opcode)
{
	BT_MESH_MODEL_BUF_DEFINE(msg, opcode, BT_MESH_LIGHT_HSL_MSG_LEN_GET);
	bt_mesh_model_msg_init(&msg, opcode);

	return model_ackd_send(cli->model, ctx, &msg,
			       rsp ? &cli->ack_ctx : NULL, ret_opcode, rsp);
}

int bt_mesh_light_hsl_get(struct bt_mesh_light_hsl_cli *cli,
			  struct bt_mesh_msg_ctx *ctx,
			  struct bt_mesh_light_hsl_status *rsp)
{
	return get_msg(cli, ctx, rsp, BT_MESH_LIGHT_HSL_GET,
		       BT_MESH_LIGHT_HSL_STATUS);
}

int bt_mesh_light_hsl_set(struct bt_mesh_light_hsl_cli *cli,
			  struct bt_mesh_msg_ctx *ctx,
			  const struct bt_mesh_light_hsl_set *set,
			  struct bt_mesh_light_hsl_status *rsp)
{
	BT_MESH_MODEL_BUF_DEFINE(msg, BT_MESH_LIGHT_HSL_SET,
				 BT_MESH_LIGHT_HSL_MSG_MAXLEN_SET);
	bt_mesh_model_msg_init(&msg, BT_MESH_LIGHT_HSL_SET);
	net_buf_simple_add_le16(&msg, set->params.lightness);
	net_buf_simple_add_le16(&msg, set->params.hue);
	net_buf_simple_add_le16(&msg, set->params.saturation);
	net_buf_simple_add_u8(&msg, cli->tid++);
	if (set->transition) {
		model_transition_buf_add(&msg, set->transition);
	}

	return model_ackd_send(cli->model, ctx, &msg,
			       rsp ? &cli->ack_ctx : NULL,
			       BT_MESH_LIGHT_HSL_STATUS, rsp);
}

int bt_mesh_light_hsl_set_unack(struct bt_mesh_light_hsl_cli *cli,
				struct bt_mesh_msg_ctx *ctx,
				const struct bt_mesh_light_hsl_set *set)
{
	BT_MESH_MODEL_BUF_DEFINE(msg, BT_MESH_LIGHT_HSL_SET_UNACK,
				 BT_MESH_LIGHT_HSL_MSG_MAXLEN_SET);
	bt_mesh_model_msg_init(&msg, BT_MESH_LIGHT_HSL_SET_UNACK);
	net_buf_simple_add_le16(&msg, set->params.lightness);
	net_buf_simple_add_le16(&msg, set->params.hue);
	net_buf_simple_add_le16(&msg, set->params.saturation);
	net_buf_simple_add_u8(&msg, cli->tid++);
	if (set->transition) {
		model_transition_buf_add(&msg, set->transition);
	}

	return model_send(cli->model, ctx, &msg);
}

int bt_mesh_light_hsl_target_get(struct bt_mesh_light_hsl_cli *cli,
				 struct bt_mesh_msg_ctx *ctx,
				 struct bt_mesh_light_hsl_status *rsp)
{
	return get_msg(cli, ctx, rsp, BT_MESH_LIGHT_HSL_TARGET_GET,
		       BT_MESH_LIGHT_HSL_TARGET_STATUS);
}

int bt_mesh_light_hsl_default_get(struct bt_mesh_light_hsl_cli *cli,
				  struct bt_mesh_msg_ctx *ctx,
				  struct bt_mesh_light_hsl *rsp)
{
	return get_msg(cli, ctx, rsp, BT_MESH_LIGHT_HSL_DEFAULT_GET,
		       BT_MESH_LIGHT_HSL_DEFAULT_STATUS);
}

int bt_mesh_light_hsl_default_set(struct bt_mesh_light_hsl_cli *cli,
				  struct bt_mesh_msg_ctx *ctx,
				  const struct bt_mesh_light_hsl *set,
				  struct bt_mesh_light_hsl *rsp)
{
	BT_MESH_MODEL_BUF_DEFINE(msg, BT_MESH_LIGHT_HSL_DEFAULT_SET,
				 BT_MESH_LIGHT_HSL_MSG_LEN_DEFAULT);
	bt_mesh_model_msg_init(&msg, BT_MESH_LIGHT_HSL_DEFAULT_SET);
	net_buf_simple_add_le16(&msg, set->lightness);
	net_buf_simple_add_le16(&msg, set->hue);
	net_buf_simple_add_le16(&msg, set->saturation);

	return model_ackd_send(cli->model, ctx, &msg,
			       rsp ? &cli->ack_ctx : NULL,
			       BT_MESH_LIGHT_HSL_DEFAULT_STATUS, rsp);
}

int bt_mesh_light_hsl_default_set_unack(struct bt_mesh_light_hsl_cli *cli,
					struct bt_mesh_msg_ctx *ctx,
					const struct bt_mesh_light_hsl *set)
{
	BT_MESH_MODEL_BUF_DEFINE(msg, BT_MESH_LIGHT_HSL_DEFAULT_SET_UNACK,
				 BT_MESH_LIGHT_HSL_MSG_LEN_DEFAULT);
	bt_mesh_model_msg_init(&msg, BT_MESH_LIGHT_HSL_DEFAULT_SET_UNACK);
	net_buf_simple_add_le16(&msg, set->lightness);
	net_buf_simple_add_le16(&msg, set->hue);
	net_buf_simple_add_le16(&msg, set->saturation);

	return model_send(cli->model, ctx, &msg);
}

int bt_mesh_light_hsl_range_get(struct bt_mesh_light_hsl_cli *cli,
				struct bt_mesh_msg_ctx *ctx,
				struct bt_mesh_light_hsl_range_status *rsp)
{
	return get_msg(cli, ctx, rsp, BT_MESH_LIGHT_HSL_RANGE_GET,
		       BT_MESH_LIGHT_HSL_RANGE_STATUS);
}

int bt_mesh_light_hsl_range_set(struct bt_mesh_light_hsl_cli *cli,
				struct bt_mesh_msg_ctx *ctx,
				const struct bt_mesh_light_hsl_range_set *set,
				struct bt_mesh_light_hsl_range_status *rsp)
{
	BT_MESH_MODEL_BUF_DEFINE(msg, BT_MESH_LIGHT_HSL_RANGE_SET,
				 BT_MESH_LIGHT_HSL_MSG_LEN_RANGE_SET);
	bt_mesh_model_msg_init(&msg, BT_MESH_LIGHT_HSL_RANGE_SET);
	net_buf_simple_add_le16(&msg, set->hue.min);
	net_buf_simple_add_le16(&msg, set->hue.max);
	net_buf_simple_add_le16(&msg, set->saturation.min);
	net_buf_simple_add_le16(&msg, set->saturation.max);

	return model_ackd_send(cli->model, ctx, &msg,
			       rsp ? &cli->ack_ctx : NULL,
			       BT_MESH_LIGHT_HSL_RANGE_STATUS, rsp);
}

int bt_mesh_light_hsl_range_set_unack(struct bt_mesh_light_hsl_cli *cli,
				      struct bt_mesh_msg_ctx *ctx,
				      const struct bt_mesh_light_hsl_range_set *set)
{
	BT_MESH_MODEL_BUF_DEFINE(msg, BT_MESH_LIGHT_HSL_RANGE_SET_UNACK,
				 BT_MESH_LIGHT_HSL_MSG_LEN_RANGE_SET);
	bt_mesh_model_msg_init(&msg, BT_MESH_LIGHT_HSL_RANGE_SET_UNACK);
	net_buf_simple_add_le16(&msg, set->hue.min);
	net_buf_simple_add_le16(&msg, set->hue.max);
	net_buf_simple_add_le16(&msg, set->saturation.min);
	net_buf_simple_add_le16(&msg, set->saturation.max);

	return model_send(cli->model, ctx, &msg);
}

int bt_mesh_light_hsl_hue_get(struct bt_mesh_light_hsl_cli *cli,
			      struct bt_mesh_msg_ctx *ctx,
			      struct bt_mesh_light_hsl_hue_status *rsp)
{
	return get_msg(cli, ctx, rsp, BT_MESH_LIGHT_HSL_HUE_GET,
		       BT_MESH_LIGHT_HSL_HUE_STATUS);
}

int bt_mesh_light_hsl_hue_set(struct bt_mesh_light_hsl_cli *cli,
			      struct bt_mesh_msg_ctx *ctx,
			      const struct bt_mesh_light_hsl_hue_set *set,
			      struct bt_mesh_light_hsl_hue_status *rsp)
{
	BT_MESH_MODEL_BUF_DEFINE(msg, BT_MESH_LIGHT_HSL_HUE_SET,
				 BT_MESH_LIGHT_HSL_MSG_MAXLEN_HUE);
	bt_mesh_model_msg_init(&msg, BT_MESH_LIGHT_HSL_HUE_SET);
	net_buf_simple_add_le16(&msg, set->level);
	net_buf_simple_add_u8(&msg, cli->tid++);
	if (set->transition) {
		model_transition_buf_add(&msg, set->transition);
	}

	return model_ackd_send(cli->model, ctx, &msg,
			       rsp ? &cli->ack_ctx : NULL,
			       BT_MESH_LIGHT_HSL_HUE_STATUS, rsp);
}

int bt_mesh_light_hsl_hue_set_unack(struct bt_mesh_light_hsl_cli *cli,
				    struct bt_mesh_msg_ctx *ctx,
				    const struct bt_mesh_light_hsl_hue_set *set)
{
	BT_MESH_MODEL_BUF_DEFINE(msg, BT_MESH_LIGHT_HSL_HUE_SET_UNACK,
				 BT_MESH_LIGHT_HSL_MSG_MAXLEN_HUE);
	bt_mesh_model_msg_init(&msg, BT_MESH_LIGHT_HSL_HUE_SET_UNACK);
	net_buf_simple_add_le16(&msg, set->level);
	net_buf_simple_add_u8(&msg, cli->tid++);
	if (set->transition) {
		model_transition_buf_add(&msg, set->transition);
	}

	return model_send(cli->model, ctx, &msg);
}

int bt_mesh_light_hsl_saturation_get(struct bt_mesh_light_hsl_cli *cli,
				     struct bt_mesh_msg_ctx *ctx,
				     struct bt_mesh_light_hsl_hue_status *rsp)
{
	return get_msg(cli, ctx, rsp, BT_MESH_LIGHT_HSL_SATURATION_GET,
		       BT_MESH_LIGHT_HSL_SATURATION_STATUS);
}

int bt_mesh_light_hsl_saturation_set(struct bt_mesh_light_hsl_cli *cli,
				     struct bt_mesh_msg_ctx *ctx,
				     const struct bt_mesh_light_hsl_hue_set *set,
				     struct bt_mesh_light_hsl_hue_status *rsp)
{
	BT_MESH_MODEL_BUF_DEFINE(msg, BT_MESH_LIGHT_HSL_SATURATION_SET,
				 BT_MESH_LIGHT_HSL_MSG_MAXLEN_HUE);
	bt_mesh_model_msg_init(&msg, BT_MESH_LIGHT_HSL_SATURATION_SET);
	net_buf_simple_add_le16(&msg, set->level);
	net_buf_simple_add_u8(&msg, cli->tid++);
	if (set->transition) {
		model_transition_buf_add(&msg, set->transition);
	}

	return model_ackd_send(cli->model, ctx, &msg,
			       rsp ? &cli->ack_ctx : NULL,
			       BT_MESH_LIGHT_HSL_SATURATION_STATUS, rsp);
}

int bt_mesh_light_hsl_saturation_set_unack(struct bt_mesh_light_hsl_cli *cli,
					   struct bt_mesh_msg_ctx *ctx,
					   const struct bt_mesh_light_hsl_hue_set *set)
{
	BT_MESH_MODEL_BUF_DEFINE(msg, BT_MESH_LIGHT_HSL_SATURATION_SET_UNACK,
				 BT_MESH_LIGHT_HSL_MSG_MAXLEN_HUE);
	bt_mesh_model_msg_init(&msg, BT_MESH_LIGHT_HSL_SATURATION_SET_UNACK);
	net_buf_simple_add_le16(&msg, set->level);
	net_buf_simple_add_u8(&msg, cli->tid++);
	if (set->transition) {
		model_transition_buf_add(&msg, set->transition);
	}

	return model_send(cli->model, ctx, &msg);
}
