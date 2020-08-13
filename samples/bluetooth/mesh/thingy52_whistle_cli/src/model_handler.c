/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <bluetooth/bluetooth.h>
#include <bluetooth/mesh/models.h>
#include <drivers/gpio.h>
#include <drivers/gpio/gpio_sx1509b.h>
#include <drivers/pwm.h>
#include <dk_buttons_and_leds.h>
#include <thingy52_orientation_handler.h>
#include "model_handler.h"

#define PDM_SAMPLING_FREQ 16000
#define FFT_SIZE 256
#define SPECTRUM_SIZE (FFT_SIZE / 2)
#define SPEAKER_PWM_FREQ 880

struct devices {
	struct device *io_expander;
	struct device *gpio_0;
	struct device *spkr_pwm;
};
static struct devices dev;

struct mic_config {
	nrfx_pdm_config_t pdm_config;
	int16_t pdm_buffer_a[FFT_SIZE];
	int16_t pdm_buffer_b[FFT_SIZE];
	struct k_delayed_work microphone_work;
};
static struct mic_config mic_cfg = {
	.pdm_config = NRFX_PDM_DEFAULT_CONFIG(26, 25),
};

static int avg_freq;

static inline int nrfx_err_code_check(nrfx_err_t nrfx_err)
{
	return NRFX_ERROR_BASE_NUM - nrfx_err ? true : false;
}

static int bind_devices(void)
{
	dev.spkr_pwm = device_get_binding(DT_PROP(DT_NODELABEL(pwm0), label));
	dev.gpio_0 = device_get_binding(DT_PROP(DT_NODELABEL(gpio0), label));
	dev.io_expander =
		device_get_binding(DT_PROP(DT_NODELABEL(sx1509b), label));

	if ((dev.gpio_0 == NULL) || (dev.spkr_pwm == NULL) ||
	    (dev.io_expander == NULL)) {
		return -1;
	}

	return 0;
}

static void button_handler_cb(u32_t pressed, u32_t changed)
{
	orientation_t orr = thingy52_orientation_get();
	printk("Orientation: %d\n", orr);

	switch (orr) {
	case THINGY_ORIENT_X_UP:
		break;
	case THINGY_ORIENT_Y_UP:
		break;
	case THINGY_ORIENT_Y_DOWN:
		break;
	case THINGY_ORIENT_X_DOWN:
		break;
	case THINGY_ORIENT_Z_DOWN:

		if ((pressed & BIT(0))) {
			sx1509b_set_pin_value(dev.io_expander, MIC_PWR, 1);
			nrfx_err_code_check(nrfx_pdm_start());
			k_delayed_work_submit(&mic_cfg.microphone_work,
					      K_NO_WAIT);
			printk("Mic Start\n");

		} else if ((!pressed & BIT(0))) {
			k_delayed_work_cancel(&mic_cfg.microphone_work);
			nrfx_err_code_check(nrfx_pdm_stop());
			sx1509b_set_pin_value(dev.io_expander, MIC_PWR, 0);
			printk("Mic Stop\n");
		}
		break;

	case THINGY_ORIENT_Z_UP:
		break;
	default:
		return;
	}
}

static struct button_handler button_handler = {
	.cb = button_handler_cb,
};

static void button_and_led_init(void)
{
	int err = 0;

	err |= dk_buttons_init(NULL);
	dk_button_handler_add(&button_handler);

	for (int i = GREEN_LED; i <= RED_LED; i++) {
		err |= sx1509b_pin_configure(dev.io_expander, i, SX1509B_PWM);
	}

	if (err) {
		printk("Initializing buttons and leds failed.\n");
	}
}

static void speaker_init(u32_t pwm_frequency)
{
	int err = 0;
	u32_t pwm_period = 1000000U / pwm_frequency;

	err |= gpio_pin_configure(dev.gpio_0, SPKR_PWR,
				  GPIO_OUTPUT | GPIO_PULL_UP);
	err |= pwm_pin_set_usec(dev.spkr_pwm, SPKR_PWM, pwm_period,
				pwm_period / 2U, 0);

	if (err) {
		printk("Initializing speaker failed.\n");
	}
}

static void pdm_event_handler(nrfx_pdm_evt_t const *p_evt)
{
	if (p_evt->error) {
		printk("PDM overflow error\n");
	} else if (p_evt->buffer_requested && p_evt->buffer_released == 0) {
		nrfx_pdm_buffer_set(mic_cfg.pdm_buffer_a,
				    sizeof(mic_cfg.pdm_buffer_a) /
					    sizeof(mic_cfg.pdm_buffer_a[0]));
	} else if (p_evt->buffer_requested &&
		   p_evt->buffer_released == mic_cfg.pdm_buffer_a) {
		fft_analyzer_update(mic_cfg.pdm_buffer_a, FFT_SIZE);
		nrfx_pdm_buffer_set(mic_cfg.pdm_buffer_b,
				    sizeof(mic_cfg.pdm_buffer_b) /
					    sizeof(mic_cfg.pdm_buffer_b[0]));
	} else if (p_evt->buffer_requested &&
		   p_evt->buffer_released == mic_cfg.pdm_buffer_b) {
		fft_analyzer_update(mic_cfg.pdm_buffer_b, FFT_SIZE);
		nrfx_pdm_buffer_set(mic_cfg.pdm_buffer_a,
				    sizeof(mic_cfg.pdm_buffer_a) /
					    sizeof(mic_cfg.pdm_buffer_a[0]));
	}
}

static void microphone_work_handler(struct k_work *work)
{
	static int spectrum[SPECTRUM_SIZE];

	if (fft_analyzer_available()) {
		int counter = 0;
		avg_freq = 0;
		fft_analyzer_read(spectrum, SPECTRUM_SIZE);

		/* i = 8 for frequencies from 500 Hz and up */
		for (int i = 8; i < SPECTRUM_SIZE; i++) {
			if (spectrum[i]) {
				if (!counter) {
					avg_freq = ((i * PDM_SAMPLING_FREQ) /
						    FFT_SIZE);
				} else {
					avg_freq = avg_freq +
						   ((((i * PDM_SAMPLING_FREQ) /
						      FFT_SIZE) -
						     avg_freq) /
						    counter);
				}

				counter++;

			}
		}
			printk("Average frequency: %d\n", avg_freq);
	}

	k_delayed_work_submit(&mic_cfg.microphone_work, K_MSEC(200));
}

static void microphone_init(void)
{
	int err = 0;

	err |= fft_analyzer_configure(FFT_SIZE);
	err |= sx1509b_pin_configure(dev.io_expander, MIC_PWR, SX1509B_OUTPUT);
	mic_cfg.pdm_config.gain_l = 70; // 80 (decimal) is max
	err |= nrfx_err_code_check(
		nrfx_pdm_init(&mic_cfg.pdm_config, pdm_event_handler));

	if (err) {
		printk("Initializing microphone failed.\n");
	}

	k_delayed_work_init(&mic_cfg.microphone_work, microphone_work_handler);
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
	for (int i = GREEN_LED; i <= RED_LED; i++) {
		sx1509b_led_set_pwm_value(dev.io_expander, i, 255);
	}
}

static void attention_off(struct bt_mesh_model *mod)
{
	for (int i = GREEN_LED; i <= RED_LED; i++) {
		sx1509b_led_set_pwm_value(dev.io_expander, i, 0);
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

struct bt_mesh_whistle_cb handlers = {

};
static struct bt_mesh_whistle_cli whistle_cli = BT_MESH_WHISTLE_CLI_INIT;

static struct bt_mesh_elem elements[] = {
	BT_MESH_ELEM(1,
		     BT_MESH_MODEL_LIST(BT_MESH_MODEL_CFG_SRV(&cfg_srv),
					BT_MESH_MODEL_HEALTH_SRV(&health_srv,
								 &health_pub)),
		     BT_MESH_MODEL_NONE),
	BT_MESH_ELEM(
		2, BT_MESH_MODEL_NONE,
		BT_MESH_MODEL_LIST(BT_MESH_MODEL_WHISTLE_CLI(&whistle_cli))),
};

static const struct bt_mesh_comp comp = {
	.cid = CONFIG_BT_COMPANY_ID,
	.elem = elements,
	.elem_count = ARRAY_SIZE(elements),
};

const struct bt_mesh_comp *model_handler_init(void)
{
	if (bind_devices()) {
		printk("Failure occured while binding devices\n");
		return &comp;
	}

	button_and_led_init();
	thingy52_orientation_handler_init();
	speaker_init(SPEAKER_PWM_FREQ);
	microphone_init();

	return &comp;
}
