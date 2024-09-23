/*
 * Copyright (c) 2024 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Test for the storage_area API */

#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>
#include <zephyr/devicetree.h>
#include <zephyr/storage/storage_area/storage_area_flash.h>
#include <zephyr/storage/storage_area/storage_area_eeprom.h>
#include <zephyr/storage/storage_area/storage_area_ram.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(sa_api_test);

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

ZTEST_USER(storage_area_api, test_read_write_direct)
{
	int rc;
	uint8_t wr[STORAGE_AREA_WRITESIZE(area)];
	uint8_t rd[STORAGE_AREA_WRITESIZE(area)];

	memset(wr, 'T', sizeof(wr));
	memset(rd, 0, sizeof(rd));

	rc = storage_area_dprog(area, 0U, wr, sizeof(wr));
	zassert_equal(rc, 0, "prog returned [%d]", rc);

	rc = storage_area_dread(area, 0U, rd, sizeof(rd));
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

ZTEST_SUITE(storage_area_api, NULL, storage_area_api_setup,
	    storage_area_api_before, NULL, NULL);