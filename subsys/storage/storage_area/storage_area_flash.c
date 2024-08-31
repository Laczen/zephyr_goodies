/*
 * Copyright (c) 2024 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/storage/storage_area_flash.h>
#include <zephyr/drivers/flash.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sa_flash, CONFIG_STORAGE_AREA_LOG_LEVEL);

#if defined(CONFIG_STORAGE_AREA_VERIFY)
static int storage_area_flash_verify(const struct storage_area *area)
{
	const struct storage_area_flash *flash =
		CONTAINER_OF(area, struct storage_area_flash, area);
	const struct device *dev = flash->dev;
	int rc = -ENODEV;

	if (!device_is_ready(dev)) {
		goto end;
	}

	const struct flash_parameters *parameters = flash_get_parameters(dev);
	struct flash_pages_info info;
	off_t doff = flash->start;
	
	rc = -EINVAL;
	if (parameters == NULL) {
		LOG_ERR("Unable to get flash parameters");
		goto end;
	}

	if ((area->write_size % parameters->write_block_size) != 0U) {
		LOG_ERR("Write size definition error");
		goto end;
	}

	if ((parameters->erase_value == 0xff) &&
	    (STORAGE_AREA_HAS_PROPERTY(area, SA_PROP_ZEROERASE))) {
		LOG_ERR("Erase value definition error");
		goto end;
	}

	while (doff < (flash->start + flash->size)) {
		rc = flash_get_page_info_by_offs(dev, doff, &info);
		if (rc !=0) {
			LOG_ERR("Unable to get page info");
			goto end;
		}

		if ((area->erase_size % info.size) != 0) {
			LOG_ERR("Erase size definition error");
			goto end;
		}

		doff += area->erase_size;
	}

	rc = 0;
end:
	return rc;
}
#endif /* CONFIG_STORAGE_AREA_VERIFY */

static bool storage_area_flash_range_valid(
	const struct storage_area_flash *flash, size_t start, size_t len)
{
	if ((flash->size < len) || ((flash->size - len) < start)) {
		return false;
	}

	return true;
}

static int storage_area_flash_ioctl(const struct storage_area *area,
				    enum storage_area_ioctl_cmd cmd,
				    void *data)
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
		
		const size_t start = ctx->start * area->erase_size;
		const size_t len = ctx->len * area->erase_size;

		if (!storage_area_flash_range_valid(flash, start, len)) {
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

static int storage_area_flash_read(const struct storage_area *area,
				   size_t start, void *data, size_t len)
{
	const struct storage_area_flash *flash =
		CONTAINER_OF(area, struct storage_area_flash, area);
	const struct device *dev = flash->dev;
	int rc;

	if (!device_is_ready(dev)) {
		rc = -ENODEV;
		goto end;
	}

	if (!storage_area_flash_range_valid(flash, start, len)) {
		rc = -EINVAL;
		goto end;
	}

	rc = flash_read(dev, flash->start + start, data, len);
end:
	return rc;
}

static int storage_area_flash_prog(const struct storage_area *area,
				   size_t start, const void *data, size_t len)
{
	const struct storage_area_flash *flash =
		CONTAINER_OF(area, struct storage_area_flash, area);
	const struct device *dev = flash->dev;
	int rc;

	if (!device_is_ready(dev)) {
		rc = -ENODEV;
		goto end;
	}

	if (STORAGE_AREA_HAS_PROPERTY(area, SA_PROP_READONLY)) {
		rc = -EROFS;
		goto end;
	}

	if (!storage_area_flash_range_valid(flash, start, len)) {
		rc = -EINVAL;
		goto end;
	}
	
	rc = flash_write(dev, flash->start + start, data, len);
end:
	return rc;
}

const struct storage_area_api storage_area_flash_api = {
	.read = storage_area_flash_read,
	.prog = storage_area_flash_prog,
	.ioctl = storage_area_flash_ioctl,
#if defined (CONFIG_STORAGE_AREA_VERIFY)
	.verify = storage_area_flash_verify,
#endif
};