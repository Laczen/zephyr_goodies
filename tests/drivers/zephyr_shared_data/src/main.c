/*
 * Copyright (c) 2024 Laczen
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/zephyr_shared_data.h>

const static struct device *shareddatatestdevice =
	DEVICE_DT_GET(DT_ALIAS(shareddatatestdevice));

static void *shared_data_api_setup(void)
{
	if (IS_ENABLED(CONFIG_USERSPACE)) {
		k_object_access_grant(shareddatatestdevice, k_current_get());
	}

	return NULL;
}

ZTEST_USER(shared_data_api, test_get_size)
{
	int rc;
	size_t size = 0U;

	rc = zephyr_shared_data_size(shareddatatestdevice, &size);

	zassert_not_equal(rc, 0, "Get size returned invalid value");
	zassert_not_equal(size, 0U, "Size value is invalid");
}

ZTEST_USER(shared_data_api, test_get_set)
{
	int rc;
	size_t size = 0U;

	rc = zephyr_shared_data_size(shareddatatestdevice, &size);

	zassert_not_equal(rc, 0, "Get size returned invalid value");
	zassert_not_equal(size, 0U, "Size value is invalid");

	uint8_t wr[size];
	uint8_t rd[size];

	memset(wr, 'T', sizeof(wr));
	memset(rd, 0, sizeof(rd));

	rc = zephyr_shared_data_prog(shareddatatestdevice, 0U, wr, sizeof(wr));
	zassert_equal(rc, 0, "prog returned [%d]", rc);

	rc = zephyr_shared_data_read(shareddatatestdevice, 0U, rd, sizeof(rd));
	zassert_equal(rc, 0, "read returned [%d]", rc);

	zassert_equal(memcmp(rd, wr, sizeof(wr)), 0, "data mismatch");
}

ZTEST_SUITE(shared_data_api, NULL, shared_data_api_setup, NULL, NULL, NULL);