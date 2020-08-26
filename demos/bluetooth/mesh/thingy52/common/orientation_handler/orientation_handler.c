/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdio.h>
#include <stdlib.h>
#include <device.h>
#include <drivers/sensor.h>
#include <orientation_handler.h>

uint8_t orientation_get(struct device *dev)
{
	if (dev != NULL) {
		struct sensor_value accel[3];
		int rc = sensor_sample_fetch(dev);

		if (rc == 0) {
			rc = sensor_channel_get(dev, SENSOR_CHAN_ACCEL_XYZ,
						accel);
		}

		if (rc == 0) {
			uint8_t orr = THINGY_ORIENT_NONE;
			int32_t top_val = 0;

			for (size_t i = 0; i < 3; i++) {
				if (abs(accel[i].val1) > abs(top_val)) {
					top_val = accel[i].val1;
					if (top_val < 0) {
						orr = i + 3;
					} else {
						orr = i;
					}
				}
			}

			return orr;
		} else {
			return THINGY_ORIENT_NONE;
		}
	} else {
		printk("Device %s is not initiated\n", dev->name);
		return THINGY_ORIENT_NONE;
	}
}
