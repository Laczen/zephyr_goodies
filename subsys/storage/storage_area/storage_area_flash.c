/*
 * Copyright (c) 2024 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/storage/storage_area/storage_area_flash.h>
#include <zephyr/drivers/flash.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sa_flash, CONFIG_STORAGE_AREA_LOG_LEVEL);

static int sa_flash_read(const struct storage_area *area, size_t start,
			 const struct storage_area_chunk *ch, size_t cnt)
{
	const struct storage_area_flash *flash =
		CONTAINER_OF(area, struct storage_area_flash, area);
	
	if (!device_is_ready(flash->dev)) {
		return -ENODEV;
	}

	int rc = 0;

	start += flash->start;
	for (size_t i = 0U; i < cnt; i++) {
		rc = flash_read(flash->dev, start, ch[i].data, ch[i].len);
		if (rc != 0) {
			break;
		}

		start += ch[i].len;
	}

	return rc;
}

static int sa_flash_prog(const struct storage_area *area, size_t start,
			 const struct storage_area_chunk *ch, size_t cnt)
{
	const struct storage_area_flash *flash =
		CONTAINER_OF(area, struct storage_area_flash, area);

	if (!device_is_ready(flash->dev)) {
		return -ENODEV;
	}

	const size_t align = area->write_size;
	uint8_t buf[align];
	size_t bpos = 0U;
	int rc = 0;

	start += flash->start;
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
				rc = flash_write(flash->dev, start, buf, align);
				if (rc != 0) {
					break;
				}

				start += align;
				bpos = 0U;
			}
		}

		if (blen >= align) {
			size_t wrlen = blen & ~(align - 1);

			rc = flash_write(flash->dev, start, data8, wrlen);
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

static int sa_flash_erase(
	const struct storage_area *area,
	size_t start,
	size_t len)
{
	const struct storage_area_flash *flash =
		CONTAINER_OF(area, struct storage_area_flash, area);
	
	if (!device_is_ready(flash->dev)) {
		return -ENODEV;
	}

	start *= area->erase_size;
	len *= area->erase_size;

	start += flash->start;
	return flash_erase(flash->dev, start, len);
}

static int sa_flash_ioctl(const struct storage_area *area,
			  enum storage_area_ioctl_cmd cmd, void *data)
{
	const struct storage_area_flash *flash =
		CONTAINER_OF(area, struct storage_area_flash, area);
	
	if (!device_is_ready(flash->dev)) {
		return -ENODEV;
	}

	int rc = -ENOTSUP;
	
	switch(cmd) {
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

	return rc;
}

const struct storage_area_api storage_area_flash_api = {
	.read = sa_flash_read,
	.prog = sa_flash_prog,
	.erase = sa_flash_erase,
	.ioctl = sa_flash_ioctl,
};