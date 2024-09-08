/*
 * Copyright (c) 2024 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/storage/storage_area_flash.h>
#include <zephyr/drivers/flash.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sa_flash, CONFIG_STORAGE_AREA_LOG_LEVEL);

static bool sa_flash_range_valid(const struct storage_area_flash *flash,
				 size_t start, size_t len)
{
	if ((flash->size < len) || ((flash->size - len) < start)) {
		return false;
	}

	return true;
}

static int sa_flash_ioctl(const struct storage_area *area,
			  enum storage_area_ioctl_cmd cmd, void *data)
{
	const struct storage_area_flash *flash =
		CONTAINER_OF(area, struct storage_area_flash, area);
	const struct device *dev = flash->dev;
	int rc;

	if (!device_is_ready(dev)) {
		rc = -ENODEV;
		goto end;
	}

	rc = -ENOTSUP;
	switch(cmd) {
	case SA_IOCTL_SIZE:
		size_t *size = (size_t *)data;

		if (size == NULL) {
			rc = -EINVAL;
			break;
		}

		*size = flash->size;
		rc = 0;
		break;

	case SA_IOCTL_WRITESIZE:
		size_t *write_size = (size_t *)data;

		if (write_size == NULL) {
			rc = -EINVAL;
			break;
		}

		*write_size = flash->write_size;
		rc = 0;
		break;

	case SA_IOCTL_ERASESIZE:
		size_t *erase_size = (size_t *)data;

		if (erase_size == NULL) {
			rc = -EINVAL;
			break;
		}

		*erase_size = flash->erase_size;
		rc = 0;
		break;

	case SA_IOCTL_WRITEPREPARE:
		if (STORAGE_AREA_HAS_PROPERTY(area, SA_PROP_OVRWRITE)) {
			rc = 0;
			break;
		}

	case SA_IOCTL_ERASE:
		const struct storage_area_erase_ctx *ctx = 
			(const struct storage_area_erase_ctx *)data;
		
		if (ctx == NULL) {
			rc = -EINVAL;
			break;
		}

		if (STORAGE_AREA_HAS_PROPERTY(area, SA_PROP_READONLY)) {
			rc = -EROFS;
			break;
		}
		
		const size_t start = ctx->start * flash->erase_size;
		const size_t len = ctx->len * flash->erase_size;

		if (!sa_flash_range_valid(flash, start, len)) {
			rc = -EINVAL;
			break;
		}

		rc = flash_erase(dev, flash->start + start, len);
		break;

	case SA_IOCTL_XIPADDRESS:
		uintptr_t *xip_address = (uintptr_t *)data;

		if (xip_address == NULL) {
			rc = -EINVAL;
			break;
		}

		*xip_address = (uintptr_t)flash->xip_address;
		rc = 0; 
		break;

	default:
		break;
	}

end:
	return rc;
}

static int sa_flash_readblocks(const struct storage_area *area, size_t start,
			       const struct storage_area_db *db, size_t bcnt)
{
	const struct storage_area_flash *flash =
		CONTAINER_OF(area, struct storage_area_flash, area);
	const struct device *dev = flash->dev;
	const size_t tlen = storage_area_dbsize(db, bcnt);
	int rc;

	if (!device_is_ready(dev)) {
		rc = -ENODEV;
		goto end;
	}

	if (!sa_flash_range_valid(flash, start, tlen)) {
		rc = -EINVAL;
		goto end;
	}

	start += flash->start;
	for (size_t i = 0U; i < bcnt; i++) {
		size_t len = db[i].len;

		rc = flash_read(dev, start, db[i].data, len);
		if (rc != 0) {
			break;
		}

		start += len;
	}

end:
	return rc;
}

static int sa_flash_progblocks(const struct storage_area *area, size_t start,
			       const struct storage_area_db *db, size_t bcnt)
{
	const struct storage_area_flash *flash =
		CONTAINER_OF(area, struct storage_area_flash, area);
	const struct device *dev = flash->dev;
	const size_t align = flash->write_size;
	const size_t tlen = storage_area_dbsize(db, bcnt);
	uint8_t buf[align];
	size_t bpos = 0U;
	int rc;

	if (!device_is_ready(dev)) {
		rc = -ENODEV;
		goto end;
	}

	if (((tlen % align) != 0U) || 
	    (!sa_flash_range_valid(flash, start, tlen))) {
		rc = -EINVAL;
		goto end;
	}
	
	start += flash->start;
	for (size_t i = 0U; i < bcnt; i++) {
		uint8_t *data8 = (uint8_t *)db[i].data;
		size_t len = db[i].len;

		if (bpos != 0U) {
			size_t cplen = MIN(len, align - bpos);

			memcpy(buf + bpos, data8, cplen);
			bpos += cplen;
			len -= cplen;
			data8 += cplen;

			if (bpos == align) {
				rc = flash_write(dev, start, buf, align);
				if (rc != 0) {
					break;
				}

				start += align;
				bpos = 0U;
			}
		}

		if (len >= align) {
			size_t wrlen = len & ~(align - 1);

			rc = flash_write(dev, start, data8, wrlen);
			if (rc != 0) {
				break;
			}

			len -= wrlen;
			data8 += wrlen;
			start += wrlen;
		}

		if (len > 0U) {
			memcpy(buf, data8, len);
			bpos = len;
		}
	}

end:
	return rc;
}

const struct storage_area_api storage_area_flash_api = {
	.readblocks = sa_flash_readblocks,
	.progblocks = sa_flash_progblocks,
	.ioctl = sa_flash_ioctl,
};