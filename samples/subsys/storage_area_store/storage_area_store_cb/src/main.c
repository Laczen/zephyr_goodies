/*
 * Copyright (c) 2024 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Test for the storage_area_store API */

#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/storage/storage_area/storage_area_store.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(sas_test);

#include <zephyr/storage/storage_area/storage_area_flash.h>
#define FLASH_AREA_NODE		DT_NODELABEL(storage_partition)
#define FLASH_AREA_OFFSET	DT_REG_ADDR(FLASH_AREA_NODE)
#define FLASH_AREA_DEVICE							\
	DEVICE_DT_GET(DT_MTD_FROM_FIXED_PARTITION(FLASH_AREA_NODE))
#define FLASH_AREA_XIP		FLASH_AREA_OFFSET + 				\
	DT_REG_ADDR(DT_MTD_FROM_FIXED_PARTITION(FLASH_AREA_NODE))
#define AREA_SIZE		DT_REG_SIZE(FLASH_AREA_NODE)
#define AREA_ERASE_SIZE		4096
#define AREA_WRITE_SIZE		8

const static struct storage_area_flash area = flash_storage_area(
	FLASH_AREA_DEVICE, FLASH_AREA_OFFSET, FLASH_AREA_XIP,
	AREA_WRITE_SIZE, AREA_ERASE_SIZE, AREA_SIZE,
	SA_PROP_LOVRWRITE);

const char cookie[]="!NVS";

#define SECTOR_SIZE	1024
/* This storage area store is using only 1 erase block */
create_storage_area_store(test, &area.area, (void *)cookie,
	sizeof(cookie), SECTOR_SIZE, AREA_ERASE_SIZE / SECTOR_SIZE, 0, NULL,
	NULL, NULL);

struct __attribute__((packed)) data_format {
	uint8_t state;
	uint32_t value;
};

static int storage_area_store_init(const struct storage_area_store *store)
{
	int rc = storage_area_store_mount(store, NULL);

	if (rc != 0) {
		return rc;
	}

	return storage_area_erase(store->area, 0, 1);
}

void storage_area_store_report_state(char *tag,
				     const struct storage_area_store *store)
{
	struct storage_area_store_data *data = store->data;

	LOG_INF("%s: sector: %d - loc:%d - wrapcnt:%d", tag, data->sector,
		data->loc, data->wrapcnt);
}

static int producer(const struct storage_area_store *store)
{
	size_t rcount = 0;
	int rc = 0;

	storage_area_store_report_state("Producer", store);

	for (int i = 0; i < 8; i++) {
		struct data_format data = {
			.state = 0xff,
			.value = (uint32_t)i,
		};

		while (true) {
			rc = storage_area_store_dwrite(store, &data, sizeof(data));
			if (rc == -ENOSPC) {
				LOG_INF("Added before advance [%d]", rcount);
				/* Not doing any copy for keeping records */
				rc = storage_area_store_advance(store);
				if (rc == 0) {
					continue;
				}
			}

			if (rc == 0) {
				rcount++;
			}
			
			break;
		}
	}

	LOG_INF("Producer added [%d] records", rcount);
	return rc;
}

static int consumer(const struct storage_area_store *store)
{
	struct storage_area_record walk;
	size_t rcount = 0U;
	int rc = 0;

	walk.store = NULL;
	while (storage_area_record_next(store, &walk) == 0) {
		struct data_format data;

		rc = storage_area_record_dread(&walk, 0U, &data, sizeof(data));
		if (rc != 0) {
			break;
		}

		if (data.state == 0xFF) {
			rcount++;
			/* invalidate read data */
			rc = storage_area_record_fbupdate(&walk, 0x0);
			if (rc != 0) {
				LOG_INF("FAIL");
				break;
			}
		}
	}
	
	LOG_INF("Consumer found [%d] valid records", rcount);
	if (rcount != 8) {
		LOG_INF("Some records were lost because data was erased");
	}
	return rc;
}

int main(void)
{
	struct storage_area_store *store = get_storage_area_store(test);
	int rc;

	LOG_INF("STARTING sample");
	rc = storage_area_store_init(store);
	if (rc != 0) {
		LOG_INF("Init failed");
		goto end;
	}

	for (int i = 0; i < 100; i++) {
		rc = producer(store);
		if (rc != 0) {
			LOG_INF("Producer failed [%d]", rc);
			break;
		}

		rc = consumer(store);
		if (rc != 0) {
			LOG_INF("Consumer failed [%d]", rc);
			break;
		}
	}
end:
	LOG_INF("Done");
	return 0;
}
