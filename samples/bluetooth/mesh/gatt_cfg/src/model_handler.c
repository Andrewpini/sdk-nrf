/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdio.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/mesh/models.h>
#include <dk_buttons_and_leds.h>

#include <shell/shell.h>
#include <shell/shell_uart.h>

#include "model_handler.h"
#include <logging/log.h>
LOG_MODULE_DECLARE(chat);

static const struct shell *chat_shell;

#define SHELL_PROXY_INTERFACE_LINK_ENTRY 6661
#define SHELL_PROXY_INTERFACE_LINK_UPDATE_STARTED 6662
#define SHELL_PROXY_INTERFACE_LINK_UPDATE_ENDED 6663
#define SHELL_PROXY_INTERFACE_LINK_CNT 6664
#define SHELL_PROXY_INTERFACE_LINK_ENTRY_STATUS 6665


/******************************************************************************/
/*************************** Health server setup ******************************/
/******************************************************************************/
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

/******************************************************************************/
/***************************** Chat model setup *******************************/
/******************************************************************************/


static void status_handler(struct bt_mesh_gpc_cli *cli,
			   struct bt_mesh_msg_ctx *ctx,
			   enum bt_mesh_gpc_status_type status,
			   uint8_t err_code)
{
	char lladdr[124] = {0};
	switch (status)
	{
	case BT_MESH_GPC_LINK_UPDATE_STARTED:
		printk("BT_MESH_GPC_LINK_UPDATE_STARTED %d\n", ctx->addr);
		sprintf(lladdr,"%d-%d-", SHELL_PROXY_INTERFACE_LINK_UPDATE_STARTED, ctx->addr);
		printk("%s\n", lladdr);
		break;

	case BT_MESH_GPC_LINK_UPDATE_ENDED:
		printk("BT_MESH_GPC_LINK_UPDATE_ENDED %d\n", ctx->addr);
		sprintf(lladdr,"%d-%d-", SHELL_PROXY_INTERFACE_LINK_UPDATE_ENDED, ctx->addr);
		printk("%s\n", lladdr);
		break;

	case BT_MESH_GPC_CONN_ADD:
		printk("BT_MESH_GPC_CONN_ADD addr:%d, err_code:%d\n", ctx->addr, err_code);
		break;

	case BT_MESH_GPC_CONN_RESET:
		printk("BT_MESH_GPC_CONN_RESET addr:%d, err_code:%d\n", ctx->addr, err_code);
		break;
	default:
		break;
	}
}

static struct bt_mesh_gpc_cli gpc_cli =
	BT_MESH_GPC_CLI_INIT(&status_handler);

static struct bt_mesh_elem elements[] = {
	BT_MESH_ELEM(1,
		     BT_MESH_MODEL_LIST(
			     BT_MESH_MODEL_CFG_SRV,
			     BT_MESH_MODEL_HEALTH_SRV(&health_srv, &health_pub),
			     BT_MESH_MODEL_GPC_CLI(&gpc_cli)),
		     BT_MESH_MODEL_NONE),
};

static const struct bt_mesh_comp comp = {
	.cid = CONFIG_BT_COMPANY_ID,
	.elem = elements,
	.elem_count = ARRAY_SIZE(elements),
};

/******************************************************************************/
/******************************** Chat shell **********************************/
/******************************************************************************/

static int cmd_gpc(const struct shell *shell, size_t argc,
			       char *argv[])
{
	uint16_t addr;
	uint8_t net_id;
	bool onoff;
	int err;

	if (argc < 4) {
		return -EINVAL;
	}

	addr = strtol(argv[1], NULL, 0);
	net_id = strtol(argv[3], NULL, 0);
	onoff = (bool)strtol(argv[2], NULL, 0);

	/* Print own message to the chat. */
	shell_print(shell, "<you>: *0x%04X* %d", addr, onoff);

	struct bt_mesh_gpc_adv_set set = {
		.on_off = onoff,
		.net_id = net_id,
	};

	struct bt_mesh_msg_ctx ctx = {
		.addr = addr,
		.send_ttl = BT_MESH_TTL_DEFAULT,
		.app_idx = 0,
	};

	// err = bt_mesh_gpc_cli_set(&gpc_cli, &ctx, &set, NULL);
	err = bt_mesh_gpc_cli_adv_set(&gpc_cli, &ctx, &set, NULL);
	if (err) {
		LOG_WRN("Failed to publish message: %d", err);
	}

	return 0;
}

static int cmd_gatt_conn_cfg(const struct shell *shell, size_t argc,
			       char *argv[])
{
	uint16_t dst_addr;
	uint16_t addr;
	uint8_t net_id;
	int err;

	if (argc < 4) {
		return -EINVAL;
	}

	dst_addr = strtol(argv[1], NULL, 0);
	addr = strtol(argv[2], NULL, 0);
	net_id = strtol(argv[3], NULL, 0);

	/* Print own message to the chat. */
	shell_print(shell, "<you>: *0x%04X*", addr);

	struct bt_mesh_gpc_conn_set set = {
		.addr = addr,
		.net_id = net_id,
	};

	struct bt_mesh_msg_ctx ctx = {
		.addr = dst_addr,
		.send_ttl = BT_MESH_TTL_DEFAULT,
		.app_idx = 0,
	};

	err = bt_mesh_gpc_cli_conn_set(&gpc_cli, &ctx, &set, NULL);
	if (err) {
		LOG_WRN("Failed to publish message: %d", err);
	}

	return 0;
}

static int cmd_gatt_adv_enable(const struct shell *shell, size_t argc,
			       char *argv[])
{
	uint16_t dst_addr;
	bool onoff;
	int err;

	if (argc < 3) {
		return -EINVAL;
	}

	dst_addr = strtol(argv[1], NULL, 0);
	enum bt_mesh_proxy_cli_adv_state state = strtol(argv[2], NULL, 0);

	/* Print own message to the chat. */
	shell_print(shell, "<you>: *0x%04X*", dst_addr);


	struct bt_mesh_msg_ctx ctx = {
		.addr = dst_addr,
		.send_ttl = BT_MESH_TTL_DEFAULT,
		.app_idx = 0,
	};

	err = bt_mesh_gpc_cli_adv_enable(&gpc_cli, &ctx, (enum bt_mesh_proxy_cli_adv_state)state, NULL);
	if (err) {
		LOG_WRN("Failed to publish message: %d", err);
	}

	return 0;
}

static int cmd_gatt_link_init(const struct shell *shell, size_t argc,
			       char *argv[])
{
	if (argc < 1) {
		return -EINVAL;
	}

	uint16_t dst_addr = strtol(argv[1], NULL, 0);
	uint8_t msg_cnt = strtol(argv[2], NULL, 0);

	/* Print own message to the chat. */
	shell_print(shell, "<you>: *0x%04X*", dst_addr);

	printk("%d-%d-\n", SHELL_PROXY_INTERFACE_LINK_CNT, msg_cnt);

	struct bt_mesh_msg_ctx ctx = {
		.addr = dst_addr,
		.send_ttl = BT_MESH_TTL_DEFAULT,
		.app_idx = 0,
	};

	int err = bt_mesh_gpc_cli_link_init(&gpc_cli, &ctx, msg_cnt);
	if (err) {
		LOG_WRN("Failed to publish message: %d", err);
	}

	return 0;
}

static int cmd_gatt_link_fetch(const struct shell *shell, size_t argc,
			       char *argv[])
{
	if (argc < 1) {
		return -EINVAL;
	}

	uint16_t dst_addr = strtol(argv[1], NULL, 0);

	/* Print own message to the chat. */
	shell_print(shell, "<you>: *0x%04X*", dst_addr);

	struct bt_mesh_msg_ctx ctx = {
		.addr = dst_addr,
		.send_ttl = BT_MESH_TTL_DEFAULT,
		.app_idx = 0,
	};

	struct link_data_entry entry = {0};
	int err = bt_mesh_gpc_cli_link_fetch(&gpc_cli, &ctx, &entry);
	if (err) {
		LOG_WRN("Failed to publish message: %d", err);
	}

	printk("Root Addr: %d\n", entry.src);
	for (size_t i = 0; i < entry.entry_cnt; i++)
	{
		// printk("\tAddr: %d, Cnt: %d\n", entry.data[i].root_addr, entry.data[i].received_cnt);

		char lladdr[124] = {0};
		sprintf(lladdr,"%d-%d-%d-%d-", SHELL_PROXY_INTERFACE_LINK_ENTRY, entry.src, entry.data[i].root_addr, entry.data[i].received_cnt);
		printk("%s\n", lladdr);
	}

	printk("%d-%d-\n", SHELL_PROXY_INTERFACE_LINK_ENTRY_STATUS, entry.entry_cnt);

	return 0;
}

static int cmd_gatt_connect(const struct shell *shell, size_t argc,
			       char *argv[])
{
	uint16_t cli_addr;
	uint16_t srv_addr;

	int err;

	if (argc < 2) {
		return -EINVAL;
	}

	cli_addr = strtol(argv[1], NULL, 0);
	srv_addr = strtol(argv[2], NULL, 0);

	/* Print own message to the chat. */
	// shell_print(shell, "<you>: *0x%04X*", dst_addr);


	struct bt_mesh_msg_ctx ctx = {
		.addr = cli_addr,
		.send_ttl = BT_MESH_TTL_DEFAULT,
		.app_idx = 0,
	};

	struct bt_mesh_gpc_conn_set set_conn = {
		.addr = srv_addr,
		.net_id = 0,
	};
	err = bt_mesh_gpc_cli_conn_set(&gpc_cli, &ctx, &set_conn, NULL);

	struct bt_mesh_gpc_adv_set set = {
		.on_off = true,
		.net_id = 0,
	};
	ctx.addr = srv_addr;
	err = bt_mesh_gpc_cli_adv_set(&gpc_cli, &ctx, &set, NULL);

	if (err) {
		LOG_WRN("Failed to publish message: %d", err);
	}

	return 0;
}

static int cmd_gatt_conn_reset(const struct shell *shell, size_t argc,
			       char *argv[])
{
	if (argc < 1) {
		return -EINVAL;
	}

	uint16_t dst_addr = strtol(argv[1], NULL, 0);

	struct bt_mesh_msg_ctx ctx = {
		.addr = dst_addr,
		.send_ttl = BT_MESH_TTL_DEFAULT,
		.app_idx = 0,
	};

	int err = bt_mesh_gpc_cli_conn_reset(&gpc_cli, &ctx);
	if (err) {
		LOG_WRN("Failed to publish message: %d", err);
	}

	return 0;
}

static int cmd_test_msg(const struct shell *shell, size_t argc,
			       char *argv[])
{
	if (argc < 1) {
		return -EINVAL;
	}

	uint16_t dst_addr = strtol(argv[1], NULL, 0);
	bool onoff = (bool)strtol(argv[2], NULL, 0);

	struct bt_mesh_msg_ctx ctx = {
		.addr = dst_addr,
		.send_ttl = BT_MESH_TTL_DEFAULT,
		.app_idx = 0,
	};

	int err = bt_mesh_gpc_cli_test_msg_init(&gpc_cli, &ctx, onoff);
	if (err) {
		LOG_WRN("Failed to publish message: %d", err);
	}

	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(
	chat_cmds,
	SHELL_CMD_ARG(adv_set, NULL, "Set the GATT config adv <addr> <onoff> <net_idx>",
		      cmd_gpc, 4, 0),
	SHELL_CMD_ARG(conn_set, NULL, "Set the GATT config <dst_addr> <addr> <net_idx>",
		      cmd_gatt_conn_cfg, 4, 0),
	SHELL_CMD_ARG(adv_enable, NULL, "Set the state for the advertiser <dst_addr> <onoff>",
		      cmd_gatt_adv_enable, 3, 0),
	SHELL_CMD_ARG(link_init, NULL, "Init link mapping",
		      cmd_gatt_link_init, 3, 0),
	SHELL_CMD_ARG(link_fetch, NULL, "Fetch link mapping",
		      cmd_gatt_link_fetch, 2, 0),
	SHELL_CMD_ARG(connect, NULL, "Connect two devices <cli_addr> <srv_addr>",
		      cmd_gatt_connect, 3, 0),
	SHELL_CMD_ARG(conn_reset, NULL, "Reset connection list",
		      cmd_gatt_conn_reset, 2, 0),
	SHELL_CMD_ARG(test_msg, NULL, "Initiate sending of test message from a node <addr> <onoff>",
		      cmd_test_msg, 3, 0),
	SHELL_SUBCMD_SET_END);

static int cmd_chat(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		shell_help(shell);
		/* shell returns 1 when help is printed */
		return 1;
	}

	shell_error(shell, "%s unknown parameter: %s", argv[0], argv[1]);

	return -EINVAL;
}

SHELL_CMD_ARG_REGISTER(cfg, &chat_cmds, "Bluetooth Mesh GATT configuring terminal",
		       cmd_chat, 1, 1);

/******************************************************************************/
/******************************** Public API **********************************/
/******************************************************************************/
const struct bt_mesh_comp *model_handler_init(void)
{
	k_delayed_work_init(&attention_blink_work, attention_blink);

	chat_shell = shell_backend_uart_get_ptr();
	shell_print(chat_shell, ">>> Bluetooth Mesh Chat sample <<<");

	return &comp;

}
