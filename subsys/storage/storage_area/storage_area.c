/*
 * Copyright (c) 2024 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/sys/util.h>
#include <zephyr/storage/storage_area.h>

int storage_area_ioctl(const struct storage_area *area,
		       enum storage_area_ioctl_cmd cmd, void *data)
{
	int rc;

	if ((area == NULL) || (area->api == NULL)) {
		rc = -EINVAL;
		goto end;
	}
	
	if (area->api->ioctl == NULL) {
		rc = -ENOTSUP;
		goto end;
	}

	rc = area->api->ioctl(area, cmd, data);
end:
	return rc;
}

int storage_area_readblocks(const struct storage_area *area, size_t start,
			    const struct storage_area_db *db, size_t bcnt)
{
	int rc;

	if ((area == NULL) || (area->api == NULL)) {
		rc = -EINVAL;
		goto end;
	}
	
	if (area->api->readblocks == NULL) {
		rc = -ENOTSUP;
		goto end;
	}

	rc = area->api->readblocks(area, start, db, bcnt);
end:
	return rc;

}

int storage_area_read(const struct storage_area *area, size_t start,
		      void *data, size_t len)
{
	const struct storage_area_db db = {
		.data = data,
		.len = len,
	};

	return storage_area_readblocks(area, start, &db, 1);
}

int storage_area_progblocks(const struct storage_area *area, size_t start,
			    const struct storage_area_db *db, size_t bcnt)
{
	int rc;

	if ((area == NULL) || (area->api == NULL)) {
		rc = -EINVAL;
		goto end;
	}

	if (STORAGE_AREA_HAS_PROPERTY(area, SA_PROP_READONLY)) {
		rc = -EROFS;
		goto end;
	}

	if (area->api->progblocks == NULL) {
		rc = -ENOTSUP;
		goto end;
	}

	rc = area->api->progblocks(area, start, db, bcnt);
end:
	return rc;
}

int storage_area_prog(const struct storage_area *area, size_t start,
		      const void *data, size_t len)
{
	const struct storage_area_db db = {
		.data = (void *)data,
		.len = len,
	};

	return storage_area_progblocks(area, start, &db, 1);
}

size_t storage_area_dbsize(const struct storage_area_db *db, size_t bcnt)
{
	size_t rv = 0U;

	for (size_t i = 0U; i < bcnt; i++) {
		rv += db[i].len;
	}

	return rv;
}