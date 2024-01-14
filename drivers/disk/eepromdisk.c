/*
 * Copyright (c) 2016 Intel Corporation.
 * Copyright (c) 2021,2023 Nordic Semiconductor ASA
 * Copyright (c) 2024 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <zephyr/types.h>
#include <zephyr/drivers/disk.h>
#include <errno.h>
#include <zephyr/init.h>
#include <zephyr/device.h>
#include <zephyr/drivers/eeprom.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(eepromdisk, CONFIG_EEPROMDISK_LOG_LEVEL);

struct eeprom_disk_config {
	const size_t sector_size;
	const size_t sector_count;
	const struct device *const eeprom_dev;
	const uint32_t eeprom_off;
	const bool eeprom_ro;
};

static uint32_t lba_to_address(const struct device *dev, uint32_t lba)
{
	const struct eeprom_disk_config *config = dev->config;

	return lba * config->sector_size;
}

static int disk_eeprom_access_status(struct disk_info *disk)
{
	return DISK_STATUS_OK;
}

static int disk_eeprom_access_init(struct disk_info *disk)
{
	return 0;
}

static int disk_eeprom_access_read(struct disk_info *disk, uint8_t *buff,
				   uint32_t sector, uint32_t count)
{
	const struct device *dev = disk->dev;
	const struct eeprom_disk_config *config = dev->config;
	const uint32_t scount = config->sector_count;

	if ((scount < count) || (scount - count) < sector) {
		LOG_ERR("Read outside disk range");
		return -EIO;
	}

	return eeprom_read(config->eeprom_dev,
			   config->eeprom_off + lba_to_address(dev, sector),
			   buff, count * config->sector_size);
}

static int disk_eeprom_access_write(struct disk_info *disk, const uint8_t *buff,
				    uint32_t sector, uint32_t count)
{
	const struct device *dev = disk->dev;
	const struct eeprom_disk_config *config = dev->config;
	const uint32_t scount = config->sector_count;

	if (config->eeprom_ro) {
		return -ENOTSUP;
	}

	if ((scount < count) || (scount - count) < sector) {
		LOG_ERR("Write outside disk range");
		return -EIO;
	}

	return eeprom_write(config->eeprom_dev,
			    config->eeprom_off + lba_to_address(dev, sector),
			    buff, count * config->sector_size);
}

static int disk_eeprom_access_ioctl(struct disk_info *disk, uint8_t cmd,
				    void *buff)
{
	const struct eeprom_disk_config *config = disk->dev->config;

	switch (cmd) {
	case DISK_IOCTL_CTRL_SYNC:
		break;
	case DISK_IOCTL_GET_SECTOR_COUNT:
		*(uint32_t *)buff = config->sector_count;
		break;
	case DISK_IOCTL_GET_SECTOR_SIZE:
		*(uint32_t *)buff = config->sector_size;
		break;
	case DISK_IOCTL_GET_ERASE_BLOCK_SZ:
		*(uint32_t *)buff = 1U;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int disk_eeprom_init(const struct device *dev)
{
	struct disk_info *info = (struct disk_info *)dev->data;

	info->dev = dev;
	return disk_access_register(info);
}

static const struct disk_operations eeprom_disk_ops = {
	.init = disk_eeprom_access_init,
	.status = disk_eeprom_access_status,
	.read = disk_eeprom_access_read,
	.write = disk_eeprom_access_write,
	.ioctl = disk_eeprom_access_ioctl,
};

#define DT_DRV_COMPAT zephyr_eeprom_disk

#define EEPROM_DEVICE(n)     DEVICE_DT_GET(DT_INST_PHANDLE(n, eeprom))
#define EEPROM_OFFSET(n)     DT_INST_PROP_OR(n, eeprom_offset, 0)
#define EEPROM_SIZE(n)       DT_INST_PROP_BY_PHANDLE(n, eeprom, size)
#define EEPROM_ASIZE(n)      EEPROM_SIZE(n) - EEPROM_OFFSET(n)
#define EEPROM_RO(n)         DT_INST_PROP_BY_PHANDLE(n, eeprom, read_only)
#define EEPROM_DISK_SIZE(n)  DT_INST_PROP_OR(n, disk_size, (EEPROM_ASIZE(n)))
#define EEPROM_DISK_SSIZE(n) DT_INST_PROP(n, sector_size)
#define EEPROM_DISK_SCNT(n)  EEPROM_DISK_SIZE(n) / EEPROM_DISK_SSIZE(n)

#define EEPROM_DISK_SIZE_OK(n)  (EEPROM_ASIZE(n) >= EEPROM_DISK_SIZE(n))
#define EEPROM_DISK_ALIGN_OK(n) (EEPROM_DISK_SIZE(n) % EEPROM_DISK_SSIZE(n) == 0)

#define EEPROMDISK_DEVICE_CONFIG_DEFINE(n)                                      \
                                                                                \
	static struct eeprom_disk_config disk_config_##n = {                    \
		.sector_size = EEPROM_DISK_SSIZE(n),                            \
		.sector_count = EEPROM_DISK_SCNT(n),                            \
		.eeprom_dev = EEPROM_DEVICE(n),                                 \
		.eeprom_off = EEPROM_OFFSET(n),                                 \
		.eeprom_ro = EEPROM_RO(n),                                      \
	}

#define EEPROMDISK_DEVICE_DEFINE(n)                                             \
	BUILD_ASSERT(EEPROM_DISK_SIZE_OK(n), "Disk does not fit on eeprom");    \
	BUILD_ASSERT(EEPROM_DISK_ALIGN_OK(n), "Disk is not a sector multiple"); \
                                                                                \
	static struct disk_info disk_info_##n = {                               \
		.name = DT_INST_PROP(n, disk_name),                             \
		.ops = &eeprom_disk_ops,                                        \
	};                                                                      \
                                                                                \
	EEPROMDISK_DEVICE_CONFIG_DEFINE(n);                                     \
                                                                                \
	DEVICE_DT_INST_DEFINE(n, disk_eeprom_init, NULL, &disk_info_##n,        \
			      &disk_config_##n, POST_KERNEL,                    \
			      CONFIG_DISK_DRIVER_EEPROM_INIT_PRIORITY,          \
			      &eeprom_disk_ops);

DT_INST_FOREACH_STATUS_OKAY(EEPROMDISK_DEVICE_DEFINE)