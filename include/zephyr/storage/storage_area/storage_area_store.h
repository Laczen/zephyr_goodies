/*
 * Copyright (c) 2024 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Public API for storing data on a storage area 
 *
 * The subsystem that enables storage of data on top of the storage area
 * subsystem. The data is stored in a very simple format:
 *	magic | wrapcnt | data length | data | crc32
 * with:
 *	magic (1 byte): 0b10010110,
 *	wrapcnt (1 byte): increased each time storage area is wrapped,
 *      data length (2 byte): little endian uint16_t,
 *	crc32 (4 byte): little endian uint32_t,
 *
 * The storage area is divided into constant sized sectors that are either
 * a whole divider or a multiple of the storage area erase blocks. Data is
 * written to a sector consecutively and each write needs to align to the
 * write size of the storage area. Data can be written to a sector until
 * space is exhausted.
 * When there is no space in a sector a write attempt will fail and returns
 * -ENOSPC. The sector can that then be closed. Closing a sector returns
 * information on the next sector that will be used for writing. This
 * information can be used to prepare a next storage area erase block for
 * writing and/or to move any data to ensure persistent storage. 
 */

#ifndef ZEPHYR_INCLUDE_STORAGE_STORAGE_AREA_STORE_H_
#define ZEPHYR_INCLUDE_STORAGE_STORAGE_AREA_STORE_H_

#include <stdint.h>
#include <stddef.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief Storage_area_store interface
 * @defgroup Storage_area_store_interface Storage_area_store interface
 * @ingroup Storage
 * @{
 */

struct storage_area_store;

struct sas_entry_info {
	size_t sector;
	size_t position;
	uint16_t magic;
	uint16_t size;
};

struct sas_entry {
	struct storage_area_store *store;
	struct sas_entry_info info;
};

struct storage_area;

struct storage_area_erase_ctx {
	size_t start;	/* first block to erase */
	size_t len;	/* number of blocks to erase */
};

struct storage_area_db { /* storage area data block */
	void *data;	/* pointer to data */
	size_t len;	/* data size */
};

enum storage_area_properties_mask {
	SA_PROP_READONLY = 0x0001,	
	SA_PROP_OVRWRITE = 0x0002,	/* overwrite is allowed */
	SA_PROP_SUBWRITE = 0x0004,	/* sub writesize write is allowed */
	SA_PROP_ZEROERASE = 0x0008,	/* erased value is 0x00 */
};

enum storage_area_ioctl_cmd {
	SA_IOCTL_NONE,
	SA_IOCTL_SIZE,		/* retrieve the storage area size */
	SA_IOCTL_WRITESIZE,	/* retrieve the storage area write block size */
	SA_IOCTL_ERASESIZE,	/* retrieve the storage area erase block size */
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
	int (*readblocks)(const struct storage_area *area, size_t start,
			  const struct storage_area_db *db, size_t bcnt);
	int (*progblocks)(const struct storage_area *area, size_t start,
			  const struct storage_area_db *db, size_t bcnt);
	int (*ioctl)(const struct storage_area *area,
	             enum storage_area_ioctl_cmd cmd, void *data);
};

/**
 * @brief Storage_area struct
 */
struct storage_area {
	const struct storage_area_api *api;
	size_t props;		/* bitfield of storage area properties */
};

/**
 * @brief Storage_area macros
 */
#define STORAGE_AREA_HAS_PROPERTY(area, prop) ((area->props & prop) == prop)
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

/**
 * @brief	Total size of storage area datablocks.
 *
 * @param db	pointer to datablocks.
 * @param bcnt	datablock count.
 *
 * @retval	Total size.
 */
size_t storage_area_dbsize(const struct storage_area_db *db, size_t bcnt);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_STORAGE_STORAGE_AREA_STORE_H_ */