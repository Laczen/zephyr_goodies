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

#define FLASH_AREA_NODE		DT_NODELABEL(storage_partition)
#define FLASH_AREA_OFFSET	DT_REG_ADDR(FLASH_AREA_NODE)
#define FLASH_AREA_SIZE		DT_REG_SIZE(FLASH_AREA_NODE)
#define FLASH_AREA_DEVICE							\
	DEVICE_DT_GET(DT_MTD_FROM_FIXED_PARTITION(FLASH_AREA_NODE))
#define FLASH_AREA_XIP		FLASH_AREA_OFFSET + 				\
	DT_REG_ADDR(DT_MTD_FROM_FIXED_PARTITION(FLASH_AREA_NODE))

const static struct storage_area_flash flash_area = flash_storage_area(
	FLASH_AREA_DEVICE, FLASH_AREA_OFFSET, FLASH_AREA_SIZE, FLASH_AREA_XIP, 4,
	4096, 0);
const static struct storage_area *area = &flash_area.area;

static void *storage_area_api_setup(void)
{
	return NULL;
}

ZTEST_USER(storage_area_api, test_read_write)
{
	int rc;
	size_t size = 4;

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