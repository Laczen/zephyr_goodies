/*
 * Copyright (c) 2024 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stdint.h>
#include <zephyr/storage/storage_area/storage_area_flash.h>
#include <zephyr/drivers/flash.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(storage_area_flash, CONFIG_STORAGE_AREA_LOG_LEVEL);

static int sa_flash_valid(const struct storage_area_flash *flash)
{
	if (!device_is_ready(flash->dev)) {
		LOG_DBG("Device is not ready");
		return -ENODEV;
	}

	if (IS_ENABLED(CONFIG_STORAGE_AREA_VERIFY)) {
		const struct storage_area *area = &flash->area;
		const struct flash_parameters *param =
			flash_get_parameters(flash->dev);
		const size_t wbs = flash_get_write_block_size(flash->dev);
		struct flash_pages_info info;

		if (param == NULL) {
			LOG_DBG("Could not obtain flash parameters");
			return -EINVAL;
		}

		if ((area->write_size & (wbs - 1)) != 0) {
			LOG_DBG("Bad write block size");
			return -EINVAL;
		}

		for (size_t i = 0; i < area->erase_blocks; i++) {
			size_t off = flash->start + i * area->erase_size;
			int rc;

			rc = flash_get_page_info_by_offs(flash->dev, off, &info);
			if (rc != 0) {
				LOG_DBG("Could not obtain page info");
				return -EINVAL;
			}

			if ((info.start_offset != off) ||
			    ((area->erase_size & (info.size - 1)) != 0)) {
				LOG_DBG("Bad erase size");
				return -EINVAL;
			}
		}
	}

	return 0;
}

static int sa_flash_readv(const struct storage_area *area, size_t start,
			  const struct storage_area_iovec *iovec, size_t iovcnt)
{
	const struct storage_area_flash *flash =
		CONTAINER_OF(area, struct storage_area_flash, area);
	int rc = sa_flash_valid(flash);

	if (rc != 0) {
		goto end;
	}

	start += flash->start;
	for (size_t i = 0U; i < iovcnt; i++) {
		rc = flash_read(flash->dev, start, iovec[i].data, iovec[i].len);
		if (rc != 0) {
			break;
		}

		start += iovec[i].len;
	}

	if (rc != 0) {
		LOG_DBG("read failed at %x", start);
	}
end:
	return rc;
}

static int sa_flash_write(const struct storage_area_flash *flash, size_t start,
			  const uint8_t *data, size_t len)
{
	const struct storage_area *area = &flash->area;

	if ((!STORAGE_AREA_AUTOERASE(area)) ||
	    (STORAGE_AREA_FOVRWRITE(area))) {
		return flash_write(flash->dev, flash->start + start, data, len);
	}

	int rc = 0;

	while (len != 0U) {
		const size_t esz = area->erase_size;
		const size_t wrlen = MIN(esz - (start & (esz - 1)), len);

		if ((start & (esz - 1)) == 0U) {
			rc = flash_erase(flash->dev, flash->start + start, esz);
			if (rc != 0) {
				break;
			}
		}

		rc = flash_write(flash->dev, flash->start + start, data, wrlen);
		if (rc != 0) {
			break;
		}

		data += wrlen;
		len -= wrlen;
	}

	return rc;
}

static int sa_flash_writev(const struct storage_area *area, size_t start,
			   const struct storage_area_iovec *iovec,
			   size_t iovcnt)
{
	const struct storage_area_flash *flash =
		CONTAINER_OF(area, struct storage_area_flash, area);
	const size_t align = area->write_size;
	uint8_t buf[align];
	size_t bpos = 0U;
	int rc = sa_flash_valid(flash);

	if (rc != 0) {
		goto end;
	}

	for (size_t i = 0U; i < iovcnt; i++) {
		uint8_t *data8 = (uint8_t *)iovec[i].data;
		size_t blen = iovec[i].len;

		if (bpos != 0U) {
			size_t cplen = MIN(blen, align - bpos);

			memcpy(buf + bpos, data8, cplen);
			bpos += cplen;
			blen -= cplen;
			data8 += cplen;

			if (bpos == align) {
				rc = sa_flash_write(flash, start, buf, align);
				if (rc != 0) {
					break;
				}

				start += align;
				bpos = 0U;
			}
		}

		if (blen >= align) {
			size_t wrlen = blen & ~(align - 1);

			rc = sa_flash_write(flash, start, data8, wrlen);
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

	if (rc != 0) {
		LOG_DBG("prog failed at %x", start);
	}
end:
	return rc;
}

static int sa_flash_erase(const struct storage_area *area, size_t start,
			  size_t len)
{
	const struct storage_area_flash *flash =
		CONTAINER_OF(area, struct storage_area_flash, area);
	int rc = sa_flash_valid(flash);

	if (rc != 0) {
		goto end;
	}

	start *= area->erase_size;
	len *= area->erase_size;

	start += flash->start;
	rc = flash_erase(flash->dev, start, len);
	if (rc != 0) {
		LOG_DBG("erase failed at %x", start);
	}
end:
	return rc;
}

static int sa_flash_ioctl(const struct storage_area *area,
			  enum storage_area_ioctl_cmd cmd, void *data)
{
	const struct storage_area_flash *flash =
		CONTAINER_OF(area, struct storage_area_flash, area);
	int rc = sa_flash_valid(flash);

	if (rc != 0) {
		goto end;
	}

	rc = -ENOTSUP;
	switch (cmd) {
	case SA_IOCTL_XIPADDRESS:
		if (data == NULL) {
			LOG_DBG("No return data supplied");
			rc = -EINVAL;
			break;
		}

		if (flash->xip_address == STORAGE_AREA_FLASH_NO_XIP) {
			rc = -ENOTSUP;
			break;
		}

		uintptr_t *xip_address = (uintptr_t *)data;
		*xip_address = flash->xip_address;
		rc = 0;
		break;

	default:
		break;
	}
end:
	return rc;
}

const struct storage_area_api storage_area_flash_api = {
	.readv = sa_flash_readv,
	.writev = sa_flash_writev,
	.erase = sa_flash_erase,
	.ioctl = sa_flash_ioctl,
};
