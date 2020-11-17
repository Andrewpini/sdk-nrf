/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */
/**
 * @file
 * @defgroup bt_mesh_light_hsl Light HSL models
 * @{
 * @brief API for the Light HSL models.
 */

#ifndef BT_MESH_LIGHT_HSL_H__
#define BT_MESH_LIGHT_HSL_H__

#include <bluetooth/mesh.h>
#include <bluetooth/mesh/model_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Common Light HSL parameters */
struct bt_mesh_light_hsl {
	/** Lightness level */
	uint16_t lightness;
	/** HUE level */
	uint16_t hue;
	/** Saturation level */
	uint16_t saturation;
};

/** Common light HSL set message parameters. */
struct bt_mesh_light_hsl_set {
	/** HSL set parameters */
	struct bt_mesh_light_hsl params;
	/** Transition time parameters for the state change. */
	const struct bt_mesh_model_transition *transition;
};

/** Common light HSL status message parameters. */
struct bt_mesh_light_hsl_status {
	/** Status parameters */
	struct bt_mesh_light_hsl params;
	/** Remaining time for the state change (ms). */
	int32_t remaining_time;
};

/** Common light HSL Hue set message parameters. */
struct bt_mesh_light_hsl_hue_set {
	/** Level to set */
	uint16_t level;
	/** Transition time parameters for the state change. */
	const struct bt_mesh_model_transition *transition;
};

/** Common light HSL Hue status message parameters. */
struct bt_mesh_light_hsl_hue_status {
	/** Current level */
	uint16_t current;
	/** Target level */
	uint16_t target;
	/** Remaining time for the state change (ms). */
	int32_t remaining_time;
};

/** Light HSL range parameters. */
struct bt_mesh_light_hsl_range {
	/** Minimum range value */
	uint16_t min;
	/** Maximum range value */
	uint16_t max;
};

/** Light HSL range set message parameters. */
struct bt_mesh_light_hsl_range_set {
	/** Hue range */
	struct bt_mesh_light_hsl_range hue;
	/** Saturation range */
	struct bt_mesh_light_hsl_range saturation;
};

/** Light HSL range status message parameters. */
struct bt_mesh_light_hsl_range_status {
	/** Range set status code */
	enum bt_mesh_model_status status_code;
	/** Hue range */
	struct bt_mesh_light_hsl_range hue;
	/** Saturation range */
	struct bt_mesh_light_hsl_range saturation;
};

/** @cond INTERNAL_HIDDEN */
#define BT_MESH_LIGHT_HSL_GET BT_MESH_MODEL_OP_2(0x82, 0x6D)
#define BT_MESH_LIGHT_HSL_HUE_GET BT_MESH_MODEL_OP_2(0x82, 0x6E)
#define BT_MESH_LIGHT_HSL_HUE_SET BT_MESH_MODEL_OP_2(0x82, 0x6F)
#define BT_MESH_LIGHT_HSL_HUE_SET_UNACK BT_MESH_MODEL_OP_2(0x82, 0x70)
#define BT_MESH_LIGHT_HSL_HUE_STATUS BT_MESH_MODEL_OP_2(0x82, 0x71)
#define BT_MESH_LIGHT_HSL_SATURATION_GET BT_MESH_MODEL_OP_2(0x82, 0x72)
#define BT_MESH_LIGHT_HSL_SATURATION_SET BT_MESH_MODEL_OP_2(0x82, 0x73)
#define BT_MESH_LIGHT_HSL_SATURATION_SET_UNACK BT_MESH_MODEL_OP_2(0x82, 0x74)
#define BT_MESH_LIGHT_HSL_SATURATION_STATUS BT_MESH_MODEL_OP_2(0x82, 0x75)
#define BT_MESH_LIGHT_HSL_SET BT_MESH_MODEL_OP_2(0x82, 0x76)
#define BT_MESH_LIGHT_HSL_SET_UNACK BT_MESH_MODEL_OP_2(0x82, 0x77)
#define BT_MESH_LIGHT_HSL_STATUS BT_MESH_MODEL_OP_2(0x82, 0x78)
#define BT_MESH_LIGHT_HSL_TARGET_GET BT_MESH_MODEL_OP_2(0x82, 0x79)
#define BT_MESH_LIGHT_HSL_TARGET_STATUS BT_MESH_MODEL_OP_2(0x82, 0x7A)
#define BT_MESH_LIGHT_HSL_DEFAULT_GET BT_MESH_MODEL_OP_2(0x82, 0x7B)
#define BT_MESH_LIGHT_HSL_DEFAULT_STATUS BT_MESH_MODEL_OP_2(0x82, 0x7C)
#define BT_MESH_LIGHT_HSL_RANGE_GET BT_MESH_MODEL_OP_2(0x82, 0x7D)
#define BT_MESH_LIGHT_HSL_RANGE_STATUS BT_MESH_MODEL_OP_2(0x82, 0x7E)
#define BT_MESH_LIGHT_HSL_DEFAULT_SET BT_MESH_MODEL_OP_2(0x82, 0x7F)
#define BT_MESH_LIGHT_HSL_DEFAULT_SET_UNACK BT_MESH_MODEL_OP_2(0x82, 0x80)
#define BT_MESH_LIGHT_HSL_RANGE_SET BT_MESH_MODEL_OP_2(0x82, 0x81)
#define BT_MESH_LIGHT_HSL_RANGE_SET_UNACK BT_MESH_MODEL_OP_2(0x82, 0x82)

#define BT_MESH_LIGHT_HSL_MSG_LEN_GET 0
#define BT_MESH_LIGHT_HSL_MSG_MINLEN_SET 7
#define BT_MESH_LIGHT_HSL_MSG_MAXLEN_SET 9
#define BT_MESH_LIGHT_HSL_MSG_MINLEN_STATUS 6
#define BT_MESH_LIGHT_HSL_MSG_MAXLEN_STATUS 7
#define BT_MESH_LIGHT_HSL_MSG_MINLEN_HUE 3
#define BT_MESH_LIGHT_HSL_MSG_MAXLEN_HUE 5
#define BT_MESH_LIGHT_HSL_MSG_MINLEN_HUE_STATUS 2
#define BT_MESH_LIGHT_HSL_MSG_MAXLEN_HUE_STATUS 5
#define BT_MESH_LIGHT_HSL_MSG_LEN_DEFAULT 6
#define BT_MESH_LIGHT_HSL_MSG_LEN_RANGE_SET 8
#define BT_MESH_LIGHT_HSL_MSG_LEN_RANGE_STATUS 9

/** @endcond */

#ifdef __cplusplus
}
#endif

#endif /* BT_MESH_LIGHT_HSL_H__ */

/** @} */
