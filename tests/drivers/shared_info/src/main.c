/*
 * Copyright (c) 2024 Laczen
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/shared_info.h>

const static struct device *sharedinfotestdevice =
	DEVICE_DT_GET(DT_ALIAS(sharedinfotestdevice));

static void *shared_info_api_setup(void)
{
	if (IS_ENABLED(CONFIG_USERSPACE)) {
		k_object_access_grant(sharedinfotestdevice, k_current_get());
	}

	return NULL;
}

ZTEST_USER(shared_info_api, test_get_size)
{
	int rc;
	size_t size = 0U;

	rc = shared_info_size(sharedinfotestdevice, &size);

	zassert_equal(rc, 0, "Get size returned invalid value [%d]", rc);
	zassert_not_equal(size, 0U, "Size value is invalid");
}

ZTEST_USER(shared_info_api, test_get_set)
{
	int rc;
	size_t size = 0U;

	rc = shared_info_size(sharedinfotestdevice, &size);

	zassert_equal(rc, 0, "Get size returned invalid value [%d]", rc);
	zassert_not_equal(size, 0U, "Size value is invalid");

	uint8_t wr[size];
	uint8_t rd[size];

	memset(wr, 'T', sizeof(wr));
	memset(rd, 0, sizeof(rd));

	rc = shared_info_prog(sharedinfotestdevice, 0U, wr, sizeof(wr));
	zassert_equal(rc, 0, "prog returned [%d]", rc);

	rc = shared_info_read(sharedinfotestdevice, 0U, rd, sizeof(rd));
	zassert_equal(rc, 0, "read returned [%d]", rc);

	zassert_equal(memcmp(rd, wr, sizeof(wr)), 0, "data mismatch");
}

ZTEST_SUITE(shared_info_api, NULL, shared_info_api_setup, NULL, NULL, NULL);