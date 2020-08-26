/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

/** @file
 *  @defgroup bt_mesh_thingy52 Thingy52 models
 *  @{
 *  @brief API for the Thingy52 models.
 */

#ifndef BT_MESH_THINGY52_H__
#define BT_MESH_THINGY52_H__

#include <bluetooth/mesh/model_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** RGB colors */
struct bt_mesh_thingy52_rgb {
	/** Color Red */
	uint8_t red;
	/** Color Green */
	uint8_t green;
	/** Color Blue */
	uint8_t blue;
};

/** Thingy52 RGB message */
struct bt_mesh_thingy52_rgb_msg {
	/** Remaining hops of the message */
	uint16_t ttl;
	/** Delay assosiated with the message */
	uint16_t delay;
	/** Color of the message. */
	struct bt_mesh_thingy52_rgb color;
	/** Speaker status of the message. */
	bool speaker_on;
};

/** @cond INTERNAL_HIDDEN */

#define BT_MESH_NORDIC_SEMI_COMPANY_ID 0x0059
#define BT_MESH_MODEL_ID_THINGY52_SRV 0x0005
#define BT_MESH_MODEL_ID_THINGY52_CLI 0x0006

#define BT_MESH_THINGY52_OP_RGB_SET                                            \
	BT_MESH_MODEL_OP_3(0x83, BT_MESH_NORDIC_SEMI_COMPANY_ID)

#define BT_MESH_THINGY52_MSG_LEN_RGB_SET 7

/** @endcond */

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* BT_MESH_THINGY52_H__ */
