/*
 * Copyright (c) 2024 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Sample which uses the filesystem API with littlefs */

#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>
#include <zephyr/devicetree.h>
#include <zephyr/storage/storage_area/storage_area_flash.h>
#include <zephyr/storage/storage_area/storage_area_eeprom.h>
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
const static struct storage_area *area = &flash_area.area;

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
	LOG_INF("Moved %d-%d to %d-%d", src->sector, src->loc, dst->sector, dst->loc);
}

static struct storage_area_store_data data;
const static struct storage_area_store store = {
	.area = &flash_area.area,
	.sector_size = 1024,
	.sector_cnt = 8,
	.spare_sectors = 4,
	.data = &data,
	.move = move,
	.move_cb = move_cb,
	.sector_cookie = (void *)cookie,
	.sector_cookie_size = sizeof(cookie),
};

static void *storage_area_api_setup(void)
{
	return NULL;
}

static void storage_area_api_before(void *)
{
	int rc = storage_area_erase(area, 0, 1);
	zassert_equal(rc, 0, "erase returned [%d]", rc);
}

ZTEST_USER(storage_area_api, test_read_write_simple)
{
	int rc;
	uint8_t wr[STORAGE_AREA_WRITESIZE(area)];
	uint8_t rd[STORAGE_AREA_WRITESIZE(area)];
	struct storage_area_chunk rd_chunk = {
		.data = &rd,
		.len = sizeof(rd),
	};
	struct storage_area_chunk wr_chunk = {
		.data = &wr,
		.len = sizeof(wr),
	};


	memset(wr, 'T', sizeof(wr));
	memset(rd, 0, sizeof(rd));

	rc = storage_area_prog(area, 0U, &wr_chunk, 1U);
	zassert_equal(rc, 0, "prog returned [%d]", rc);

	rc = storage_area_read(area, 0U, &rd_chunk, 1U);
	zassert_equal(rc, 0, "read returned [%d]", rc);

	zassert_equal(memcmp(rd, wr, sizeof(wr)), 0, "data mismatch");
}

ZTEST_USER(storage_area_api, test_read_write_blocks)
{
	int rc;
	uint8_t magic = 0xA0;
	uint8_t wr[STORAGE_AREA_WRITESIZE(area)];
	uint8_t rd[STORAGE_AREA_WRITESIZE(area)];
	uint8_t fill[STORAGE_AREA_WRITESIZE(area) - 1];
	struct storage_area_chunk rd_chunk[] =
	{
		{
			.data = (void *)&magic,
			.len = sizeof(magic),
		},
		{
			.data = (void *)&rd,
			.len = sizeof(rd),
		},
	};
	struct storage_area_chunk wr_chunk[] =
	{
		{
			.data = (void *)&magic,
			.len = sizeof(magic),
		},
		{
			.data = (void *)&wr,
			.len = sizeof(wr),
		},
		{
			.data = (void *)&fill,
			.len = sizeof(fill),
		},
	};

	memset(fill, 0xff, sizeof(fill));
	memset(wr, 'T', sizeof(wr));
	memset(rd, 0, sizeof(rd));

	rc = storage_area_prog(area, 0U, wr_chunk, ARRAY_SIZE(wr_chunk));
	zassert_equal(rc, 0, "prog returned [%d]", rc);

	rc = storage_area_read(area, 0U, rd_chunk, ARRAY_SIZE(rd_chunk));
	zassert_equal(rc, 0, "read returned [%d]", rc);

	zassert_equal(magic, 0xA0, "magic has changed");
	zassert_equal(memcmp(rd, wr, sizeof(wr)), 0, "data mismatch");

}

ZTEST_USER(storage_area_api, test_store)
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
	int rc;

	rc = storage_area_store_mount(&store);
	zassert_equal(rc, 0, "mount returned [%d]", rc);
	LOG_INF("STORE Data: sector: %d, loc: %d", data.sector, data.loc);

	rc = storage_area_store_write(&store, wr, ARRAY_SIZE(wr));
	zassert_equal(rc, 0, "write returned [%d]", rc);
	LOG_INF("STORE Data: sector: %d, loc: %d", data.sector, data.loc);
	
	rc = storage_area_store_unmount(&store);
	zassert_equal(rc, 0, "unmount returned [%d]", rc);
	LOG_INF("STORE Data: sector: %d, loc: %d", data.sector, data.loc);

	rc = storage_area_store_mount(&store);
	zassert_equal(rc, 0, "mount returned [%d]", rc);
	LOG_INF("STORE Data: sector: %d, loc: %d", data.sector, data.loc);

	for (int i = 0; i < 8; i++) {
		while (storage_area_store_write(&store, wr, ARRAY_SIZE(wr)) == 0);
		LOG_INF("STORE Data: sector: %d, loc: %d", data.sector, data.loc);
		rc = storage_area_store_compact(&store);
		zassert_equal(rc, 0, "advance returned [%d]", rc);
		LOG_INF("STORE Data: sector: %d, loc: %d", data.sector, data.loc);
	}

	rc = storage_area_store_unmount(&store);
	zassert_equal(rc, 0, "unmount returned [%d]", rc);
	LOG_INF("UNMOUNT STORE Data: sector: %d, loc: %d, wcnt: %d", data.sector, data.loc, data.wrapcnt);

	rc = storage_area_store_mount(&store);
	zassert_equal(rc, 0, "mount returned [%d]", rc);
	LOG_INF("MOUNT STORE Data: sector: %d, loc: %d, wcnt: %d", data.sector, data.loc, data.wrapcnt);
	
	rc = storage_area_store_compact(&store);
	zassert_equal(rc, 0, "compact returned [%d]", rc);
	LOG_INF("COMPACT STORE Data: sector: %d, loc: %d, wcnt: %d", data.sector, data.loc, data.wrapcnt);

	rc = storage_area_record_next(&store, &record);
	zassert_equal(rc, 0, "record next returned [%d]", rc);

	rc = storage_area_record_read(&record, 0U, wr, ARRAY_SIZE(wr));
	zassert_equal(rc, 0, "record read returned [%d]", rc);

	rc = storage_area_store_compact(&store);
}

ZTEST_SUITE(storage_area_api, NULL, storage_area_api_setup,
	    storage_area_api_before, NULL, NULL);