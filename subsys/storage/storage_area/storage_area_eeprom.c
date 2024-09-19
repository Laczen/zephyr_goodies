/*
 * Copyright (c) 2024 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/storage/storage_area/storage_area_eeprom.h>
#include <zephyr/drivers/eeprom.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sa_eeprom, CONFIG_STORAGE_AREA_LOG_LEVEL);

static int sa_eeprom_read(const struct storage_area *area, size_t start,
			  const struct storage_area_chunk *ch, size_t cnt)
{
	const struct storage_area_eeprom *eeprom =
		CONTAINER_OF(area, struct storage_area_eeprom, area);

	if (!device_is_ready(eeprom->dev)) {
		return -ENODEV;
	}

	int rc = 0;

	start += eeprom->start;
	for (size_t i = 0U; i < cnt; i++) {
		rc = eeprom_read(eeprom->dev, start, ch[i].data, ch[i].len);
		if (rc != 0) {
			break;
		}

		start += ch[i].len;
	}

	return rc;
}

static int sa_eeprom_prog(const struct storage_area *area, size_t start,
			  const struct storage_area_chunk *ch, size_t cnt)
{
	const struct storage_area_eeprom *eeprom =
		CONTAINER_OF(area, struct storage_area_eeprom, area);
	
	if (!device_is_ready(eeprom->dev)) {
		return -ENODEV;
	}

	const size_t align = area->write_size;
	uint8_t buf[align];
	size_t bpos = 0U;
	int rc = 0;

	start += eeprom->start;
	for (size_t i = 0U; i < cnt; i++) {
		uint8_t *data8 = (uint8_t *)ch[i].data;
		size_t blen = ch[i].len;

		if (bpos != 0U) {
			size_t cplen = MIN(blen, align - bpos);

			memcpy(buf + bpos, data8, cplen);
			bpos += cplen;
			blen -= cplen;
			data8 += cplen;

			if (bpos == align) {
				rc = eeprom_write(eeprom->dev, start, buf, align);
				if (rc != 0) {
					break;
				}

				start += align;
				bpos = 0U;
			}
		}

		if (blen >= align) {
			size_t wrlen = blen & ~(align - 1);

			rc = eeprom_write(eeprom->dev, start, data8, wrlen);
			if (rc != 0) {
				break;
			}

			blen -= wrlen;
			data8 += wrlen;
		}

		if (blen > 0U) {
			memcpy(buf, data8, blen);
			bpos = blen;
		}
	}

	return rc;
}

static int sa_eeprom_ioctl(const struct storage_area *area,
			   enum storage_area_ioctl_cmd cmd, void *data)
{
	const struct storage_area_eeprom *eeprom =
		CONTAINER_OF(area, struct storage_area_eeprom, area);

	if (!device_is_ready(eeprom->dev)) {
		return -ENODEV;
	}

	int rc = -ENOTSUP;
	
	switch(cmd) {
	default:
		break;
	}

	return rc;
}

const struct storage_area_api storage_area_eeprom_api = {
	.read = sa_eeprom_read,
	.prog = sa_eeprom_prog,
	.ioctl = sa_eeprom_ioctl,
};