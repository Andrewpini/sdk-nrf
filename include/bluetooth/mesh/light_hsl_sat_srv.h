/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

/**
 * @file
 * @defgroup bt_mesh_light_hsl_sat_srv Light HSL Saturation Server model
 * @{
 * @brief API for the Light HSL Saturation Server model.
 */

#ifndef BT_MESH_LIGHT_HSL_SAT_SRV_H__
#define BT_MESH_LIGHT_HSL_SAT_SRV_H__

#include <bluetooth/mesh/light_hsl.h>
#include <bluetooth/mesh/model_types.h>
#include <bluetooth/mesh/gen_lvl_srv.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_mesh_light_hsl_sat_srv;

/** @def BT_MESH_LIGHT_HSL_SAT_SRV_INIT
 *
 * @brief Initialization parameters for a @ref bt_mesh_light_hsl_sat_srv
 * instance.
 *
 * @param[in] _handlers HSL Saturation Server callbacks.
 */
#define BT_MESH_LIGHT_HSL_SAT_SRV_INIT(_handlers)                                    \
	{                                                                      \
		.handlers = _handlers,                                         \
		.lvl = BT_MESH_LVL_SRV_INIT(                                   \
			&_bt_mesh_light_hsl_sat_srv_lvl_handlers),                   \
		.pub = { .msg = NET_BUF_SIMPLE(BT_MESH_MODEL_BUF_LEN(          \
				 BT_MESH_LIGHT_HSL_SATURATION_STATUS,                \
				 BT_MESH_LIGHT_HSL_MSG_MAXLEN_HUE_STATUS)) },        \
		.range = { .min = 0, .max = 0xFFFF },                          \
	}

/** @def BT_MESH_MODEL_HSL_SAT_SRV
 *
 * @brief HSL Saturation Server model composition data entry.
 *
 * @param[in] _srv Pointer to a @ref bt_mesh_light_hsl_sat_srv instance.
 */
#define BT_MESH_MODEL_HSL_SAT_SRV(_srv)                                        \
	BT_MESH_MODEL_LVL_SRV(&(_srv)->lvl),                                   \
		BT_MESH_MODEL_CB(BT_MESH_MODEL_ID_LIGHT_HSL_SAT_SRV,           \
				 _bt_mesh_light_hsl_sat_srv_op, &(_srv)->pub,        \
				 BT_MESH_MODEL_USER_DATA(                      \
					 struct bt_mesh_light_hsl_sat_srv, _srv),    \
				 &_bt_mesh_light_hsl_sat_srv_cb)

/** HSL Saturation Server state access handlers. */
struct bt_mesh_light_hsl_sat_srv_handlers {
	/** @brief Set the Saturation state.
	 *
	 * @note This handler is mandatory.
	 *
	 * @param[in] srv Server to set the Saturation state of.
	 * @param[in] ctx Message context.
	 * @param[in] set Parameters of the state change.
	 * @param[out] rsp Response structure to be filled.
	 */
	void (*const set)(struct bt_mesh_light_hsl_sat_srv *srv,
			  struct bt_mesh_msg_ctx *ctx,
			  const struct bt_mesh_light_hsl_hue_set *set,
			  struct bt_mesh_light_hsl_hue_status *rsp);

	/** @brief Get the Saturation state.
	 *
	 * @note This handler is mandatory.
	 *
	 * @param[in] srv Server to get the CTL state of.
	 * @param[in] ctx Message context.
	 * @param[out] rsp Response structure to be filled.
	 */
	void (*const get)(struct bt_mesh_light_hsl_sat_srv *srv,
			  struct bt_mesh_msg_ctx *ctx,
			  struct bt_mesh_light_hsl_hue_status *rsp);
};

/**
 * HSL Saturation Server instance. Should be initialized with
 * @ref BT_MESH_LIGHT_HSL_SAT_SRV_INIT.
 */
struct bt_mesh_light_hsl_sat_srv {
	/** Light Level Server instance. */
	struct bt_mesh_lvl_srv lvl;
	/** Model entry. */
	struct bt_mesh_model *model;
	/** Publish parameters. */
	struct bt_mesh_model_pub pub;
	/** Transaction ID tracker for the set messages. */
	struct bt_mesh_tid_ctx prev_transaction;
	/** Acknowledged message tracking. */
	struct bt_mesh_model_ack_ctx ack_ctx;
	/** Handler function structure. */
	const struct bt_mesh_light_hsl_sat_srv_handlers *handlers;
	/** Saturation range */
	struct bt_mesh_light_hsl_range range;
	/** Last known Saturation level */
	uint16_t satur_last;
	/** Default Saturation level */
	uint16_t satur_default;
};

/** @brief Publish the current HSL Saturation status.
 *
 * Asynchronously publishes a HSL Saturation status message with the configured
 * publish parameters.
 *
 * @note This API is only used publishing unprompted status messages. Response
 * messages for get and set messages are handled internally.
 *
 * @param[in] srv Server instance to publish on.
 * @param[in] ctx Message context to send with, or NULL to send with the
 * default publish parameters.
 * @param[in] status Status parameters.
 *
 * @retval 0 Successfully sent the message.
 * @retval -ENOTSUP A message context was not provided and publishing is not
 * supported.
 * @retval -EADDRNOTAVAIL A message context was not provided and publishing is
 * not configured.
 * @retval -EAGAIN The device has not been provisioned.
 */
int32_t bt_mesh_light_hsl_sat_srv_pub(struct bt_mesh_light_hsl_sat_srv *srv,
				struct bt_mesh_msg_ctx *ctx,
				struct bt_mesh_light_hsl_hue_status *status);

/** @cond INTERNAL_HIDDEN */
extern const struct bt_mesh_model_op _bt_mesh_light_hsl_sat_srv_op[];
extern const struct bt_mesh_model_cb _bt_mesh_light_hsl_sat_srv_cb;
extern const struct bt_mesh_lvl_srv_handlers _bt_mesh_light_hsl_sat_srv_lvl_handlers;
/** @endcond */

#ifdef __cplusplus
}
#endif

#endif /* BT_MESH_LIGHT_HSL_SAT_SRV_H__ */

/* @} */
