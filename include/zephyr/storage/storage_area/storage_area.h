/*
 * Copyright (c) 2024 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Public API for storage area subsystem
 *
 * The storage area API is a subsystem that creates a unified method to work
 * with flash, eeprom, ram, disks, files, ... for storage. A storage area is
 * an area that has a number of constant sized erase blocks, and has constant
 * write block size.  
 *
 * There are only 3 methods exposed:
 *   readblocks() requires no alignment,
 *   writeblocks() requires write_size alignment,
 *   ioctl(),
 *
 * The subsystem is easy extendable to create custom (virtual) storage areas
 * that consist of e.g. a combination of flash and ram, a encrypted storage
 * area, ...
 *
 * A storage area is defined e.g. for flash:
 * struct storage_area_flash fa =
 * 	flash_storage_area(dev, start, size, xip_address, write_size,
 *                         erase_size, properties);
 * struct storage_area *area = &fa.area;
 *
 * The write_size, erase_size, ... are declarations of how the storage_area
 * will be used.
 * 
 */

#ifndef ZEPHYR_INCLUDE_STORAGE_STORAGE_AREA_H_
#define ZEPHYR_INCLUDE_STORAGE_STORAGE_AREA_H_

#include <stdint.h>
#include <stddef.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Storage_area interface
 * @defgroup Storage_area_interface Storage_area interface
 * @ingroup Storage
 * @{
 */

struct storage_area;

struct storage_area_erase_ctx {
	size_t start;	/* first block to erase */
	size_t count;	/* number of blocks to erase */
};

enum storage_area_properties_mask {
	SA_PROP_READONLY = 0x0001,	
	SA_PROP_OVRWRITE = 0x0002,	/* overwrite is allowed */
	SA_PROP_SUBWRITE = 0x0004,	/* sub writesize write is allowed */
	SA_PROP_ZEROERASE = 0x0008,	/* erased value is 0x00 */
};

enum storage_area_ioctl_cmd {
	SA_IOCTL_NONE,
	SA_IOCTL_WRITEPREPARE,	/* prepare erase region for writing */
	SA_IOCTL_ERASE,		/* erase region */
	SA_IOCTL_XIPADDRESS,	/* retrieve the storage area xip address */
};

/**
 * @brief Storage_area API
 *
 * API to access storage area.
 */
struct storage_area_api {
	int (*read)(const struct storage_area *area, size_t start, void *data,
		    size_t len);
	int (*prog)(const struct storage_area *area, size_t start,
		    const void *data, size_t len);
	int (*ioctl)(const struct storage_area *area,
	             enum storage_area_ioctl_cmd cmd, void *data);
};

/**
 * @brief Storage_area struct
 */
struct storage_area {
	const struct storage_area_api *api;
	size_t write_size;
	size_t write_blocks;
	size_t erase_size;
	uint32_t props;		/* bitfield of storage area properties */
};

/**
 * @brief Storage_area macros
 */
#define STORAGE_AREA_HAS_PROPERTY(area, prop) ((area->props & prop) == prop)
#define STORAGE_AREA_WRITESIZE(area) area->write_size
#define STORAGE_AREA_ERASESIZE(area) area->erase_size
#define STORAGE_AREA_SIZE(area) (area->write_size * area->write_blocks)
#define STORAGE_AREA_OVRWRITE(area)						\
	STORAGE_AREA_HAS_PROPERTY(area, SA_PROP_OVRWRITE)
#define STORAGE_AREA_ERASEVALUE(area)						\
	STORAGE_AREA_HAS_PROPERTY(area, SA_PROP_ZEROERASE) ? 0x00 : 0xff

/**
 * @brief 	Storage area ioctl.
 *
 * @param area	storage area.
 * @param cmd	ioctl command.
 * @param data	in/out data pointer.
 *
 * @retval	0 on success else negative errno code.
 */
int storage_area_ioctl(const struct storage_area *area,
		       enum storage_area_ioctl_cmd cmd, void *data);

/**
 * @brief	Read storage area.
 *
 * @param area	storage area.
 * @param start	start in storage area (byte).
 * @param data	pointer for read data.
 * @param len	read length (byte).
 *
 * @retval	0 on success else negative errno code.
 */
int storage_area_read(const struct storage_area *area, size_t start,
		      void *data, size_t len);

/**
 * @brief	Program storage area.
 *
 * @param area	storage area.
 * @param start	start in storage area (byte).
 * @param data	pointer to data.
 * @param bcnt	write length (byte).
 *
 * @retval	0 on success else negative errno code.
 */
int storage_area_prog(const struct storage_area *area, size_t start,
		      const void *data, size_t len);

struct storage_area_db { /* storage area data block */
	void *data;	/* pointer to data */
	size_t len;	/* data size */
};

/**
 * @brief	Read storage area blocks.
 *
 * @param area	storage area.
 * @param start	start in storage area (byte).
 * @param db	data blocks for read.
 * @param bcnt	data block count.
 *
 * @retval	0 on success else negative errno code.
 */
int storage_area_readblocks(const struct storage_area *area, size_t start,
			    const struct storage_area_db *db, size_t bcnt);

/**
 * @brief	Program storage area blocks.
 *
 * @param area	storage area.
 * @param start	start in storage area (byte).
 * @param db	data blocks for program.
 * @param bcnt	data block count.
 *
 * @retval	0 on success else negative errno code.
 */
int storage_area_progblocks(const struct storage_area *area, size_t start,
			    const struct storage_area_db *db, size_t bcnt);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_STORAGE_STORAGE_AREA_H_ */