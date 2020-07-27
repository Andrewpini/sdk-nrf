/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef MODEL_HANDLER_H__
#define MODEL_HANDLER_H__

#include <bluetooth/mesh.h>
#include <nrfx_pdm.h>
#include "fft_analyzer.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GREEN_LED 5
#define BLUE_LED 6
#define RED_LED 7
#define MIC_PWR 9
#define SPKR_PWR 29

const struct bt_mesh_comp *model_handler_init(void);

#ifdef __cplusplus
}
#endif

#endif /* MODEL_HANDLER_H__ */
