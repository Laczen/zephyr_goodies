/*
 * Copyright (c) 2024 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/storage/storage_area/storage_area_flash.h>
#include <zephyr/drivers/flash.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sa_flash, CONFIG_STORAGE_AREA_LOG_LEVEL);

static int sa_flash_erase(const struct storage_area *area, size_t start, size_t len)
{
	const struct storage_area_flash *flash =
		CONTAINER_OF(area, struct storage_area_flash, area);
	const size_t asize = area->write_size * area->write_blocks;
	int rc = -EINVAL;

	start *= area->erase_size;
	len *= area->erase_size;
	if ((asize > len) || ((len - asize) > start)) {
		goto end;
	}

	start += flash->start;
	rc = flash_erase(flash->dev, start, len);
end:
	return rc;
}

static int sa_flash_ioctl(const struct storage_area *area,
			  enum storage_area_ioctl_cmd cmd, void *data)
{
	const struct storage_area_flash *flash =
		CONTAINER_OF(area, struct storage_area_flash, area);
	int rc = -ENOTSUP;

	if (!device_is_ready(flash->dev)) {
		rc = -ENODEV;
		goto end;
	}

	switch(cmd) {
	case SA_IOCTL_ERASE:
		if (STORAGE_AREA_HAS_PROPERTY(area, SA_PROP_READONLY)) {
			rc = -EROFS;
			break;
		}

		const struct storage_area_erase_ctx *ctx = 
			(const struct storage_area_erase_ctx *)data;

		if (ctx == NULL) {
			rc = -EINVAL;
			break;
		}
		
		rc = sa_flash_erase(area, ctx->start, ctx->count);	
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

static int sa_flash_read(const struct storage_area *area, size_t start,
			 void *data, size_t len)
{
	const struct storage_area_flash *flash =
		CONTAINER_OF(area, struct storage_area_flash, area);
	int rc = -ENODEV;

	if (!device_is_ready(flash->dev)) {
		goto end;
	}

	rc = flash_read(flash->dev, flash->start + start, data, len);
end:
	return rc;
}

static int sa_flash_prog(const struct storage_area *area, size_t start,
			 const void *data, size_t len)
{
	const struct storage_area_flash *flash =
		CONTAINER_OF(area, struct storage_area_flash, area);
	int rc = -ENODEV;

	if (!device_is_ready(flash->dev)) {
		goto end;
	}

	rc = flash_write(flash->dev, flash->start + start, data, len);

end:
	return rc;
}

const struct storage_area_api storage_area_flash_api = {
	.read = sa_flash_read,
	.prog = sa_flash_prog,
	.ioctl = sa_flash_ioctl,
};