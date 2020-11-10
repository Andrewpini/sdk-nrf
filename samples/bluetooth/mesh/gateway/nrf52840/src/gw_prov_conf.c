/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <bluetooth/bluetooth.h>
#include <bluetooth/mesh.h>
#include <settings/settings.h>
#include "gw_prov_conf.h"
#include "prov_conf_serial.h"

static struct unprov_devices unprov_devs;

static const uint16_t net_idx;
static const uint16_t app_idx;
static struct bt_mesh_comp gw_comp_data;
static uint16_t self_addr = 1, added_node_addr;
static const uint8_t dev_uuid[16] = { 0xdd, 0xdd };

K_SEM_DEFINE(sem_node_added, 0, 1);

static void setup_cdb(void)
{
	struct bt_mesh_cdb_app_key *key;

	key = bt_mesh_cdb_app_key_alloc(net_idx, app_idx);
	if (key == NULL) {
		printk("Failed to allocate app-key 0x%04x\n", app_idx);
		return;
	}

	bt_rand(key->keys[0].app_key, 16);

	if (IS_ENABLED(CONFIG_BT_SETTINGS)) {
		bt_mesh_cdb_app_key_store(key);
	}
}

static void configure_self(struct bt_mesh_cdb_node *self)
{
	struct bt_mesh_cdb_app_key *key;
	int err;

	printk("Configuring self...\n");

	key = bt_mesh_cdb_app_key_get(app_idx);
	if (key == NULL) {
		printk("No app-key 0x%04x\n", app_idx);
		return;
	}

	/* Add Application Key */
	err = bt_mesh_cfg_app_key_add(self->net_idx, self->addr, self->net_idx,
				      app_idx, key->keys[0].app_key, NULL);
	if (err < 0) {
		printk("Failed to add app-key (err %d)\n", err);
		return;
	}

	err = bt_mesh_cfg_mod_app_bind(self->net_idx, self->addr, self->addr,
				       app_idx, BT_MESH_MODEL_ID_HEALTH_CLI,
				       NULL);
	if (err < 0) {
		printk("Failed to bind health client app-key (err %d)\n", err);
		return;
	}

	err = bt_mesh_cfg_mod_app_bind(self->net_idx, self->addr, (self->addr + 1),
				       app_idx, BT_MESH_MODEL_ID_GEN_LEVEL_CLI,
				       NULL);
	if (err < 0) {
		printk("Failed to bind level client app-key (err %d)\n", err);
		return;
	}

	err = bt_mesh_cfg_mod_app_bind(self->net_idx, self->addr, (self->addr + 1),
				       app_idx, BT_MESH_MODEL_ID_GEN_ONOFF_CLI,
				       NULL);
	if (err < 0) {
		printk("Failed to bind onoff client app-key (err %d)\n", err);
		return;
	}

	err = bt_mesh_cfg_mod_app_bind(self->net_idx, self->addr, (self->addr + 1),
				       app_idx, BT_MESH_MODEL_ID_GEN_ONOFF_SRV,
				       NULL);
	if (err < 0) {
		printk("Failed to bind onoff server app-key (err %d)\n", err);
		return;
	}

	// TODO: bt_mesh_cfg_mod_sub_add() and bt_mesh_cfg_mod_pub_set()

	atomic_set_bit(self->flags, BT_MESH_CDB_NODE_CONFIGURED);

	if (IS_ENABLED(CONFIG_BT_SETTINGS)) {
		bt_mesh_cdb_node_store(self);
	}

	printk("Configuration complete\n");
}

static void configure_node(struct bt_mesh_cdb_node *node)
{
	struct bt_mesh_cdb_app_key *key;
	struct bt_mesh_cfg_mod_pub pub;
	uint8_t status;
	int err;

	printk("Configuring node 0x%04x...\n", node->addr);

	key = bt_mesh_cdb_app_key_get(app_idx);
	if (key == NULL) {
		printk("No app-key 0x%04x\n", app_idx);
		return;
	}

	/* Add Application Key */
	err = bt_mesh_cfg_app_key_add(net_idx, node->addr, net_idx, app_idx,
				      key->keys[0].app_key, NULL);
	if (err < 0) {
		printk("Failed to add app-key (err %d)\n", err);
		return;
	}

	/* Bind to Health model */
	err = bt_mesh_cfg_mod_app_bind(net_idx, node->addr, node->addr, app_idx,
				       BT_MESH_MODEL_ID_HEALTH_SRV, NULL);
	if (err < 0) {
		printk("Failed to bind app-key (err %d)\n", err);
		return;
	}

	pub.addr = 1;
	pub.app_idx = key->app_idx;
	pub.cred_flag = false;
	pub.ttl = 7;
	pub.period = BT_MESH_PUB_PERIOD_10SEC(1);
	pub.transmit = 0;

	err = bt_mesh_cfg_mod_pub_set(net_idx, node->addr, node->addr,
				      BT_MESH_MODEL_ID_HEALTH_SRV, &pub,
				      &status);
	if (err < 0) {
		printk("mod_pub_set %d, %d\n", err, status);
		return;
	}

	atomic_set_bit(node->flags, BT_MESH_CDB_NODE_CONFIGURED);

	if (IS_ENABLED(CONFIG_BT_SETTINGS)) {
		bt_mesh_cdb_node_store(node);
	}

	printk("Configuration complete\n");
}

static void unprovisioned_beacon(uint8_t uuid[16],
				 bt_mesh_prov_oob_info_t oob_info,
				 uint32_t *uri_hash)
{
	bool dev_in_list = false;

	if (unprov_devs.number == 0) {
		memcpy(unprov_devs.dev[0].uuid, uuid, 16);
		unprov_devs.dev[0].time = 0;
		unprov_devs.number++;
		printk("%d unprovisioned node\n", unprov_devs.number);
	} else if (unprov_devs.number < MAX_UNPROV_DEVICES) {
		for (int i = 0; i < unprov_devs.number; i++) {
			if (!memcmp(unprov_devs.dev[i].uuid, uuid, 16)) {
				dev_in_list = true;
				unprov_devs.dev[i].time = 0;
			}
		}

		if (!dev_in_list) {
			memcpy(unprov_devs.dev[unprov_devs.number].uuid, uuid, 16);
			unprov_devs.dev[unprov_devs.number].time = 0;
			unprov_devs.number++;
			printk("%d unprovisioned nodes\n", unprov_devs.number);
		}
	}

	// char uuid_hex_str[32 + 1];
	// bin2hex(uuid, 16, uuid_hex_str, sizeof(uuid_hex_str));
	// printk("Unprov beacon: %s - %X\n", uuid_hex_str, oob_info);
}

static void node_added(uint16_t net_idx, uint8_t uuid[16], uint16_t addr,
		       uint8_t num_elem)
{
	added_node_addr = addr;

	for (int i = 0; i < unprov_devs.number; i++) {
		if (!memcmp(unprov_devs.dev[i].uuid, uuid, 16)) {
			for (int j = i; j < unprov_devs.number; j++) {
				unprov_devs.dev[j] = unprov_devs.dev[j + 1];
			}

			unprov_devs.number--;

			break;
		}
	}

	k_sem_give(&sem_node_added);
}

static const struct bt_mesh_prov prov = {
	.uuid = dev_uuid,
	.unprovisioned_beacon = unprovisioned_beacon,
	.node_added = node_added,
};

static uint8_t check_unconfigured(struct bt_mesh_cdb_node *node, void *data)
{
	if (!atomic_test_bit(node->flags, BT_MESH_CDB_NODE_CONFIGURED)) {
		if (node->addr == self_addr) {
			configure_self(node);
		} else {
			configure_node(node);
		}
	}

	return BT_MESH_CDB_ITER_CONTINUE;
}

static struct k_delayed_work prov_beac_timeout_work;

static void prov_beac_timeout_work_handler(struct k_work *work)
{
	for (int i = 0; i < unprov_devs.number; i++) {
		unprov_devs.dev[i].time++;

		if(unprov_devs.dev[i].time > UNPROV_BEAC_TIMEOUT) {
			for (int j = i; j < unprov_devs.number; j++) {
				unprov_devs.dev[j] = unprov_devs.dev[j + 1];
			}

			unprov_devs.number--;
		}
	}

	k_delayed_work_submit(&prov_beac_timeout_work, K_MSEC(1000));
}

void set_comp_data(const struct bt_mesh_comp comp_data)
{
	gw_comp_data = comp_data;
}

void bt_ready(int err)
{
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	uint8_t net_key[16], dev_key[16];

	err = bt_mesh_init(&prov, &gw_comp_data);
	if (err) {
		printk("Initializing mesh failed (err %d)\n", err);
		return;
	}

	printk("Mesh initialized\n");

	if (IS_ENABLED(CONFIG_BT_SETTINGS)) {
		printk("Loading stored settings\n");
		settings_load();
	}

	bt_rand(net_key, 16);

	err = bt_mesh_cdb_create(net_key);
	if (err == -EALREADY) {
		printk("Using stored CDB\n");
	} else if (err) {
		printk("Failed to create CDB (err %d)\n", err);
		return;
	} else {
		printk("Created CDB\n");
		setup_cdb();
	}

	bt_rand(dev_key, 16);

	err = bt_mesh_provision(net_key, BT_MESH_NET_PRIMARY, 0, 0, self_addr,
				dev_key);
	if (err == -EALREADY) {
		printk("Using stored settings\n");
	} else if (err) {
		printk("Provisioning failed (err %d)\n", err);
		return;
	} else {
		printk("Provisioning completed\n");
	}

	bt_mesh_cdb_node_foreach(check_unconfigured, NULL);

	k_delayed_work_init(&prov_beac_timeout_work, prov_beac_timeout_work_handler);
	k_delayed_work_submit(&prov_beac_timeout_work, K_NO_WAIT);

	return;
}

int provision_device(uint8_t dev_num, const uint8_t *uuid)
{
	char uuid_hex_str[32 + 1];
	int err;

	if (bt_mesh_prov_link_active()) {
		return -1;
	}

	k_sem_reset(&sem_node_added);

	if (uuid == NULL) {
		bin2hex(unprov_devs.dev[dev_num].uuid, 16, uuid_hex_str, sizeof(uuid_hex_str));

		printk("Provisioning %s\n", uuid_hex_str);

		err = bt_mesh_provision_adv(unprov_devs.dev[dev_num].uuid, net_idx, 0, 0, false);
		if (err < 0) {
			printk("Provisioning failed (err %d)\n", err);
			return err;
		}
	} else {
		bin2hex(uuid, 16, uuid_hex_str, sizeof(uuid_hex_str));

		printk("Provisioning %s\n", uuid_hex_str);
		err = bt_mesh_provision_adv(uuid, net_idx, 0, 0, false);
		if (err < 0) {
			printk("Provisioning failed (err %d)\n", err);
			return err;
		}
	}

	printk("Waiting for node to be added...\n");
	err = k_sem_take(&sem_node_added, K_SECONDS(10));
	if (err == -EAGAIN) {
		printk("Timeout waiting for node to be added\n");
		return err;
	}

	printk("Added node 0x%04x\n", added_node_addr);

	bt_mesh_cdb_node_foreach(check_unconfigured, NULL);

	k_sleep(K_MSEC(3000)); // TODO: Improve this?

	return err;
}

// struct bt_mesh_cdb_node *node = bt_mesh_cdb_node_get(added_node_addr);

int get_model_info(struct model_info *mod_inf)
{
	NET_BUF_SIMPLE_DEFINE(comp, 64);
	uint8_t status;
	int err, i;

	/* Only page 0 is currently implemented */
	err = bt_mesh_cfg_comp_data_get(net_idx, added_node_addr, 0x00,
					&status, &comp);
	if (err) {
		printk("Getting composition failed (err %d)\n", err);
		return err;
	}

	if (status != 0x00) {
		printk("Got non-success status 0x%02x\n", status);
		return status;
	}

	printk("Composition Data for 0x%04x:\n", added_node_addr);
	printk("\tCID      0x%04x\n",
		    net_buf_simple_pull_le16(&comp));
	printk("\tPID      0x%04x\n",
		    net_buf_simple_pull_le16(&comp));
	printk("\tVID      0x%04x\n",
		    net_buf_simple_pull_le16(&comp));
	printk("\tCRPL     0x%04x\n",
		    net_buf_simple_pull_le16(&comp));
	printk("\tFeatures 0x%04x\n",
		    net_buf_simple_pull_le16(&comp));

	while (comp.len > 4) {
		uint8_t sig, vnd;
		uint16_t loc;

		loc = net_buf_simple_pull_le16(&comp);
		sig = net_buf_simple_pull_u8(&comp);
		vnd = net_buf_simple_pull_u8(&comp);

		printk("\tElement @ 0x%04x:\n", loc);

		if (comp.len < ((sig * 2U) + (vnd * 4U))) {
			printk("\t\t...truncated data!\n");
			break;
		}

		if (sig) {
			printk("\t\tSIG Models:\n");
		} else {
			printk("\t\tNo SIG Models\n");
		}

		for (i = 0; i < sig; i++) {
			uint16_t mod_id = net_buf_simple_pull_le16(&comp);

			if (mod_id == BT_MESH_MODEL_ID_GEN_ONOFF_SRV) {
				mod_inf->srv_count++;
			} else if (mod_id == BT_MESH_MODEL_ID_GEN_ONOFF_CLI) {
				mod_inf->cli_count++;
			}

			printk("\t\t\t0x%04x\n", mod_id);
		}

		if (vnd) {
			printk("\t\tVendor Models:\n");
		} else {
			printk("\t\tNo Vendor Models\n");
		}

		for (i = 0; i < vnd; i++) {
			uint16_t cid = net_buf_simple_pull_le16(&comp);
			uint16_t mod_id = net_buf_simple_pull_le16(&comp);

			printk("\t\t\tCompany 0x%04x: 0x%04x\n", cid,
				    mod_id);
		}
	}

	return 0;
}

int configure_server(uint8_t elem_num, uint16_t group_addr)
{
	int err;

	err = bt_mesh_cfg_mod_app_bind(net_idx, added_node_addr, (added_node_addr + elem_num), app_idx, BT_MESH_MODEL_ID_GEN_ONOFF_SRV, NULL);
	if (err) {
		printk("Error binding app key to server (%d)\n", err);
		return err;
	}

	err = bt_mesh_cfg_mod_sub_add(net_idx, added_node_addr, (added_node_addr + elem_num), group_addr, BT_MESH_MODEL_ID_GEN_ONOFF_SRV, NULL);
	if (err) {
		printk("Error setting server subscription address (%d)\n", err);
		return err;
	}

	printk("Server model %d configured\n", elem_num);

	return 0;
}

int configure_client(uint8_t elem_num, uint16_t group_addr)
{
	int err;

	struct bt_mesh_cfg_mod_pub pub_params = {
		.addr = group_addr,
		.app_idx = app_idx,
		.cred_flag = false,
		.ttl = 7,
		.period = 0,
		.transmit = 0,
	};

	err = bt_mesh_cfg_mod_app_bind(net_idx, added_node_addr, (added_node_addr + elem_num), app_idx, BT_MESH_MODEL_ID_GEN_ONOFF_CLI, NULL);
	if (err) {
		printk("Error binding app key to client (%d)\n", err);
		return err;
	}

	err = bt_mesh_cfg_mod_pub_set(net_idx, added_node_addr, (added_node_addr + elem_num), BT_MESH_MODEL_ID_GEN_ONOFF_CLI, &pub_params, NULL);
	if (err) {
		printk("Error setting client publishing parameters (%d)\n", err);
		return err;
	}

	printk("Client model %d configured\n", elem_num);

	return 0;
}

void blink_device(uint8_t dev_num) 
{
	int err;

	err = bt_mesh_provision_adv(unprov_devs.dev[dev_num].uuid, net_idx, 0, 100, true);
	if (err == -16) {
		printk("Provision link still active...\n");
	} else if (err < 0) {
		printk("provision_adv failed (%d)\n", err);
	}
}

uint8_t get_unprov_dev_num(void) 
{
	return unprov_devs.number;
}

bool prov_link_active(void)
{
	return bt_mesh_prov_link_active();
}
