/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdio.h>
#include <zephyr.h>
#include <thingy52_orientation_handler.h>


#include <drivers/gpio.h>
#include <drivers/pwm.h>
#include <dk_buttons_and_leds.h>

#define GREEN_LED 5
#define BLUE_LED 6
#define RED_LED 7
#define MIC_PWR 9
#define SPKR_PWR 29

static struct device *io_expander;

static void led_init(void)
{
	int err;

	io_expander = device_get_binding(DT_PROP(DT_NODELABEL(sx1509b), label));

	if (io_expander == NULL) {
		printk("Could not initiate I/O expander\n");
	}

	err = gpio_pin_configure(io_expander, GREEN_LED, GPIO_OUTPUT);

	if (err) {
		printk("Could not configure green LED pin\n");
	}

	err = gpio_pin_configure(io_expander, BLUE_LED, GPIO_OUTPUT);

	if (err) {
		printk("Could not configure blue LED pin\n");
	}

	err = gpio_pin_configure(io_expander, RED_LED, GPIO_OUTPUT);

	if (err) {
		printk("Could not configure red LED pin\n");
	}
}

void main(void)
{
        led_init();
        thingy52_orientation_handler_init();
        gpio_pin_set_raw(io_expander, BLUE_LED, 0);
	while (true) {

                orientation_t orr = thingy52_orientation_get();

                switch (orr)
                {
                case THINGY_ORIENT_X_UP:
                        gpio_pin_set_raw(io_expander, BLUE_LED, 0);
                        gpio_pin_set_raw(io_expander, GREEN_LED, 1);
                        gpio_pin_set_raw(io_expander, RED_LED, 1);
                        break;
                case THINGY_ORIENT_X_DOWN:
                        gpio_pin_set_raw(io_expander, BLUE_LED, 1);
                        gpio_pin_set_raw(io_expander, GREEN_LED, 0);
                        gpio_pin_set_raw(io_expander, RED_LED, 1);
                        break;
                case THINGY_ORIENT_Y_UP:
                        gpio_pin_set_raw(io_expander, BLUE_LED, 1);
                        gpio_pin_set_raw(io_expander, GREEN_LED, 1);
                        gpio_pin_set_raw(io_expander, RED_LED, 0);
                        break;
                case THINGY_ORIENT_Y_DOWN:
                        gpio_pin_set_raw(io_expander, BLUE_LED, 1);
                        gpio_pin_set_raw(io_expander, GREEN_LED, 0);
                        gpio_pin_set_raw(io_expander, RED_LED, 0);
                        break;
                case THINGY_ORIENT_Z_UP:
                        gpio_pin_set_raw(io_expander, BLUE_LED, 0);
                        gpio_pin_set_raw(io_expander, GREEN_LED, 1);
                        gpio_pin_set_raw(io_expander, RED_LED, 0);
                        break;
                case THINGY_ORIENT_Z_DOWN:
                        gpio_pin_set_raw(io_expander, BLUE_LED, 0);
                        gpio_pin_set_raw(io_expander, GREEN_LED, 0);
                        gpio_pin_set_raw(io_expander, RED_LED, 0);
                        break;
                default:
                        break;
                }

	        // printk("Current orientation: %d\n", orr);
		k_sleep(K_MSEC(30));
	}
}