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

int storage_area_read(const struct storage_area *area, size_t start,
		      void *data, size_t len)
{
	int rc;

	if ((area == NULL) || (area->api == NULL)) {
		rc = -EINVAL;
		goto end;
	}
	
	if (area->api->read == NULL) {
		rc = -ENOTSUP;
		goto end;
	}

	rc = area->api->read(area, start, data, len);
end:
	return rc;
}

int storage_area_prog(const struct storage_area *area, size_t start,
		      const void *data, size_t len)
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

	if (area->api->prog == NULL) {
		rc = -ENOTSUP;
		goto end;
	}

	if (((start % area->write_size) != 0U) ||
	    ((len % area->write_size) != 0U)) {
		rc = -EINVAL;
		goto end;
	}

	rc = area->api->prog(area, start, data, len);
end:
	return rc;
}

int storage_area_verify(const struct storage_area *area)
{
	int rc = 0;

	if ((area == NULL) || (area->api == NULL)) {
		rc = -EINVAL;
		goto end;
	}

	if (IS_ENABLED(CONFIG_STORAGE_AREA_VERIFY)) {
		if (area->api->verify == NULL) {
			rc = -ENOTSUP;
			goto end;
		}

		rc = area->api->verify(area);
	}
end:
	return rc;
}