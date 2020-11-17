/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

/**
 * @file
 * @defgroup bt_mesh_light_hsl_srv Light HSL Server model
 * @{
 * @brief API for the Light HSL Server model.
 */

#ifndef BT_MESH_LIGHT_HSL_SRV_H__
#define BT_MESH_LIGHT_HSL_SRV_H__

#include <bluetooth/mesh/light_hsl.h>
#include <bluetooth/mesh/light_hsl_sat_srv.h>
#include <bluetooth/mesh/light_hsl_hue_srv.h>
#include <bluetooth/mesh/lightness_srv.h>
#include <bluetooth/mesh/model_types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_mesh_light_hsl_srv;

/** @def BT_MESH_LIGHT_HSL_SRV_INIT
 *
 * @brief Initialization parameters for a @ref bt_mesh_light_hsl_srv instance.
 *
 * @param[in] _handlers Light HSL server callbacks.
 */
#define BT_MESH_LIGHT_HSL_SRV_INIT(_handlers)                                  \
	{                                                                      \
		.handlers = _handlers,                                         \
		.lightness_srv = BT_MESH_LIGHTNESS_SRV_INIT(                   \
			&_bt_mesh_light_hsl_lightness_handlers),               \
		.sat_srv = BT_MESH_LIGHT_HSL_SAT_SRV_INIT(                     \
			&_bt_mesh_light_hsl_sat_srv_handlers),                 \
		.sat_srv = BT_MESH_LIGHT_HSL_HUE_SRV_INIT(                     \
			&_bt_mesh_light_hsl_hue_srv_handlers),                 \
		.pub = { .msg = NET_BUF_SIMPLE(BT_MESH_MODEL_BUF_LEN(          \
				 BT_MESH_LIGHT_HSL_RANGE_STATUS,               \
				 BT_MESH_LIGHT_HSL_MSG_LEN_RANGE_STATUS)) },   \
	}

/** @def BT_MESH_MODEL_LIGHT_HSL_SRV
 *
 * @brief Light HSL Server model composition data entry.
 *
 * @param[in] _srv Pointer to a @ref bt_mesh_light_hsl_srv instance.
 */
#define BT_MESH_MODEL_LIGHT_HSL_SRV(_srv)                                      \
	BT_MESH_MODEL_LIGHTNESS_SRV(&(_srv)->lightness_srv),                   \
	BT_MESH_MODEL_CB(BT_MESH_MODEL_ID_LIGHT_HSL_SRV,                       \
			 _bt_mesh_light_hsl_srv_op, &(_srv)->pub,              \
			 BT_MESH_MODEL_USER_DATA(struct bt_mesh_light_hsl_srv, \
						 _srv),                        \
			 &_bt_mesh_light_hsl_srv_cb),                          \
	BT_MESH_MODEL_CB(BT_MESH_MODEL_ID_LIGHT_HSL_SETUP_SRV,                 \
			 _bt_mesh_light_hsl_setup_srv_op, &(_srv)->setup_pub,  \
			 BT_MESH_MODEL_USER_DATA(struct bt_mesh_light_hsl_srv, \
						 _srv),                        \
			 NULL)

/** Light HSL Server state access handlers. */
struct bt_mesh_light_hsl_srv_handlers {
	/** @brief The Range state has changed.
	 *
	 * @param[in] srv Server the Range state was changed on.
	 * @param[in] ctx Context of the set message that triggered the update.
	 * @param[in] set The new Range.
	 */
	void (*const range_update)(
		struct bt_mesh_light_hsl_srv *srv, struct bt_mesh_msg_ctx *ctx,
		const struct bt_mesh_light_hsl_range_set *set);

	/** @brief The Default Parameter state has changed.
	 *
	 * @param[in] srv Server the Default Parameter state was changed on.
	 * @param[in] ctx Context of the set message that triggered the update.
	 * @param[in] set The new Default Parameters.
	 */
	void (*const default_update)(struct bt_mesh_light_hsl_srv *srv,
				     struct bt_mesh_msg_ctx *ctx,
				     const struct bt_mesh_light_hsl *set);

	/** @brief The Light Range state has changed.
	 *
	 * @param[in] srv Server the Light Range state was changed on.
	 * @param[in] ctx Context of the set message that triggered the update,
	 * or NULL if it was not triggered by a message.
	 * @param[in] old_range The Light Range before the change.
	 * @param[in] new_range The Light Range after the change.
	 */
	void (*const lightness_range_update)(
		struct bt_mesh_light_hsl_srv *srv, struct bt_mesh_msg_ctx *ctx,
		const struct bt_mesh_lightness_range *old_range,
		const struct bt_mesh_lightness_range *new_range);
};

/**
 * Light HSL Server instance. Should be initialized with
 * @ref BT_MESH_LIGHT_HSL_SRV_INIT.
 */
struct bt_mesh_light_hsl_srv {
	/** Model entry. */
	struct bt_mesh_model *model;
	/** Light HSL Saturation Server instance. */
	struct bt_mesh_light_hsl_sat_srv sat_srv;
	/** Light HSL Hue Server instance. */
	struct bt_mesh_light_hsl_hue_srv hue_srv;
	/** Lightness Server instance. */
	struct bt_mesh_lightness_srv lightness_srv;
	/** Publish parameters. */
	struct bt_mesh_model_pub pub;
	/** Transaction ID tracker for the set messages. */
	struct bt_mesh_tid_ctx prev_transaction;
	/** Setup model publish parameters */
	struct bt_mesh_model_pub setup_pub;
	/** Acknowledged message tracking. */
	struct bt_mesh_model_ack_ctx ack_ctx;
	/** Handler function structure. */
	const struct bt_mesh_light_hsl_srv_handlers *handlers;
	/** Scene entry */
	struct bt_mesh_scene_entry scene;
};

/** @brief Publish the current HSL status.
 *
 * Asynchronously publishes a HSL status message with the configured
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
int32_t bt_mesh_light_hsl_srv_pub(struct bt_mesh_light_hsl_srv *srv,
				struct bt_mesh_msg_ctx *ctx,
				struct bt_mesh_light_hsl_status *status);

/** @brief Publish the current HSL Target status.
 *
 * Asynchronously publishes a HSL Target status message with the configured
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
int32_t bt_mesh_light_hsl_srv_target_pub(struct bt_mesh_light_hsl_srv *srv,
				struct bt_mesh_msg_ctx *ctx,
				struct bt_mesh_light_hsl_status *status);

/** @brief Publish the current HSL Default status.
 *
 * Asynchronously publishes a HSL Default status message with the configured
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
int32_t bt_mesh_light_hsl_default_pub(struct bt_mesh_light_hsl_srv *srv,
				struct bt_mesh_msg_ctx *ctx);

/** @brief Publish the current HSL Range status.
 *
 * Asynchronously publishes a HSL Range status message with the configured
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
int32_t bt_mesh_light_hsl_srv_range_pub(struct bt_mesh_light_hsl_srv *srv,
				struct bt_mesh_msg_ctx *ctx,
				struct bt_mesh_light_hsl_range_status *status);

/** @cond INTERNAL_HIDDEN */
extern const struct bt_mesh_model_op _bt_mesh_light_hsl_srv_op[];
extern const struct bt_mesh_model_op _bt_mesh_light_hsl_setup_srv_op[];
extern const struct bt_mesh_model_cb _bt_mesh_light_hsl_srv_cb;
extern const struct bt_mesh_light_hsl_sat_srv_handlers
	_bt_mesh_light_hsl_sat_srv_handlers;
extern const struct bt_mesh_light_hsl_hue_srv_handlers
	_bt_mesh_light_hsl_hue_srv_handlers;
extern const struct bt_mesh_lightness_srv_handlers
	_bt_mesh_light_hsl_lightness_handlers;

/** @endcond */

#ifdef __cplusplus
}
#endif

#endif /* BT_MESH_LIGHT_HSL_SRV_H__ */

/* @} */
