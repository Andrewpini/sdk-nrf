/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <bluetooth/bluetooth.h>
#include <bluetooth/mesh/models.h>
#include <drivers/gpio.h>
#include <drivers/pwm.h>
#include <dk_buttons_and_leds.h>
#include "model_handler.h"
#include <drivers/gpio/gpio_sx1509b.h>

static struct device *io_expander;
static struct device *gpio_0;

static struct device *spkr_pwm;
static u32_t pwm_frequency = 880;

nrfx_pdm_config_t pdm_config = NRFX_PDM_DEFAULT_CONFIG(26, 25);
static const int pdm_sampling_freq = 16000;
static const int fft_size = 256;
static int16_t pdm_buffer_a[256];
static int16_t pdm_buffer_b[256];
static const int spectrum_size = fft_size / 2;
static int spectrum[128];
static int avg_freq;

static struct k_delayed_work microphone_work;

static int nrfx_err_code_check(nrfx_err_t nrfx_err)
{
	if (NRFX_ERROR_BASE_NUM - nrfx_err) {
		return 1;
	} else {
		return 0;
	}
}

static void microphone_work_handler(struct k_work *work)
{
	if (fft_analyzer_available()) {
		int counter = 0;

		fft_analyzer_read(spectrum, spectrum_size);

		/* i = 8 for frequencies from 500 Hz and up */
		for (int i = 8; i < spectrum_size; i++) {
			if (spectrum[i]) {
				if (!counter) {
					avg_freq = ((i * pdm_sampling_freq) /
						    fft_size);
				} else {
					avg_freq = avg_freq +
						   ((((i * pdm_sampling_freq) /
						      fft_size) -
						     avg_freq) /
						    counter);
				}

				counter++;
			}
		}

		if (avg_freq) {
			printk("Average frequency: %d\n", avg_freq);
		}
	}

	k_delayed_work_submit(&microphone_work, K_MSEC(200));
}

static void button_handler_cb(u32_t pressed, u32_t changed)
{
	int err;

	if ((pressed & BIT(0))) {
		gpio_port_set_bits_raw(io_expander, BIT(MIC_PWR));

		err = nrfx_err_code_check(nrfx_pdm_start());

		if (err) {
			printk("Error starting PDM\n");
		} else {
			printk("PDM started\n");
		}

		k_delayed_work_submit(&microphone_work, K_NO_WAIT);
	} else if ((!pressed & BIT(0))) {
		gpio_port_set_bits_raw(io_expander, BIT(GREEN_LED) |
							    BIT(BLUE_LED) |
							    BIT(RED_LED));

		k_delayed_work_cancel(&microphone_work);

		err = nrfx_err_code_check(nrfx_pdm_stop());

		if (err) {
			printk("Error stopping PDM\n");
		} else {
			printk("PDM stopped\n");
		}

		gpio_port_clear_bits_raw(io_expander, BIT(MIC_PWR));
	}
}

static struct button_handler button_handler = {
	.cb = button_handler_cb,
};

static void pdm_event_handler(nrfx_pdm_evt_t const *p_evt)
{
	if (p_evt->error) {
		printk("PDM overflow error\n");
	} else if (p_evt->buffer_requested && p_evt->buffer_released == 0) {
		nrfx_pdm_buffer_set(pdm_buffer_a,
				    sizeof(pdm_buffer_a) /
					    sizeof(pdm_buffer_a[0]));
	} else if (p_evt->buffer_requested &&
		   p_evt->buffer_released == pdm_buffer_a) {
		fft_analyzer_update(pdm_buffer_a, fft_size);
		nrfx_pdm_buffer_set(pdm_buffer_b,
				    sizeof(pdm_buffer_b) /
					    sizeof(pdm_buffer_b[0]));
	} else if (p_evt->buffer_requested &&
		   p_evt->buffer_released == pdm_buffer_b) {
		fft_analyzer_update(pdm_buffer_b, fft_size);
		nrfx_pdm_buffer_set(pdm_buffer_a,
				    sizeof(pdm_buffer_a) /
					    sizeof(pdm_buffer_a[0]));
	}
}

static void button_and_led_init(void)
{
	int err = 0;

	dk_buttons_init(NULL);
	dk_button_handler_add(&button_handler);

	io_expander = device_get_binding(DT_PROP(DT_NODELABEL(sx1509b), label));
        err |= sx1509b_led_drv_init(io_expander);
        err |= sx1509b_led_drv_pin_init(io_expander, GREEN_LED);
        err |= sx1509b_led_drv_pin_init(io_expander, BLUE_LED);
        err |= sx1509b_led_drv_pin_init(io_expander, RED_LED);


        printk("PWM_ERROR: %d\n", err);
	if (io_expander == NULL) {
		printk("Could not initiate I/O expander\n");
	}

	// err = gpio_pin_configure(io_expander, GREEN_LED, GPIO_OUTPUT);

	// if (err) {
	// 	printk("Could not configure green LED pin\n");
	// }

	// err = gpio_pin_configure(io_expander, BLUE_LED, GPIO_OUTPUT);

	// if (err) {
	// 	printk("Could not configure blue LED pin\n");
	// }

	// err = gpio_pin_configure(io_expander, RED_LED, GPIO_OUTPUT);

	// if (err) {
	// 	printk("Could not configure red LED pin\n");
	// }
}

static void speaker_init(void)
{
	int err;
	u32_t pwm_period = 1000000U / pwm_frequency;

	gpio_0 = device_get_binding(DT_PROP(DT_NODELABEL(gpio0), label));

	if (gpio_0 == NULL) {
		printk("Could not initiate GPIO 0\n");
		return;
	}

	err = gpio_pin_configure(gpio_0, 29, GPIO_OUTPUT | GPIO_PULL_UP);

	if (err) {
		printk("Could not configure speaker power pin\n");
		return;
	} else {
		printk("Speaker power pin configured\n");
	}

	spkr_pwm = device_get_binding(DT_PROP(DT_NODELABEL(pwm0), label));

	if (spkr_pwm == NULL) {
		printk("Could not initiate speaker PWM\n");
		return;
	} else {
		printk("Speaker PWM (%s) initiated\n", spkr_pwm->name);
	}

	err = pwm_pin_set_usec(spkr_pwm, 27, pwm_period, pwm_period / 2U, 0);

	if (err) {
		printk("Error %d: failed to set PWM values\n", err);
		return;
	}
}

static void microphone_init(void)
{
	int err;

	err = fft_analyzer_configure(fft_size);

	if (err) {
		printk("Error configuring FFT analyzer\n");
	} else {
		printk("FFT analyzer configured\n");
	}

	err = gpio_pin_configure(io_expander, 9, GPIO_OUTPUT);

	if (err) {
		printk("Could not configure speaker power pin\n");
		return;
	} else {
		printk("Speaker power pin configured\n");
	}

	gpio_port_set_bits_raw(io_expander, BIT(MIC_PWR));

	pdm_config.gain_l = 70; // 80 (decimal) is max

	err = nrfx_err_code_check(
		nrfx_pdm_init(&pdm_config, pdm_event_handler));

	if (err) {
		printk("Error initiating PDM\n");
	} else {
		printk("PDM initiated\n");
	}

	gpio_port_clear_bits_raw(io_expander, BIT(MIC_PWR));

	k_delayed_work_init(&microphone_work, microphone_work_handler);
}

/** Configuration server definition */
static struct bt_mesh_cfg_srv cfg_srv = {
	.relay = IS_ENABLED(CONFIG_BT_MESH_RELAY),
	.beacon = BT_MESH_BEACON_ENABLED,
	.frnd = IS_ENABLED(CONFIG_BT_MESH_FRIEND),
	.gatt_proxy = IS_ENABLED(CONFIG_BT_MESH_GATT_PROXY),
	.default_ttl = 7,

	/* 3 transmissions with 20ms interval */
	.net_transmit = BT_MESH_TRANSMIT(2, 20),
	.relay_retransmit = BT_MESH_TRANSMIT(2, 20),
};

static void attention_on(struct bt_mesh_model *mod)
{
	int err;

	err = gpio_pin_set_raw(gpio_0, SPKR_PWR, 1);

	if (err) {
		printk("Could not set speaker power pin\n");
		return;
	} else {
		printk("Speaker power on\n");
	}
}

static void attention_off(struct bt_mesh_model *mod)
{
	int err;

	err = gpio_pin_set_raw(gpio_0, SPKR_PWR, 0);

	if (err) {
		printk("Could not set speaker power pin\n");
		return;
	} else {
		printk("Speaker power off\n");
	}
}

static const struct bt_mesh_health_srv_cb health_srv_cb = {
	.attn_on = attention_on,
	.attn_off = attention_off,
};

static struct bt_mesh_health_srv health_srv = {
	.cb = &health_srv_cb,
};

BT_MESH_HEALTH_PUB_DEFINE(health_pub, 0);

static struct bt_mesh_elem elements[] = {
	BT_MESH_ELEM(1,
		     BT_MESH_MODEL_LIST(BT_MESH_MODEL_CFG_SRV(&cfg_srv),
					BT_MESH_MODEL_HEALTH_SRV(&health_srv,
								 &health_pub)),
		     BT_MESH_MODEL_NONE),
};

static const struct bt_mesh_comp comp = {
	.cid = CONFIG_BT_COMPANY_ID,
	.elem = elements,
	.elem_count = ARRAY_SIZE(elements),
};

static struct k_delayed_work orentiation_led_work;
static void orentiation_led(struct k_work *work)
{
        static uint32_t cnt;
        uint8_t red_val = ((cnt >> 0) & 0b11) * 64;
        uint8_t blue_val = ((cnt >> 2) & 0b11) * 64;
        uint8_t green_val = ((cnt >> 4) & 0b11) * 64;
        printk("Current count: %d. Red: %d, Blue: %d, green: %d\n", cnt, red_val, blue_val, green_val);
        sx1509b_set_pwm(io_expander, RED_LED, red_val);
        sx1509b_set_pwm(io_expander, BLUE_LED, blue_val);
        sx1509b_set_pwm(io_expander, GREEN_LED, green_val);
        cnt ++;
        if (cnt > 256)
        {
                cnt= 0;
        }

	k_delayed_work_submit(&orentiation_led_work, K_MSEC(200));
}

const struct bt_mesh_comp *model_handler_init(void)
{
	button_and_led_init();
	speaker_init();
	// microphone_init();
	// gpio_pin_set_raw(io_expander, BLUE_LED, 0);
	// gpio_pin_set_raw(io_expander, GREEN_LED, 0);
	// gpio_pin_set_raw(io_expander, RED_LED, 0);
	k_delayed_work_init(&orentiation_led_work, orentiation_led);
	// k_delayed_work_submit(&orentiation_led_work, K_NO_WAIT);
        sx1509b_set_pwm(io_expander, RED_LED, 255);
        sx1509b_set_pwm(io_expander, GREEN_LED, 8);
        sx1509b_set_pwm(io_expander, BLUE_LED, 0);


	return &comp;
}
