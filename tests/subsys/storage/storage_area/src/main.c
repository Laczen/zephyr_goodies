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
#include <zephyr/storage/flash_map.h>
#include <zephyr/storage/storage_area_flash.h>

#define TEST_AREA	storage_partition
#define TEST_AREA_OFFSET	FIXED_PARTITION_OFFSET(TEST_AREA)
#define TEST_AREA_SIZE		FIXED_PARTITION_SIZE(TEST_AREA)
#define TEST_AREA_MAX		(TEST_AREA_OFFSET + TEST_AREA_SIZE)
#define TEST_AREA_DEVICE	FIXED_PARTITION_DEVICE(TEST_AREA)
#define TEST_AREA_XIP		TEST_AREA_OFFSET + 				\
	DT_REG_ADDR(DT_MTD_FROM_FIXED_PARTITION(DT_NODELABEL(TEST_AREA)))

const static struct storage_area_flash flash_area = flash_storage_area(
	TEST_AREA_DEVICE, TEST_AREA_OFFSET, TEST_AREA_SIZE, TEST_AREA_XIP, 4,
	4096, 0);
const static struct storage_area *area = &flash_area.area;

static void *storage_area_api_setup(void)
{
	return NULL;
}

ZTEST_USER(storage_area_api, test_read_write)
{
	int rc;
	size_t size = STORAGE_AREA_WRITESIZE(area);

	zassert_not_equal(size, 0U, "Size value is invalid");

	uint8_t wr[size];
	uint8_t rd[size];

	memset(wr, 'T', sizeof(wr));
	memset(rd, 0, sizeof(rd));

	rc = storage_area_prog(area, 0U, wr, sizeof(wr));
	zassert_equal(rc, 0, "prog returned [%d]", rc);

	rc = storage_area_read(area, 0U, rd, sizeof(rd));
	zassert_equal(rc, 0, "read returned [%d]", rc);

	zassert_equal(memcmp(rd, wr, sizeof(wr)), 0, "data mismatch");
}

ZTEST_SUITE(storage_area_api, NULL, storage_area_api_setup, NULL, NULL, NULL);