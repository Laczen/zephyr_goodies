/*
 * Copyright (c) 2024 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/storage/storage_area/storage_area_eeprom.h>
#include <zephyr/drivers/eeprom.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sa_eeprom, CONFIG_STORAGE_AREA_LOG_LEVEL);

static int sa_eeprom_erase(const struct storage_area *area, size_t start, size_t len)
{
	const struct storage_area_eeprom *eeprom =
		CONTAINER_OF(area, struct storage_area_eeprom, area);
	const size_t asize = area->write_size * area->write_blocks;
	const uint8_t erase_value = STORAGE_AREA_ERASEVALUE(area);
	uint8_t buf[area->erase_size];
	int rc = -EINVAL;

	start *= area->erase_size;
	len *= area->erase_size;
	if ((asize > len) || ((len - asize) > start)) {
		goto end;
	}

	memset(buf, erase_value, sizeof(buf));
	start += eeprom->start;
	while (len > 0U) {
		rc = eeprom_write(eeprom->dev, start, buf, sizeof(buf));
		if (rc != 0) {
			break;
		}

		len -= sizeof(buf);
		start += sizeof(buf);
	}

end:
	return rc;
}

static int sa_eeprom_ioctl(const struct storage_area *area,
			   enum storage_area_ioctl_cmd cmd, void *data)
{
	const struct storage_area_eeprom *eeprom =
		CONTAINER_OF(area, struct storage_area_eeprom, area);
	int rc = -ENOTSUP;

	if (!device_is_ready(eeprom->dev)) {
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

		rc = sa_eeprom_erase(area, ctx->start, ctx->count);
		break;

	default:
		break;
	}

end:
	return rc;
}

static int sa_eeprom_read(const struct storage_area *area, size_t start,
			  void *data, size_t len)
{
	const struct storage_area_eeprom *eeprom =
		CONTAINER_OF(area, struct storage_area_eeprom, area);
	int rc = -ENODEV;

	if (!device_is_ready(eeprom->dev)) {
		goto end;
	}

	rc = eeprom_read(eeprom->dev, eeprom->start + start, data, len);
end:
	return rc;
}

static int sa_eeprom_prog(const struct storage_area *area, size_t start,
			  const void *data, size_t len)
{
	const struct storage_area_eeprom *eeprom =
		CONTAINER_OF(area, struct storage_area_eeprom, area);
	int rc = -ENODEV;

	if (!device_is_ready(eeprom->dev)) {
		goto end;
	}

	rc = eeprom_write(eeprom->dev, eeprom->start + start, data, len);
end:
	return rc;
}

const struct storage_area_api storage_area_eeprom_api = {
	.read = sa_eeprom_read,
	.prog = sa_eeprom_prog,
	.ioctl = sa_eeprom_ioctl,
};