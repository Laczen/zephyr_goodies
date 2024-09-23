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
#include <zephyr/storage/storage_area/storage_area_flash.h>
#include <zephyr/storage/storage_area/storage_area_store.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(sas_test);

#define FLASH_AREA_NODE		DT_NODELABEL(storage_partition)
#define FLASH_AREA_OFFSET	DT_REG_ADDR(FLASH_AREA_NODE)
#define FLASH_AREA_SIZE		DT_REG_SIZE(FLASH_AREA_NODE)
#define FLASH_AREA_DEVICE							\
	DEVICE_DT_GET(DT_MTD_FROM_FIXED_PARTITION(FLASH_AREA_NODE))
#define FLASH_AREA_XIP		FLASH_AREA_OFFSET + 				\
	DT_REG_ADDR(DT_MTD_FROM_FIXED_PARTITION(FLASH_AREA_NODE))
#define FLASH_AREA_WBS		8

const static struct storage_area_flash flash_area = flash_storage_area(
	FLASH_AREA_DEVICE, FLASH_AREA_OFFSET, FLASH_AREA_XIP, FLASH_AREA_WBS,
	4096, FLASH_AREA_SIZE, 0);

const char cookie[]="!NVS";
bool move(const struct storage_area_record *record) {
	if (record->sector == 0U) {
		return true;
	}

	return false;
}

void move_cb(const struct storage_area_record *src,
	     const struct storage_area_record *dst)
{
	LOG_INF("Moved %d-%d to %d-%d", src->sector, src->loc, dst->sector,
		dst->loc);
}

create_storage_area_store(test, &flash_area.area, (void *)cookie,
	sizeof(cookie), 1024, 8, 4, move, move_cb, NULL);

static void *storage_area_store_api_setup(void)
{
	return NULL;
}

static void storage_area_store_api_before(void *)
{
	int rc = storage_area_erase(&flash_area.area, 0, 1);
	zassert_equal(rc, 0, "erase returned [%d]", rc);
}

void storage_area_store_report_state(char *tag,
				     const struct storage_area_store *store)
{
	struct storage_area_store_data *data = store->data;

	LOG_INF("%s: sector: %d - loc:%d - wrapcnt:%d", tag, data->sector,
		data->loc, data->wrapcnt);
}

ZTEST_USER(storage_area_store_api, test_store)
{
	char name[] = "mydata";
	size_t value = 0xF0F0;
	struct storage_area_chunk wr[] = {
		{
			.data = &name,
			.len = sizeof(name),
		}, {
			.data = &value,
			.len = sizeof(value),
		},
	};
	struct storage_area_record record = {
		.store = NULL
	};
	struct storage_area_store *store = get_storage_area_store(test);
	int rc;

	rc = storage_area_store_mount(store);
	zassert_equal(rc, 0, "mount returned [%d]", rc);
	storage_area_store_report_state("Mount", store);

	rc = storage_area_store_write(store, wr, ARRAY_SIZE(wr));
	zassert_equal(rc, 0, "write returned [%d]", rc);
	storage_area_store_report_state("Write", store);
	
	rc = storage_area_store_unmount(store);
	zassert_equal(rc, 0, "unmount returned [%d]", rc);
	storage_area_store_report_state("Unmount", store);

	rc = storage_area_store_mount(store);
	zassert_equal(rc, 0, "mount returned [%d]", rc);
	storage_area_store_report_state("Mount", store);

	for (int i = 0; i < 8; i++) {
		while (storage_area_store_write(store, wr, ARRAY_SIZE(wr)) == 0);
		storage_area_store_report_state("Write", store);
		rc = storage_area_store_compact(store);
		zassert_equal(rc, 0, "advance returned [%d]", rc);
		storage_area_store_report_state("Compact", store);
	}

	rc = storage_area_store_unmount(store);
	zassert_equal(rc, 0, "unmount returned [%d]", rc);
	storage_area_store_report_state("Unmount", store);

	rc = storage_area_store_mount(store);
	zassert_equal(rc, 0, "mount returned [%d]", rc);
	storage_area_store_report_state("Mount", store);
	
	rc = storage_area_store_compact(store);
	zassert_equal(rc, 0, "compact returned [%d]", rc);
	storage_area_store_report_state("Compact", store);

	rc = storage_area_record_next(store, &record);
	zassert_equal(rc, 0, "record next returned [%d]", rc);

	rc = storage_area_record_read(&record, 0U, wr, ARRAY_SIZE(wr));
	zassert_equal(rc, 0, "record read returned [%d]", rc);

	rc = storage_area_store_compact(store);
}

ZTEST_SUITE(storage_area_store_api, NULL, storage_area_store_api_setup,
	    storage_area_store_api_before, NULL, NULL);