/*
 * Copyright (c) 2024 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Test for the storage_area_store API */

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
#define FLASH_AREA_XIP		FLASH_AREA_OFFSET + 				\
	DT_REG_ADDR(DT_MTD_FROM_FIXED_PARTITION(FLASH_AREA_NODE))
#define AREA_SIZE		DT_REG_SIZE(FLASH_AREA_NODE)
#define AREA_ERASE_SIZE		4096
#define AREA_WRITE_SIZE		512

const static struct storage_area_flash area = flash_storage_area(
	FLASH_AREA_DEVICE, FLASH_AREA_OFFSET, FLASH_AREA_XIP, AREA_WRITE_SIZE,
	AREA_ERASE_SIZE, AREA_SIZE, SA_PROP_LOVRWRITE);
#endif /* CONFIG_STORAGE_AREA_FLASH */

#ifdef CONFIG_STORAGE_AREA_EEPROM
#include <zephyr/storage/storage_area/storage_area_eeprom.h>
#define EEPROM_NODE		DT_ALIAS(eeprom_0)
#define EEPROM_AREA_DEVICE	DEVICE_DT_GET(EEPROM_NODE)
#define AREA_SIZE		DT_PROP(EEPROM_NODE, size)
#define AREA_ERASE_SIZE		1024
#define AREA_WRITE_SIZE		4

const static struct storage_area_eeprom area = eeprom_storage_area(
	EEPROM_AREA_DEVICE, 0U, AREA_WRITE_SIZE, AREA_ERASE_SIZE, AREA_SIZE, 0);
#endif /* CONFIG_STORAGE_AREA_EEPROM */

#ifdef CONFIG_STORAGE_AREA_RAM
#include <zephyr/storage/storage_area/storage_area_ram.h>
#define RAM_NODE		DT_NODELABEL(storage_sram)
#define AREA_SIZE		DT_REG_SIZE(RAM_NODE)
#define AREA_ERASE_SIZE		4096
#define AREA_WRITE_SIZE		4
const static struct storage_area_ram area = ram_storage_area(
	DT_REG_ADDR(RAM_NODE), AREA_WRITE_SIZE, AREA_ERASE_SIZE, AREA_SIZE, 0);
#endif /* CONFIG_STORAGE_AREA_RAM */

const char cookie[]="!NVS";

#define SECTOR_SIZE 1024
create_storage_area_store(test, &area.area, (void *)cookie, sizeof(cookie),
			  SECTOR_SIZE, AREA_SIZE / SECTOR_SIZE,
			  AREA_ERASE_SIZE / SECTOR_SIZE, NULL, NULL, NULL);

create_settings_storage_area_store(test, get_storage_area_store(test));

static void *settings_storage_area_store_api_setup(void)
{
	return NULL;
}

static void settings_storage_area_store_api_before(void *)
{
	int rc = storage_area_erase(&area.area, 0, 1);

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

	uint32_t val = 0x00C0FFEE;
	rc = settings_save_one("data/val", &val, sizeof(val));
	zassert_equal(rc, 0, "save one returned [%d]", rc);
	storage_area_store_report_state("Save one", sstore->sa_store);

	set_cnt = 0U;
	rc = settings_load();
	zassert_equal(rc, 0, "load returned [%d]", rc);
	zassert_equal(set_cnt, 1U, "loaded wrong settings count");

	rc = settings_save_one("data/test", &val, sizeof(val));
	zassert_equal(rc, 0, "save one returned [%d]", rc);
	storage_area_store_report_state("Save one", sstore->sa_store);

	set_cnt = 0U;
	rc = settings_load();
	zassert_equal(rc, 0, "load returned [%d]", rc);
	zassert_equal(set_cnt, 2U, "loaded wrong settings count");

	rc = settings_save_one("data/test", &val, sizeof(val));
	zassert_equal(rc, 0, "save one returned [%d]", rc);
	storage_area_store_report_state("Save one", sstore->sa_store);

	set_cnt = 0U;
	rc = settings_load();
	zassert_equal(rc, 0, "load returned [%d]", rc);
	zassert_equal(set_cnt, 2U, "loaded wrong settings count");

	rc = settings_delete("data/test");
	zassert_equal(rc, 0, "delete returned [%d]", rc);
	storage_area_store_report_state("Delete", sstore->sa_store);

	set_cnt = 0U;
	rc = settings_load();
	zassert_equal(rc, 0, "load returned [%d]", rc);
	zassert_equal(set_cnt, 1U, "loaded wrong settings count");
	
	while (settings_save_one("data/test", &val, sizeof(val)) == 0) {
	 	if (sstore->sa_store->data->wrapcnt != 1) {
	 		break;
	 	}
	}

	storage_area_store_report_state("Wrapped", sstore->sa_store);

	set_cnt = 0U;
	rc = settings_load();
	zassert_equal(rc, 0, "load returned [%d]", rc);
	zassert_equal(set_cnt, 2U, "loaded wrong settings count");

	rc = storage_area_store_unmount(sstore->sa_store);
	zassert_equal(rc, 0, "unmount returned [%d]", rc);

	set_cnt = 0U;
	rc = settings_load();
	zassert_equal(rc, 0, "load returned [%d]", rc);
	zassert_equal(set_cnt, 2U, "loaded wrong settings count");
	storage_area_store_report_state("Remount", sstore->sa_store);

	rc = settings_save_one("other/test", &val, sizeof(val));
	zassert_equal(rc, 0, "save one returned [%d]", rc);
	storage_area_store_report_state("Save one", sstore->sa_store);

	set_cnt = 0U;
	rc = settings_load();
	zassert_equal(rc, 0, "load returned [%d]", rc);
	zassert_equal(set_cnt, 3U, "loaded wrong settings count %d", set_cnt);

	set_cnt = 0U;
	rc = settings_load_subtree("data");
	zassert_equal(rc, 0, "load returned [%d]", rc);
	zassert_equal(set_cnt, 2U, "loaded wrong settings count %d", set_cnt);

	set_cnt = 0U;
	rc = settings_load_subtree("other");
	zassert_equal(rc, 0, "load returned [%d]", rc);
	zassert_equal(set_cnt, 1U, "loaded wrong settings count %d", set_cnt);
}

ZTEST_SUITE(settings_storage_area_store_api, NULL,
	    settings_storage_area_store_api_setup,
	    settings_storage_area_store_api_before, NULL, NULL);