/*
 * Copyright (c) 2024 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Test for the storage_area_store settings backend */

#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>
#include <zephyr/devicetree.h>
#include <zephyr/settings/settings_storage_area_store.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(sas_test);

#ifdef CONFIG_STORAGE_AREA_FLASH
#include <zephyr/storage/storage_area/storage_area_flash.h>
#define FLASH_AREA_NODE		DT_NODELABEL(storage_partition)
#define FLASH_AREA_OFFSET	DT_REG_ADDR(FLASH_AREA_NODE)
#define FLASH_AREA_DEVICE							\
	DEVICE_DT_GET(DT_MTD_FROM_FIXED_PARTITION(FLASH_AREA_NODE))
#define FLASH_AREA_XIP		FLASH_AREA_OFFSET +				\
	DT_REG_ADDR(DT_MTD_FROM_FIXED_PARTITION(FLASH_AREA_NODE))
#define AREA_SIZE		DT_REG_SIZE(FLASH_AREA_NODE)
#define AREA_ERASE_SIZE		4096
#define AREA_WRITE_SIZE		8

STORAGE_AREA_FLASH_RW_DEFINE(test, FLASH_AREA_DEVICE, FLASH_AREA_OFFSET,
	FLASH_AREA_XIP, AREA_WRITE_SIZE, AREA_ERASE_SIZE, AREA_SIZE,
	STORAGE_AREA_PROP_LOVRWRITE | STORAGE_AREA_PROP_AUTOERASE);
#endif /* CONFIG_STORAGE_AREA_FLASH */

const char cookie[]="!NVS";

#define SECTOR_SIZE 1024
STORAGE_AREA_STORE_PCB_DEFINE(test, GET_STORAGE_AREA(test), (void *)cookie,
			      sizeof(cookie), SECTOR_SIZE, AREA_SIZE / SECTOR_SIZE,
			      AREA_ERASE_SIZE / SECTOR_SIZE, 0U);

create_settings_storage_area_store(test, GET_STORAGE_AREA_STORE(test));

static void *settings_storage_area_store_api_setup(void)
{
	return NULL;
}

static void settings_storage_area_store_api_before(void *fixture)
{
	ARG_UNUSED(fixture);
	
	int rc = storage_area_erase(GET_STORAGE_AREA(test), 0, 1);

	zassert_equal(rc, 0, "erase returned [%d]", rc);
}

void storage_area_store_report_state(char *tag,
				     const struct storage_area_store *store)
{
	struct storage_area_store_data *data = store->data;

	LOG_INF("%s: sector: %d - loc:%d - wrapcnt:%d", tag, data->sector,
		data->loc, data->wrapcnt);
}

static size_t set_cnt;

int myset(const char *key, size_t len, settings_read_cb read_cb,
	  void *cb_arg)
{
	set_cnt++;
	return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(data, "data", NULL, myset, NULL, NULL);
SETTINGS_STATIC_HANDLER_DEFINE(other, "other", NULL, myset, NULL, NULL);

ZTEST_USER(settings_storage_area_store_api, test_store)
{
	struct settings_store *store =
		get_settings_storage_area_store_settings_store(test);
	struct settings_storage_area_store *sstore =
		CONTAINER_OF(store, struct settings_storage_area_store, store);
	int rc;

	settings_dst_register(store);
	settings_src_register(store);
	rc = settings_load();
	zassert_equal(rc, 0, "load returned [%d]", rc);

	/* Save a value and see if it can be retrieved */
	uint32_t val = 0x00C0FFEE;
	rc = settings_save_one("data/val", &val, sizeof(val));
	zassert_equal(rc, 0, "save one returned [%d]", rc);
	storage_area_store_report_state("Save one", sstore->sa_store);

	set_cnt = 0U;
	rc = settings_load();
	zassert_equal(rc, 0, "load returned [%d]", rc);
	zassert_equal(set_cnt, 1U, "loaded wrong settings count");

	/* Save a second value and see if both can be retrieved */
	rc = settings_save_one("data/test", &val, sizeof(val));
	zassert_equal(rc, 0, "save one returned [%d]", rc);
	storage_area_store_report_state("Save one", sstore->sa_store);

	set_cnt = 0U;
	rc = settings_load();
	zassert_equal(rc, 0, "load returned [%d]", rc);
	zassert_equal(set_cnt, 2U, "loaded wrong settings count");

	/* Remove a value and check that it is removed */
	rc = settings_delete("data/test");
	zassert_equal(rc, 0, "delete returned [%d]", rc);
	storage_area_store_report_state("Delete", sstore->sa_store);

	set_cnt = 0U;
	rc = settings_load();
	zassert_equal(rc, 0, "load returned [%d]", rc);
	zassert_equal(set_cnt, 1U, "loaded wrong settings count");

	/* Save the same key-value twice, and check that only one is added */
	rc = settings_save_one("data/test", &val, sizeof(val));
	zassert_equal(rc, 0, "save one returned [%d]", rc);

	size_t loc = sstore->sa_store->data->loc;
	rc = settings_save_one("data/test", &val, sizeof(val));
	zassert_equal(rc, 0, "save one returned [%d]", rc);
	zassert_equal(loc, sstore->sa_store->data->loc, "Wrong data loc");
	
	/* Add values until storage is wrapped, no data should be lost */
	while (settings_save_one("data/test", &val, sizeof(val)) == 0) {
	 	if (sstore->sa_store->data->wrapcnt != 1) {
	 		break;
	 	}

		val++;
	}

	storage_area_store_report_state("Wrapped", sstore->sa_store);
	set_cnt = 0U;
	rc = settings_load();
	zassert_equal(rc, 0, "load returned [%d]", rc);
	zassert_equal(set_cnt, 2U, "loaded wrong settings count");

	/* Unmount and automatic remount on load */
	rc = storage_area_store_unmount(sstore->sa_store);
	zassert_equal(rc, 0, "unmount returned [%d]", rc);

	set_cnt = 0U;
	rc = settings_load();
	zassert_equal(rc, 0, "load returned [%d]", rc);
	zassert_equal(set_cnt, 2U, "loaded wrong settings count");
	storage_area_store_report_state("Remount", sstore->sa_store);

	/* Add data with different handler and check load */
	rc = settings_save_one("other/test", &val, sizeof(val));
	zassert_equal(rc, 0, "save one returned [%d]", rc);
	storage_area_store_report_state("Save one", sstore->sa_store);

	set_cnt = 0U;
	rc = settings_load();
	zassert_equal(rc, 0, "load returned [%d]", rc);
	zassert_equal(set_cnt, 3U, "loaded wrong settings count %d", set_cnt);

	/* Check loading of subtree "data", should contain 2 entries */
	set_cnt = 0U;
	rc = settings_load_subtree("data");
	zassert_equal(rc, 0, "load returned [%d]", rc);
	zassert_equal(set_cnt, 2U, "loaded wrong settings count %d", set_cnt);

	/* Check loading of subtree "other", should contain 1 entry */
	set_cnt = 0U;
	rc = settings_load_subtree("other");
	zassert_equal(rc, 0, "load returned [%d]", rc);
	zassert_equal(set_cnt, 1U, "loaded wrong settings count %d", set_cnt);
}

ZTEST_SUITE(settings_storage_area_store_api, NULL,
	    settings_storage_area_store_api_setup,
	    settings_storage_area_store_api_before, NULL, NULL);