/*
 * Copyright (c) 2024 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/storage/storage_area_eeprom.h>
#include <zephyr/drivers/eeprom.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sa_eeprom, CONFIG_STORAGE_AREA_LOG_LEVEL);

static bool sa_eeprom_range_valid(const struct storage_area_eeprom *eeprom,
				  size_t start, size_t len)
{
	if ((eeprom->size < len) || ((eeprom->size - len) < start)) {
		return false;
	}

	return true;
}

static int sa_eeprom_ioctl(const struct storage_area *area,
			   enum storage_area_ioctl_cmd cmd, void *data)
{
	const struct storage_area_eeprom *eeprom =
		CONTAINER_OF(area, struct storage_area_eeprom, area);
	const struct device *dev = eeprom->dev;
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

		*size = eeprom->size;
		rc = 0;
		break;

	case SA_IOCTL_WRITESIZE:
		size_t *write_size = (size_t *)data;

		if (write_size == NULL) {
			rc = -EINVAL;
			break;
		}

		*write_size = 1U;
		rc = 0;
		break;

	case SA_IOCTL_ERASESIZE:
		size_t *erase_size = (size_t *)data;

		if (erase_size == NULL) {
			rc = -EINVAL;
			break;
		}

		*erase_size = 1U;
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
		
		size_t start = ctx->start;
		size_t len = ctx->len;

		if (!sa_eeprom_range_valid(eeprom, start, len)) {
			rc = -EINVAL;
			break;
		}

		const uint8_t erase_value= STORAGE_AREA_ERASEVALUE(area);
		start += eeprom->start;
		while (len > 0) {
			rc = eeprom_write(dev, start, &erase_value, 1);
			if (rc != 0) {
				break;
			}

			len--;
			start++;
		}

		break;

	default:
		break;
	}

end:
	return rc;
}

static int sa_eeprom_readblocks(const struct storage_area *area, size_t start,
			       const struct storage_area_db *db, size_t bcnt)
{
	const struct storage_area_eeprom *eeprom =
		CONTAINER_OF(area, struct storage_area_eeprom, area);
	const struct device *dev = eeprom->dev;
	const size_t tlen = storage_area_dbsize(db, bcnt);
	int rc;

	if (!device_is_ready(dev)) {
		rc = -ENODEV;
		goto end;
	}

	if (!sa_eeprom_range_valid(eeprom, start, tlen)) {
		rc = -EINVAL;
		goto end;
	}

	start += eeprom->start;
	for (size_t i = 0U; i < bcnt; i++) {
		size_t len = db[i].len;

		rc = eeprom_read(dev, start, db[i].data, len);
		if (rc != 0) {
			break;
		}

		start += len;
	}

end:
	return rc;
}

static int sa_eeprom_progblocks(const struct storage_area *area, size_t start,
			        const struct storage_area_db *db, size_t bcnt)
{
	const struct storage_area_eeprom *eeprom =
		CONTAINER_OF(area, struct storage_area_eeprom, area);
	const struct device *dev = eeprom->dev;
	const size_t tlen = storage_area_dbsize(db, bcnt);
	int rc;

	if (!device_is_ready(dev)) {
		rc = -ENODEV;
		goto end;
	}

	if (!sa_eeprom_range_valid(eeprom, start, tlen)) {
		rc = -EINVAL;
		goto end;
	}
	
	start += eeprom->start;
	for (size_t i = 0U; i < bcnt; i++) {
		uint8_t *data8 = (uint8_t *)db[i].data;
		size_t len = db[i].len;

		rc = eeprom_write(dev, start, data8, len);
		if (rc != 0) {
			break;
		}

		start += len;
	}

end:
	return rc;
}

const struct storage_area_api storage_area_eeprom_api = {
	.readblocks = sa_eeprom_readblocks,
	.progblocks = sa_eeprom_progblocks,
	.ioctl = sa_eeprom_ioctl,
};