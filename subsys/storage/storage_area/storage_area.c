/*
 * Copyright (c) 2024 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/sys/util.h>
#include <string.h>
#include <zephyr/storage/storage_area/storage_area.h>

static bool storage_area_range_valid(const struct storage_area *area,
				     size_t start, size_t len)
{
	const size_t sa_size = area->write_size * area->write_blocks;

	if ((sa_size < len) || ((sa_size - len) < start)) {
		return false;
	}

	return true;
}

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
	int rc = -EINVAL;

	if ((area == NULL) || (area->api == NULL) ||
	    (!storage_area_range_valid(area, start, len))) {
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
	int rc = -EINVAL;

	if ((area == NULL) || (area->api == NULL) ||
	    (!storage_area_range_valid(area, start, len)) ||
	    ((len % area->write_size) != 0U)) {
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

	rc = area->api->prog(area, start, data, len);
end:
	return rc;
}

static size_t storage_area_dbsize(const struct storage_area_db *db, size_t bcnt)
{
	size_t rv = 0U;

	for (size_t i = 0U; i < bcnt; i++) {
		rv += db[i].len;
	}

	return rv;
}

int storage_area_readblocks(const struct storage_area *area, size_t start,
			    const struct storage_area_db *db, size_t bcnt)
{
	const size_t len = storage_area_dbsize(db, bcnt);
	int rc = -EINVAL;

	if ((area == NULL) || (area->api == NULL) ||
	    (!storage_area_range_valid(area, start, len))) {
		goto end;
	}
	
	if (area->api->read == NULL) {
		rc = -ENOTSUP;
		goto end;
	}

	for (size_t i = 0U; i < bcnt; i++) {
		rc = area->api->read(area, start, db[i].data, db[i].len);
		if (rc != 0) {
			break;
		}

		start += db[i].len;
	}

end:
	return rc;

}

int storage_area_progblocks(const struct storage_area *area, size_t start,
			    const struct storage_area_db *db, size_t bcnt)
{
	const size_t len = storage_area_dbsize(db, bcnt);
	const size_t align = area->write_size;
	uint8_t buf[align];
	size_t bpos = 0U;
	int rc = -EINVAL;

	if ((area == NULL) || (area->api == NULL) ||
	    (!storage_area_range_valid(area, start, len)) ||
	    ((len % area->write_size) != 0U)) {
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

	for (size_t i = 0U; i < bcnt; i++) {
		uint8_t *data8 = (uint8_t *)db[i].data;
		size_t blen = db[i].len;

		if (bpos != 0U) {
			size_t cplen = MIN(blen, align - bpos);

			memcpy(buf + bpos, data8, cplen);
			bpos += cplen;
			blen -= cplen;
			data8 += cplen;

			if (bpos == align) {
				rc = area->api->prog(area, start, buf, align);
				if (rc != 0) {
					break;
				}

				start += align;
				bpos = 0U;
			}
		}

		if (blen >= align) {
			size_t wrlen = blen & ~(align - 1);

			rc = area->api->prog(area, start, data8, wrlen);
			if (rc != 0) {
				break;
			}

			blen -= wrlen;
			data8 += wrlen;
			start += wrlen;
		}

		if (blen > 0U) {
			memcpy(buf, data8, blen);
			bpos = blen;
		}
	}

end:
	return rc;
}
