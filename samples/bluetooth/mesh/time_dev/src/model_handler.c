/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <bluetooth/bluetooth.h>
#include <bluetooth/mesh/models.h>
#include <dk_buttons_and_leds.h>
#include "model_handler.h"

static void led_set(struct bt_mesh_onoff_srv *srv, struct bt_mesh_msg_ctx *ctx,
		    const struct bt_mesh_onoff_set *set,
		    struct bt_mesh_onoff_status *rsp);

static void led_get(struct bt_mesh_onoff_srv *srv, struct bt_mesh_msg_ctx *ctx,
		    struct bt_mesh_onoff_status *rsp);

static const struct bt_mesh_onoff_srv_handlers onoff_handlers = {
	.set = led_set,
	.get = led_get,
};

struct led_ctx {
	struct bt_mesh_onoff_srv srv;
	struct k_delayed_work work;
	uint32_t remaining;
	bool value;
};

static struct led_ctx led_ctx[4] = {
	[0 ... 3] = {
		.srv = BT_MESH_ONOFF_SRV_INIT(&onoff_handlers),
	}
};

static void led_transition_start(struct led_ctx *led)
{
	int led_idx = led - &led_ctx[0];

	/* As long as the transition is in progress, the onoff
	 * state is "on":
	 */
	dk_set_led(led_idx, true);
	k_delayed_work_submit(&led->work, K_MSEC(led->remaining));
	led->remaining = 0;
}

static void led_status(struct led_ctx *led, struct bt_mesh_onoff_status *status)
{
	status->remaining_time =
		k_delayed_work_remaining_get(&led->work) + led->remaining;
	status->target_on_off = led->value;
	/* As long as the transition is in progress, the onoff state is "on": */
	status->present_on_off = led->value || status->remaining_time;
}

static void led_set(struct bt_mesh_onoff_srv *srv, struct bt_mesh_msg_ctx *ctx,
		    const struct bt_mesh_onoff_set *set,
		    struct bt_mesh_onoff_status *rsp)
{
	struct led_ctx *led = CONTAINER_OF(srv, struct led_ctx, srv);
	int led_idx = led - &led_ctx[0];

	if (set->on_off == led->value) {
		goto respond;
	}

	led->value = set->on_off;
	led->remaining = set->transition->time;

	if (set->transition->delay > 0) {
		k_delayed_work_submit(&led->work,
				      K_MSEC(set->transition->delay));
	} else if (set->transition->time > 0) {
		led_transition_start(led);
	} else {
		dk_set_led(led_idx, set->on_off);
	}

respond:
	if (rsp) {
		led_status(led, rsp);
	}
}

static void led_get(struct bt_mesh_onoff_srv *srv, struct bt_mesh_msg_ctx *ctx,
		    struct bt_mesh_onoff_status *rsp)
{
	struct led_ctx *led = CONTAINER_OF(srv, struct led_ctx, srv);

	led_status(led, rsp);
}

static void led_work(struct k_work *work)
{
	struct led_ctx *led = CONTAINER_OF(work, struct led_ctx, work.work);
	int led_idx = led - &led_ctx[0];

	if (led->remaining) {
		led_transition_start(led);
	} else {
		dk_set_led(led_idx, led->value);

		/* Publish the new value at the end of the transition */
		struct bt_mesh_onoff_status status;

		led_status(led, &status);
		bt_mesh_onoff_srv_pub(&led->srv, NULL, &status);
	}
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

/* Set up a repeating delayed work to blink the DK's LEDs when attention is
 * requested.
 */
static struct k_delayed_work attention_blink_work;

static void attention_blink(struct k_work *work)
{
	static int idx;
	const uint8_t pattern[] = {
		BIT(0) | BIT(1),
		BIT(1) | BIT(2),
		BIT(2) | BIT(3),
		BIT(3) | BIT(0),
	};
	dk_set_leds(pattern[idx++ % ARRAY_SIZE(pattern)]);
	k_delayed_work_submit(&attention_blink_work, K_MSEC(30));
}

static void attention_on(struct bt_mesh_model *mod)
{
	k_delayed_work_submit(&attention_blink_work, K_NO_WAIT);
}

static void attention_off(struct bt_mesh_model *mod)
{
	k_delayed_work_cancel(&attention_blink_work);
	dk_set_leds(DK_NO_LEDS_MSK);
}

static const struct bt_mesh_health_srv_cb health_srv_cb = {
	.attn_on = attention_on,
	.attn_off = attention_off,
};

static struct bt_mesh_health_srv health_srv = {
	.cb = &health_srv_cb,
};

BT_MESH_HEALTH_PUB_DEFINE(health_pub, 0);

void time_update_cb(struct bt_mesh_time_srv *srv, struct bt_mesh_msg_ctx *ctx,
		    enum bt_mesh_time_update_types type)
{
	printk("TIME UPDATE CB: %d\n", type);
}

struct bt_mesh_time_cli time_cli = BT_MESH_TIME_CLI_INIT;
struct bt_mesh_time_cli time_cli2 = BT_MESH_TIME_CLI_INIT;
struct bt_mesh_time_srv time_srv = BT_MESH_TIME_SRV_INIT(time_update_cb);

static struct bt_mesh_elem elements[] = {
	BT_MESH_ELEM(
		1, BT_MESH_MODEL_LIST(
			BT_MESH_MODEL_CFG_SRV(&cfg_srv),
			BT_MESH_MODEL_HEALTH_SRV(&health_srv, &health_pub),
			BT_MESH_MODEL_ONOFF_SRV(&led_ctx[0].srv)),
		BT_MESH_MODEL_NONE),
	BT_MESH_ELEM(
		2, BT_MESH_MODEL_LIST(BT_MESH_MODEL_TIME_SRV(&time_srv)),
		BT_MESH_MODEL_NONE),
	BT_MESH_ELEM(
		3, BT_MESH_MODEL_LIST(BT_MESH_MODEL_TIME_CLI(&time_cli)),
		BT_MESH_MODEL_NONE),
	BT_MESH_ELEM(
		4, BT_MESH_MODEL_LIST(BT_MESH_MODEL_TIME_CLI(&time_cli2)),
		BT_MESH_MODEL_NONE),
};

static void test_time_set(void){
	static struct bt_mesh_time_status resp;
	static struct bt_mesh_time_status nr0 = {
		.tai = 5000,
		.uncertainty = 40,
		.is_authority = false,
		.tai_utc_delta = -1,
		.time_zone_offset = -4,
	};
	int err = bt_mesh_time_cli_time_set(&time_cli, NULL, &nr0, &resp);
	if (!err)
	{
		printk("TAI sec: %lld\n", resp.tai);
		printk("Uncertainty: %lld\n", resp.uncertainty);
		printk("Authority?: %d\n", resp.is_authority);
		printk("TAi UTC delta: %d\n", resp.tai_utc_delta);
		printk("Time Zone offset: %d\n\n", resp.time_zone_offset);
	}
}
static void test_time_get(void){
	static struct bt_mesh_time_status resp;
	int err = bt_mesh_time_cli_time_get(&time_cli, NULL, &resp);
	if (!err)
	{
		printk("TAI sec: %lld\n", resp.tai);
		printk("Uncertainty: %lld\n", resp.uncertainty);
		printk("Authority?: %d\n", resp.is_authority);
		printk("TAi UTC delta: %d\n", resp.tai_utc_delta);
		printk("Time Zone offset: %d\n\n", resp.time_zone_offset);
	}
}

static void test_zone_set(void){
	struct bt_mesh_time_zone_change set_parms = {
		.new_offset = 123,
		.timestamp = 10,
	};
	static struct bt_mesh_time_zone_status resp;
	int err = bt_mesh_time_cli_zone_set(&time_cli, NULL, &set_parms, &resp);
	if (!err)
	{
		printk("current_offset: %d\n", resp.current_offset);
		printk("new_offset: %d\n", resp.time_zone_change.new_offset);
		printk("timestamp: %lld\n\n", resp.time_zone_change.timestamp);
	}
}

static void test_zone_get(void){
	static struct bt_mesh_time_zone_status resp;
	int err = bt_mesh_time_cli_zone_get(&time_cli, NULL, &resp);
	if (!err)
	{
		printk("current_offset: %d\n", resp.current_offset);
		printk("new_offset: %d\n", resp.time_zone_change.new_offset);
		printk("timestamp: %lld\n\n", resp.time_zone_change.timestamp);
	}
}

static void test_delta_set(void){
	static struct bt_mesh_time_tai_utc_delta_status resp;
	struct bt_mesh_time_tai_utc_change set_parms = {
		.delta_new =456,
		.timestamp = 15,
	};
	int err = bt_mesh_time_cli_tai_utc_delta_set(&time_cli, NULL, &set_parms, &resp);
	if (!err)
	{
		printk("delta_current: %d\n", resp.delta_current);
		printk("delta_new: %d\n", resp.tai_utc_change.delta_new);
		printk("timestamp: %lld\n\n", resp.tai_utc_change.timestamp);
	}
}

static void test_delta_get(void){
	static struct bt_mesh_time_tai_utc_delta_status resp;
	int err = bt_mesh_time_cli_tai_utc_delta_get(&time_cli, NULL, &resp);
	if (!err)
	{
		printk("delta_current: %d\n", resp.delta_current);
		printk("delta_new: %d\n", resp.tai_utc_change.delta_new);
		printk("timestamp: %lld\n\n", resp.tai_utc_change.timestamp);
	}
}

static void test_role_set(void){
	// static struct bt_mesh_time_role_ctx resp;
	// static struct bt_mesh_time_role_ctx set_params = {
	// 	.time_role = BT_MESH_TIME_RELAY,
	// };
	uint8_t resp;
	uint8_t set_params = (uint8_t)BT_MESH_TIME_CLIENT;
	int err = bt_mesh_time_cli_role_set(&time_cli, NULL, &set_params, &resp);
	if (!err)
	{
		printk("role: %d\n\n", resp);
	}
}

static void test_role_get(void){
	uint8_t resp;
	int err = bt_mesh_time_cli_role_get(&time_cli, NULL, &resp);
	if (!err)
	{
		printk("role: %d\n\n", resp);
	}
}

static void test_status_send(void){
	bt_mesh_time_srv_time_status_send(&time_srv, NULL);
}

static void test_manual_param_set(void){

        static struct bt_mesh_time_status ctx = {
                .tai = 12*60*60 * 1000,
                .uncertainty = 0,
                .tai_utc_delta = 0xff + 0,
                .time_zone_offset = 64 + 0,
                .is_authority = true,
        };
        bt_mesh_time_srv_time_set(&time_srv, k_uptime_get(), &ctx);

        static struct bt_mesh_time_zone_change ctx2 = {
                .new_offset = 64,
                .timestamp = 0,
        };
        bt_mesh_time_srv_time_zone_change_set(&time_srv, &ctx2);
        static struct bt_mesh_time_tai_utc_change ctx3 = {
                .delta_new = 0xff - 5,
                .timestamp = 12*60*60 + 5,
        };
        bt_mesh_time_srv_tai_utc_change_set(&time_srv, &ctx3);
        bt_mesh_time_srv_role_set(&time_srv, BT_MESH_TIME_RELAY);
}

static void display_ctx(void){
        printk("\n\nTAI millisec: %lld\n\n", time_srv.data.sync.status.tai);

        printk("Uncertainty millisec: %lld\n", time_srv.data.sync.status.uncertainty);
        printk("Authority?: %d\n\n", time_srv.data.sync.status.is_authority);

        printk("TAi UTC current: %d\n", time_srv.data.sync.status.tai_utc_delta);
        printk("TAi UTC delta: %d\n", time_srv.data.tai_utc_change.delta_new);
        printk("TAi UTC timestamp: %lld\n\n", time_srv.data.tai_utc_change.timestamp);

        printk("Time Zone current: %d\n", time_srv.data.sync.status.time_zone_offset);
        printk("Time Zone offset: %d\n", time_srv.data.time_zone_change.new_offset);
        printk("Time Zone timestamp: %lld\n\n", time_srv.data.time_zone_change.timestamp);

        printk("Role: %d\n\n", time_srv.data.role);
}

static void test_man_get_time(void){
	struct bt_mesh_time_status time;
	int err = bt_mesh_time_srv_status(&time_srv, k_uptime_get() - 3000, &time);

	if (!err)
	{
		printk("TAI msec: %lld\n", time.tai);
		printk("Uncertainty: %lld\n", time.uncertainty);
		printk("Authority?: %d\n", time.is_authority);
		printk("TAi UTC delta: %d\n", time.tai_utc_delta);
		printk("Time Zone offset: %d\n\n", time.time_zone_offset);
	} else if (err == -EAGAIN) {
		printk("Not initalized TAI\n");
	} else if (err == -ECANCELED) {
		printk("Could not compute time. Bad params\n");
	} else if (err == -ENOTSUP) {
		printk("Not initialized\n");
	}
}

static void test_tai_conversion(void)
{
        // for (int i = 0; i < 10; i++)
        // {
        //         struct tm timeptr;
        //         int64_t timestamp = (int64_t)sys_rand32_get();
        //         // int64_t timestamp = 50050000003001;
        //         // int64_t rest = timestamp % 1000;
        //         // bt_mesh_time_srv_localtime(&time_srv, timestamp * 1000, &timeptr, NULL);
        //         int64_t u_time;
        //         // struct tm timeptr2 = {
        //         //         .tm_sec = 20,   // seconds after the minute - [0, 60] including leap second
        //         //         .tm_min = 53,   // minutes after the hour - [0, 59]
        //         //         .tm_hour = 0,  // hours since midnight - [0, 23]
        //         //         // .tm_mday,  // day of the month - [1, 31]
        //         //         // .tm_mon,   // months since January - [0, 11]
        //         //         .tm_year = 2015 - 1900,  // years since 1900
        //         //         // .tm_wday,  // days since Sunday - [0, 6]
        //         //         .tm_yday = 308,  // days since January 1 - [0, 365]
        //         //         // .tm_isdst; // daylight savings time flag
        //         // };
        //         // bt_mesh_time_srv_mktime(&time_srv, &timeptr, &u_time, NULL);

        //         if ((timestamp * 1000) == u_time)
        //         {
        //                 printk("TAI: %llx\n", u_time);
        //         }
        //         else
        //         {
        //                 printk("ERROR WITH TS\n");
        //                 break;
        //         }
        // }
}

static void test_local_time(void)
{
        struct tm *timeptr;
        // uint32_t uncertainty;
        int64_t t_now = k_uptime_get();
        timeptr = bt_mesh_time_srv_localtime(&time_srv, t_now);
        printk("\n\nlocal_time_ms: %lld\n", t_now);
        printk("Year: %d\n", timeptr->tm_year + 1900);
        printk("Month: %d\n", timeptr->tm_mon);
        printk("Day: %d\n", timeptr->tm_mday);
        printk("Hour: %d\n", timeptr->tm_hour);
        printk("Minute: %d\n", timeptr->tm_min);
        printk("Second: %d\n", timeptr->tm_sec);
        printk("Days since 1st January: %d\n", timeptr->tm_yday);
        printk("Day of the week: %d\n", timeptr->tm_wday);
        // printk("Uncertainty: %d\n", uncertainty);

        struct tm timeptr2 = {
                .tm_sec = 11,   // seconds after the minute - [0, 60] including leap second
                .tm_min = 0,   // minutes after the hour - [0, 59]
                .tm_hour = 12,  // hours since midnight - [0, 23]
                // .tm_mday,  // day of the month - [1, 31]
                // .tm_mon,   // months since January - [0, 11]
                .tm_year = 2000 - 1900,  // years since 1900
                // .tm_wday,  // days since Sunday - [0, 6]
                .tm_yday = 0,  // days since January 1 - [0, 365]
                // .tm_isdst; // daylight savings time flag
        };
        int64_t uptime = bt_mesh_time_srv_mktime(&time_srv, &timeptr2);
        printk("Uptime: %lld\n", uptime);
        printk("Diff: %lld\n", abs(t_now - uptime));
}

static void test_mk_time(void)
{
        struct tm timeptr = {
                .tm_sec = 12,   // seconds after the minute - [0, 60] including leap second
                .tm_min = 23,   // minutes after the hour - [0, 59]
                .tm_hour = 1,  // hours since midnight - [0, 23]
                // .tm_mday,  // day of the month - [1, 31]
                // .tm_mon,   // months since January - [0, 11]
                .tm_year = 2000 - 1900,  // years since 1900
                // .tm_wday,  // days since Sunday - [0, 6]
                .tm_yday = 0,  // days since January 1 - [0, 365]
                // .tm_isdst; // daylight savings time flag
        };
        int64_t uptime = bt_mesh_time_srv_mktime(&time_srv, &timeptr);
        printk("Uptime: %lld\n", uptime);

        struct tm *timeptr3;
        // uint32_t uncertainty;
        timeptr3 = bt_mesh_time_srv_localtime(&time_srv, uptime);
        printk("\n\nYear: %d\n", timeptr3->tm_year + 1900);
        printk("Month: %d\n", timeptr3->tm_mon);
        printk("Day: %d\n", timeptr3->tm_mday);
        printk("Hour: %d\n", timeptr3->tm_hour);
        printk("Minute: %d\n", timeptr3->tm_min);
        printk("Second: %d\n", timeptr3->tm_sec);
        printk("Days since 1st January: %d\n", timeptr3->tm_yday);
        printk("Day of the week: %d\n", timeptr3->tm_wday);
        // struct tm timeptr2 = {
        //         .tm_sec = 0,   // seconds after the minute - [0, 60] including leap second
        //         .tm_min = 0,   // minutes after the hour - [0, 59]
        //         .tm_hour = 1,  // hours since midnight - [0, 23]
        //         // .tm_mday,  // day of the month - [1, 31]
        //         // .tm_mon,   // months since January - [0, 11]
        //         .tm_year = 2000 - 1900,  // years since 1900
        //         // .tm_wday,  // days since Sunday - [0, 6]
        //         .tm_yday = 0,  // days since January 1 - [0, 365]
        //         // .tm_isdst; // daylight savings time flag
        // };
        // int64_t uptime2;
        // bt_mesh_time_srv_mktime(&time_srv, &timeptr2, &uptime2);
        // printk("Uptime: %lld\n", uptime2);

        // struct tm timeptr3 = {
        //         .tm_sec = 10,   // seconds after the minute - [0, 60] including leap second
        //         .tm_min = 0,   // minutes after the hour - [0, 59]
        //         .tm_hour = 1,  // hours since midnight - [0, 23]
        //         // .tm_mday,  // day of the month - [1, 31]
        //         // .tm_mon,   // months since January - [0, 11]
        //         .tm_year = 2000 - 1900,  // years since 1900
        //         // .tm_wday,  // days since Sunday - [0, 6]
        //         .tm_yday = 0,  // days since January 1 - [0, 365]
        //         // .tm_isdst; // daylight savings time flag
        // };
        // int64_t uptime3;
        // bt_mesh_time_srv_mktime(&time_srv, &timeptr3, &uptime3);
        // printk("Uptime: %lld\n", uptime3);
}
static void time_srv_localtime(int64_t uptime)
{
        struct tm *timeptr;
        // uint32_t uncertainty;
        timeptr = bt_mesh_time_srv_localtime(&time_srv, uptime);
        printk("\n\nYear: %d\n", timeptr->tm_year + 1900);
        printk("Month: %d\n", timeptr->tm_mon);
        printk("Day: %d\n", timeptr->tm_mday);
        printk("Hour: %d\n", timeptr->tm_hour);
        printk("Minute: %d\n", timeptr->tm_min);
        printk("Second: %d\n", timeptr->tm_sec);
        printk("Days since 1st January: %d\n", timeptr->tm_yday);
        printk("Day of the week: %d\n", timeptr->tm_wday);
}

const int64_t start_time = 12*60*60 * 1000;
static void local_time_test(void)
{
        static struct bt_mesh_time_status ctx = {
                .tai = start_time, //1st of january, year 2000, 12:00:00
                .uncertainty = 0,
                .tai_utc_delta = 0,
                .time_zone_offset = 0,
                .is_authority = true,
        };
        static struct bt_mesh_time_tai_utc_change utc = {
                .delta_new = 0,
                .timestamp = 0,
        };
        static struct bt_mesh_time_zone_change zone = {
                .new_offset = 0,
                .timestamp = 0,
        };

        // Initial setup
        bt_mesh_time_srv_time_set(&time_srv, 0, &ctx);
        bt_mesh_time_srv_time_zone_change_set(&time_srv, &zone);
        bt_mesh_time_srv_tai_utc_change_set(&time_srv, &utc);

        printk("\nNO OFFSET\n");
        time_srv_localtime(0); //Expect: 1st of january, year 2000, 12:00:00
        time_srv_localtime(5000); //Expect: 1st of january, year 2000, 12:00:05
        time_srv_localtime(60*1000); //Expect: 1st of january, year 2000, 12:01:00

        zone.new_offset = 4;
        zone.timestamp = ((start_time + 3000) / 1000); //change of zone: 1st of january, year 2000, 12:00:03
        utc.delta_new = -10;
        utc.timestamp = ((start_time + 3000) / 1000); //change of zone: 1st of january, year 2000, 12:00:03
        bt_mesh_time_srv_tai_utc_change_set(&time_srv, &utc);
        bt_mesh_time_srv_time_zone_change_set(&time_srv, &zone);

        printk("\nOFFSET + 1 HOUR & 10 SEC\n");
        time_srv_localtime(0); //Expect: 1st of january, year 2000, 12:00:00
        time_srv_localtime(5000); //Expect: 1st of january, year 2000, 13:00:05
        time_srv_localtime(60*1000); //Expect: 1st of january, year 2000, 13:01:00

        zone.new_offset = -4;
        zone.timestamp = ((start_time + 3000) / 1000); //change of zone: 1st of january, year 2000, 12:00:03
        utc.delta_new = 10;
        utc.timestamp = ((start_time + 3000) / 1000); //change of zone: 1st of january, year 2000, 12:00:03
        bt_mesh_time_srv_tai_utc_change_set(&time_srv, &utc);
        bt_mesh_time_srv_time_zone_change_set(&time_srv, &zone);

        printk("\nOFFSET - 1 HOUR & 10 SEC\n");
        time_srv_localtime(0); //Expect: 1st of january, year 2000, 12:00:00
        time_srv_localtime(5000); //Expect: 1st of january, year 2000, 11:00:05
        time_srv_localtime(60*1000); //Expect: 1st of january, year 2000, 11:01:00

}

static void time_srv_mk_time(uint8_t H, uint8_t M, uint8_t S)
{
        struct tm timeptr = {
                .tm_sec = S,   // seconds after the minute - [0, 60] including leap second
                .tm_min = M,   // minutes after the hour - [0, 59]
                .tm_hour = H,  // hours since midnight - [0, 23]
                .tm_mday = 1,  // day of the month - [1, 31]
                .tm_mon = 0,   // months since January - [0, 11]
                .tm_year = 2000 - 1900,  // years since 1900
                // .tm_wday,  // days since Sunday - [0, 6]
                .tm_yday = 0,  // days since January 1 - [0, 365]
                // .tm_isdst; // daylight savings time flag
        };
        int64_t uptime = bt_mesh_time_srv_mktime(&time_srv, &timeptr);
        printk("Uptime: %lld\n", uptime);

}

static void mk_time_test(void)
{
        static struct bt_mesh_time_status ctx = {
                .tai = start_time, //1st of january, year 2000, 12:00:00
                .uncertainty = 0,
                .tai_utc_delta = 0,
                .time_zone_offset = 0,
                .is_authority = true,
        };
        static struct bt_mesh_time_tai_utc_change utc = {
                .delta_new = 0,
                .timestamp = 0,
        };
        static struct bt_mesh_time_zone_change zone = {
                .new_offset = 0,
                .timestamp = 0,
        };

        // Initial setup
        bt_mesh_time_srv_time_set(&time_srv, 0, &ctx);
        bt_mesh_time_srv_time_zone_change_set(&time_srv, &zone);
        bt_mesh_time_srv_tai_utc_change_set(&time_srv, &utc);

        printk("\nNO OFFSET\n");

        time_srv_mk_time(12,0,0); //Expect: 0
        time_srv_mk_time(12,0,5); //Expect: 5000
        time_srv_mk_time(13,0,5); //Expect: 3665000

        zone.new_offset = 4;
        zone.timestamp = ((start_time + 5000) / 1000); //change of zone: 1st of january, year 2000, 12:00:03
        utc.delta_new = -10;
        utc.timestamp = ((start_time + 3000) / 1000); //change of zone: 1st of january, year 2000, 12:00:03
        // bt_mesh_time_srv_tai_utc_change_set(&time_srv, &utc);
        bt_mesh_time_srv_time_zone_change_set(&time_srv, &zone);

        printk("\nOFFSET + 1 HOUR\n");
        time_srv_mk_time(12,0,0); //Expect: 0
        time_srv_mk_time(12,0,7);
        time_srv_mk_time(13,0,5); //Expect: 7205000
        time_srv_mk_time(13,0,10); //Expect: 0
        time_srv_mk_time(13,0,15); //Expect: 0

        zone.new_offset = -4;
        zone.timestamp = ((start_time + 0) / 1000); //change of zone: 1st of january, year 2000, 12:00:03
        utc.delta_new = 10;
        utc.timestamp = ((start_time + 4000) / 1000); //change of zone: 1st of january, year 2000, 12:00:03
        // bt_mesh_time_srv_tai_utc_change_set(&time_srv, &utc);
        bt_mesh_time_srv_time_zone_change_set(&time_srv, &zone);

        printk("\nOFFSET - 1 HOUR\n");
        time_srv_mk_time(11,0,2); //Expect: 0
        time_srv_mk_time(11,0,3); //Expect: 0
        time_srv_mk_time(12,0,0); //Expect: 0
        time_srv_mk_time(12,0,3);
        time_srv_mk_time(12,0,5);
        time_srv_mk_time(13,0,5); //Expect: 7205000

}

// static void test_man_uptime_get(void){
// 	uint64_t uptime;
// 	int err = bt_mesh_time_srv_get_uptime(&time_srv, 100, 0, 64, &uptime);

// 	if (!err)
// 	{
// 		printk("Uptime: %lld\n", uptime);
// 	} else if (err == -ECANCELED) {
// 		printk("Could not compute time. Bad params\n");
// 	}
// }

static uint8_t inc = 5;

static void menu_inc(void){
	if (inc < 5)
	{
		inc++;
	} else {
		inc = 0;
	}
	printk("\nON MENU NR: %d\n",inc);
}

static void button_handler_cb(uint32_t pressed, uint32_t changed)
{
	if (!bt_mesh_is_provisioned()) {
		return;
	}
		if (pressed & changed & BIT(0)) {
			menu_inc();
		} else {
			switch (inc)
			{
			case 0:
				if (pressed & changed & BIT(1)) {
					printk("Button 2\n");
					test_zone_set();
					test_delta_set();
				}
				if (pressed & changed & BIT(2)) {
					printk("Button 3\n");
					test_zone_get();
				}
				if (pressed & changed & BIT(3)) {
					printk("Button 4\n");
					test_delta_get();
				}
			break;

			case 1:
				if (pressed & changed & BIT(1)) {
					printk("Button 2\n");
					test_role_set();
				}
				if (pressed & changed & BIT(2)) {
					printk("Button 3\n");
					test_role_get();
				}
				if (pressed & changed & BIT(3)) {
					printk("Button 4\n");
                                        test_mk_time();
				}
			break;

			case 2:
				if (pressed & changed & BIT(1)) {
					printk("Button 2\n");
					test_time_set();
				}
				if (pressed & changed & BIT(2)) {
					printk("Button 3\n");
					test_time_get();
				}
				if (pressed & changed & BIT(3)) {
					printk("Button 4\n");
					test_man_get_time();
				}
			break;

			case 3:
				if (pressed & changed & BIT(1)) {
					printk("Button 2\n");
					test_manual_param_set();
				}
				if (pressed & changed & BIT(2)) {
					printk("Button 3\n");
					display_ctx();
				}
				if (pressed & changed & BIT(3)) {
					printk("Button 4\n");
					test_status_send();
				}
			break;

			case 4:
				if (pressed & changed & BIT(1)) {
					printk("Button 2\n");
					test_manual_param_set();
				}
				if (pressed & changed & BIT(2)) {
					printk("Button 3\n");
                                        test_local_time();
				}
				if (pressed & changed & BIT(3)) {
					printk("Button 4\n");
                                        local_time_test();
				}
			break;

			case 5:
				if (pressed & changed & BIT(1)) {
					printk("Button 2\n");
					mk_time_test();
				}
				if (pressed & changed & BIT(2)) {
					printk("Button 3\n");
				}
				if (pressed & changed & BIT(3)) {
					printk("Button 4\n");
				}
			break;


			default:
				break;
			}
		}

}

static const struct bt_mesh_comp comp = {
	.cid = CONFIG_BT_COMPANY_ID,
	.elem = elements,
	.elem_count = ARRAY_SIZE(elements),
};

static struct k_delayed_work time_work;
// static struct bt_mesh_time_status response;

// static struct bt_mesh_time_status nr0;
// static struct bt_mesh_time_zone_status nr1;
// static struct bt_mesh_time_tai_utc_delta_status nr2;
// static struct bt_mesh_time_role_ctx nr3;

// static struct bt_mesh_time_status nr5;
// static struct bt_mesh_time_zone_change nr6;
// static struct bt_mesh_time_tai_utc_change nr7;
// static struct bt_mesh_time_role_ctx nr8;


static void time_cb(struct k_work *work)
{
	// bt_mesh_time_cli_time_get(&time_cli, NULL, NULL);
	// bt_mesh_time_cli_zone_get(&time_cli, NULL, NULL);
	// bt_mesh_time_cli_tai_utc_delta_get(&time_cli, NULL, NULL);
	// bt_mesh_time_cli_role_get(&time_cli, NULL, NULL);

	// bt_mesh_time_cli_time_set(&time_cli, NULL, &nr5, NULL);
	// bt_mesh_time_cli_zone_set(&time_cli, NULL, &nr6, NULL);
	// bt_mesh_time_cli_tai_utc_delta_set(&time_cli, NULL, &nr7, NULL);
	// bt_mesh_time_cli_role_set(&time_cli, NULL, &nr8, NULL);
	// bt_mesh_time_srv_time_status_send(&time_srv, NULL, &nr0);
	// bt_mesh_time_srv_zone_status(&time_srv, NULL, &nr1);
	// bt_mesh_time_srv_tai_utc_delta_status(&time_srv, NULL, &nr2);
	// bt_mesh_time_srv_role_status(&time_srv, NULL, &nr3);

	// struct bt_mesh_time_status status = {
	// 	.seconds = 12345,
	// 	.subsec = 200,
	// 	.uncertainty = 69,
	// 	.is_authority = false,
	// 	.utc_delta = 666,
	// 	.time_zone = 100,
	// };
    // int err = bt_mesh_time_cli_time_set(&time_cli, NULL, &status, &response);
	// // int err = bt_mesh_time_srv_time_status_send(&time_srv, NULL, &status, NULL);
	// printk("ERR:%d\n", err);
	// printk("ACK: TAI sec: %lld\n", response.seconds);
	// printk("ACK: Subsec: %d\n", response.subsec);
	// printk("ACK: Uncertainty: %d\n", response.uncertainty);
	// printk("ACK: Authority?: %d\n", response.is_authority);
	// printk("ACK: TAi UTC delta: %d\n", response.utc_delta);
	// printk("ACK: Time Zone offset: %d\n", response.time_zone);
	k_delayed_work_submit(&time_work, K_SECONDS(3));
}


// K_TIMER_DEFINE(my_timer, my_timer_handler, NULL);

const struct bt_mesh_comp *model_handler_init(void)
{
	static struct button_handler button_handler = {
		.cb = button_handler_cb,
	};

	dk_button_handler_add(&button_handler);

	k_delayed_work_init(&attention_blink_work, attention_blink);
	k_delayed_work_init(&time_work, time_cb);
	k_delayed_work_submit(&time_work, K_SECONDS(1));
	// k_timer_start(&my_timer, K_SECONDS(1), K_SECONDS(1));

	for (int i = 0; i < ARRAY_SIZE(led_ctx); ++i) {
		k_delayed_work_init(&led_ctx[i].work, led_work);
	}

	return &comp;
}











#if 0

void test_conv(uint64_t tai)
{
    struct tm time;
    char timestr[40];
    tai_to_ts(MSEC_PER_SEC * tai, &time);
    strftime(timestr, sizeof(timestr), "%F %T", &time);
    printf("TAI: %lu -> %s\n", tai, timestr);
    ts_to_tai(&tai, &time);
    printf("TAI: %s -> %llu\n", timestr, tai / MSEC_PER_SEC);
}
void test_mktime(int8_t h, int8_t m, int8_t sec)
{
    struct tm time = {
        .tm_year = 120, // 2020
        .tm_mon = 6,
        .tm_mday = 15,
        .tm_hour = h,
        .tm_min = m,
        .tm_sec = sec,
    };
    int64_t uptime;
    char timestr[40];
    strftime(timestr, sizeof(timestr), "%F %T", &time);
    if (bt_mesh_time_srv_mktime(&time, &uptime)) {
        printf("%s -> Error\n", timestr);
    } else {
        printf("%s -> +%02ldH:%02ldM:%02dS (%lu ms)\n", timestr, uptime / MSEC_PER_HOUR, (uptime % MSEC_PER_HOUR) / MSEC_PER_MIN, (uptime % MSEC_PER_MIN) / MSEC_PER_SEC, uptime);
    }
}
void test_localtime(uint64_t uptime)
{
    struct tm time;
    bt_mesh_time_srv_localtime(uptime, &time);
    char timestr[40];
    strftime(timestr, sizeof(timestr), "%F %T", &time);
    printf("+%02ldH:%02ldM:%02dS -> %s\n", uptime / MSEC_PER_HOUR, (uptime % MSEC_PER_HOUR) / MSEC_PER_MIN, (uptime % MSEC_PER_MIN) / MSEC_PER_SEC, timestr);
}
#define UNIX_EPOCH_DIFF 946684800LL
#define SYNC_TIME_UNIX 1594805968LL //  July 15, 2020 9:39:28 AM UTC+0
#define SYNC_TIME_AFTER_Y2K_UTC (SYNC_TIME_UNIX - UNIX_EPOCH_DIFF)
#define DST_CHANGE_UNIX 1594818000LL // July 15, 2020 13:00:00 UTC+0, 15:00:00 localtime
#define DST_CHANGE_AFTER_Y2K_UTC (DST_CHANGE_UNIX - UNIX_EPOCH_DIFF)
void main(void)
{
    // test_conv(60);
    // test_conv(1594799538LL - UNIX_EPOCH_DIFF);
    data.sync.uptime = 1000;
    data.sync.status.tai = MSEC_PER_SEC * (SYNC_TIME_AFTER_Y2K_UTC + 37); //  July 15, 2020 9:39:28 AM UTC+0, 11:39:28 localtime
    data.sync.status.tai_utc_delta = 37; // Since January 2017
    data.sync.status.time_zone_offset = 8; // GMT+2
    data.sync.status.uncertainty = 1000;
#if 1
    test_localtime(1000);
    test_mktime(11, 39, 28);
    test_mktime(11, 40, 28);
    test_localtime(61000);
    printf("\n-- Turn time back 1 hour -- \n");
    data.time_zone_change.new_offset = 4; // Turn clock back 1 hour, like we do in October
    data.time_zone_change.timestamp = (DST_CHANGE_AFTER_Y2K_UTC + 37);
    // should be unaffected:
    test_localtime(1000);
    test_mktime(11, 39, 28);
    test_mktime(11, 40, 28);
    test_localtime(61000);
    printf("-- After DST change -- \n");
    test_mktime(13, 59, 0);
    test_mktime(15, 1, 0); // should be 2 hours and 2 minutes later
    test_mktime(14, 15, 0);
    test_localtime(8373000ULL);
    test_localtime(15693000ULL);
    test_localtime(9333000ULL);
#endif
    printf("\n-- Turn time forwards 1 hour -- \n");
    data.time_zone_change.new_offset = 12; // Turn clock forwards 1 hour, like we do in March
    data.time_zone_change.timestamp = (DST_CHANGE_AFTER_Y2K_UTC + 37);
    // should be unaffected:
    test_localtime(1000);
    test_mktime(11, 39, 28);
    test_mktime(11, 40, 28);
    test_localtime(61000);
    printf("-- After DST change -- \n");
    test_mktime(13, 59, 0);
    test_mktime(15, 1, 0); // should be 2 minutes later
    test_mktime(16, 1, 0); // should be 1 hour 2 minutes later
    test_mktime(15, 15, 0);
    test_localtime(8373000ULL); // 13:59
    test_localtime(8493000ULL); // 15:01
    test_localtime(12093000ULL); // 16:01
    test_localtime(9333000ULL); // 15:15
}
#endif




