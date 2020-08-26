/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <bluetooth/bluetooth.h>
#include <bluetooth/mesh/models.h>
#include <drivers/gpio.h>
#include <drivers/gpio/gpio_sx1509b.h>
#include <nrfx_pdm.h>
#include <dk_buttons_and_leds.h>
#include "model_handler.h"
#include "thingy52_cli.h"

#define ARM_MATH_CM0PLUS
#include <arm_math.h>

#define PDM_SIZE 512
#define SPECTRUM_SIZE (PDM_SIZE / 2)
#define NUM_FREQ_BANDS 3

static arm_rfft_instance_q15 S15;

struct mic_data {
	nrfx_pdm_config_t pdm_config;
	int16_t pdm_buf_a[PDM_SIZE];
	q15_t fft_buf[SPECTRUM_SIZE];
	q15_t spectrum[SPECTRUM_SIZE];
	int16_t pdm_buf_b[PDM_SIZE];
	bool fft_result_ready;
	struct k_delayed_work microphone_work;
};

static struct device *io_expander;

static struct mic_data mic_data = {
	.pdm_config = NRFX_PDM_DEFAULT_CONFIG(26, 25),
};
static struct bt_mesh_thingy52_cli thingy52_cli[NUM_FREQ_BANDS] = {
	[0 ... NUM_FREQ_BANDS - 1] = BT_MESH_THINGY52_CLI_INIT
};

static inline int nrfx_err_code_check(nrfx_err_t nrfx_err)
{
	return NRFX_ERROR_BASE_NUM - nrfx_err ? true : false;
}

static void button_handler(uint32_t pressed, uint32_t changed)
{
	if (pressed & BIT(0)) {
		int err = 0;
		static bool onoff = true;

		if (onoff) {
			err |= gpio_port_set_bits_raw(io_expander,
						      BIT(MIC_PWR));
			err |= nrfx_err_code_check(nrfx_pdm_start());

			for (int i = GREEN_LED; i <= RED_LED; i++) {
				err |= sx1509b_pwm_pin_set(io_expander, i, 20);
			}

			k_delayed_work_submit(&mic_data.microphone_work,
					      K_NO_WAIT);
		} else {
			k_delayed_work_cancel(&mic_data.microphone_work);

			for (int i = GREEN_LED; i <= RED_LED; i++) {
				err |= sx1509b_pwm_pin_set(io_expander, i, 0);
			}

			err |= nrfx_err_code_check(nrfx_pdm_stop());
			err |= gpio_port_clear_bits_raw(io_expander,
							BIT(MIC_PWR));
		}

		onoff = !onoff;

		if (err) {
			printk("Error running PDM\n");
		}
	}
}

static void button_and_led_init(void)
{
	int err = 0;

	err |= dk_buttons_init(button_handler);

	for (int i = GREEN_LED; i <= RED_LED; i++) {
		err |= sx1509b_pwm_pin_configure(io_expander, i);
	}

	if (err) {
		printk("Initializing buttons and leds failed\n");
	}
}

static void pdm_event_handler(nrfx_pdm_evt_t const *p_evt)
{
	if (p_evt->error) {
		printk("PDM overflow error\n");
	} else if (p_evt->buffer_requested && p_evt->buffer_released == 0) {
		nrfx_pdm_buffer_set(mic_data.pdm_buf_a,
				    sizeof(mic_data.pdm_buf_a) /
					    sizeof(mic_data.pdm_buf_a[0]));
	} else if (p_evt->buffer_requested &&
		   p_evt->buffer_released == mic_data.pdm_buf_a) {
		nrfx_pdm_buffer_set(mic_data.pdm_buf_b,
				    sizeof(mic_data.pdm_buf_b) /
					    sizeof(mic_data.pdm_buf_b[0]));

		if (!mic_data.fft_result_ready) {
			arm_rfft_q15(&S15, (q15_t *)mic_data.pdm_buf_a,
				     mic_data.fft_buf);
			arm_cmplx_mag_q15(mic_data.fft_buf, mic_data.spectrum,
					  SPECTRUM_SIZE);

			mic_data.fft_result_ready = true;
		}
	} else if (p_evt->buffer_requested &&
		   p_evt->buffer_released == mic_data.pdm_buf_b) {
		nrfx_pdm_buffer_set(mic_data.pdm_buf_a,
				    sizeof(mic_data.pdm_buf_a) /
					    sizeof(mic_data.pdm_buf_a[0]));

		if (!mic_data.fft_result_ready) {
			arm_rfft_q15(&S15, (q15_t *)mic_data.pdm_buf_b,
				     mic_data.fft_buf);
			arm_cmplx_mag_q15(mic_data.fft_buf, mic_data.spectrum,
					  SPECTRUM_SIZE);

			mic_data.fft_result_ready = true;
		}
	}
}

static void send_color_helper(int iterator, uint16_t value)
{
	int err = 0;

	static struct bt_mesh_thingy52_rgb_msg msg = {
		.ttl = 0,
		.delay = 0xFFFF,
		.speaker_on = false,
	};

	/* Should match NUM_FREQ_BANDS */
	if (iterator == 0) {
		msg.color.red = 0;
		msg.color.green = 0;
		msg.color.blue = value;
		err |= bt_mesh_thingy52_cli_rgb_set(&thingy52_cli[iterator],
						    NULL, &msg);
	} else if (iterator == 1) {
		msg.color.red = 0;
		msg.color.green = value;
		msg.color.blue = 0;
		err |= bt_mesh_thingy52_cli_rgb_set(&thingy52_cli[iterator],
						    NULL, &msg);
	} else if (iterator == 2) {
		msg.color.red = value;
		msg.color.green = 0;
		msg.color.blue = 0;
		err |= bt_mesh_thingy52_cli_rgb_set(&thingy52_cli[iterator],
						    NULL, &msg);
	} else {
		printk("Unknown iterator\n");
	}

	if (err) {
		printk("Error sending RGB set message\n");
	}
}

/* Values in the spectrum array are used as raw values (not q15) */
static void microphone_work_handler(struct k_work *work)
{
	static uint16_t freq_sum[NUM_FREQ_BANDS];
	static uint16_t prev_freq_sum[NUM_FREQ_BANDS];
	static uint16_t total_sum;
	static uint16_t checker;
	static uint8_t divider;
	static uint16_t adj_freq_sum;

	if (mic_data.fft_result_ready) {
		int i;

		for (i = 0; i < NUM_FREQ_BANDS; i++) {
			freq_sum[i] = 0;
		}

		/* Should create the amount of for-loops
		 * that macthes NUM_FREQ_BANDS
		 */
		for (i = 0; i <= 80; i++) {
			if (mic_data.spectrum[i] > 100) {
				freq_sum[0] += mic_data.spectrum[i];
			}
		}

		for (i = 81; i <= 115; i++) {
			if (mic_data.spectrum[i] > 100) {
				freq_sum[1] += mic_data.spectrum[i];
			}
		}

		for (i = 116; i <= 255; i++) {
			if (mic_data.spectrum[i] > 100) {
				freq_sum[2] += mic_data.spectrum[i];
			}
		}

		total_sum = freq_sum[0] + freq_sum[1] + freq_sum[2];

		/* A basic way of handling volume changes */
		if (total_sum) {
			checker = total_sum / (4 + total_sum / 5000);
			divider = 6 + total_sum / 1000;
		}

		for (i = 0; i < NUM_FREQ_BANDS; i++) {
			if (freq_sum[i]) {
				if (freq_sum[i] - prev_freq_sum[i] > checker) {
					adj_freq_sum = freq_sum[i] / divider;

					if (adj_freq_sum > 255) {
						adj_freq_sum = 255;
					}

					send_color_helper(i, adj_freq_sum);

					prev_freq_sum[i] = freq_sum[i];
				}
			} else {
				prev_freq_sum[i] = 0;
			}
		}

		mic_data.fft_result_ready = false;
	}

	/* Should change PDM buffer size to match interval */
	k_delayed_work_submit(&mic_data.microphone_work, K_MSEC(60));
}

static void microphone_init(void)
{
	int err = 0;

	err |= arm_rfft_init_q15(&S15, PDM_SIZE, 0, 1);
	err |= gpio_pin_configure(io_expander, MIC_PWR, GPIO_OUTPUT);

	/* 80 is max */
	mic_data.pdm_config.gain_l = 80;

	err |= nrfx_err_code_check(
		nrfx_pdm_init(&mic_data.pdm_config, pdm_event_handler));

	if (err) {
		printk("Initializing microphone failed\n");
	}

	k_delayed_work_init(&mic_data.microphone_work, microphone_work_handler);
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
	int err = 0;

	for (int i = GREEN_LED; i <= RED_LED; i++) {
		err |= sx1509b_pwm_pin_set(io_expander, i, 255);
	}

	if (err) {
		printk("Error turning on attention light\n");
	}
}

static void attention_off(struct bt_mesh_model *mod)
{
	int err = 0;

	for (int i = GREEN_LED; i <= RED_LED; i++) {
		err |= sx1509b_pwm_pin_set(io_expander, i, 0);
	}

	if (err) {
		printk("Error turning off attention light\n");
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
	BT_MESH_ELEM(2, BT_MESH_MODEL_NONE,
		     BT_MESH_MODEL_LIST(
			     BT_MESH_MODEL_THINGY52_CLI(&thingy52_cli[0]))),
	BT_MESH_ELEM(3, BT_MESH_MODEL_NONE,
		     BT_MESH_MODEL_LIST(
			     BT_MESH_MODEL_THINGY52_CLI(&thingy52_cli[1]))),
	BT_MESH_ELEM(4, BT_MESH_MODEL_NONE,
		     BT_MESH_MODEL_LIST(
			     BT_MESH_MODEL_THINGY52_CLI(&thingy52_cli[2]))),
};

static const struct bt_mesh_comp comp = {
	.cid = CONFIG_BT_COMPANY_ID,
	.elem = elements,
	.elem_count = ARRAY_SIZE(elements),
};

const struct bt_mesh_comp *model_handler_init(void)
{
	io_expander = device_get_binding(DT_PROP(DT_NODELABEL(sx1509b), label));

	if (io_expander == NULL) {
		printk("Failure occurred while binding I/O expander\n");
		return &comp;
	}

	button_and_led_init();
	microphone_init();

	return &comp;
}
