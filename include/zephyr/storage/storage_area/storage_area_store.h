/*
 * Copyright (c) 2024 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @brief A storage area store is a record storage solution on a storage
 *        area
 *
 * The storage area store enables storage of records on top of a storage
 * area. The record format is:
 * - magic | data size | data | crc32
 * .
 * with:
 * - magic (2 byte): second byte is a wrapcnt variable that is increased
 *   each time storage area is wrapped around,
 * - data size (2 byte): little endian uint16_t,
 * - crc32 (4 byte): little endian uint32_t, calculated over (part of) data,
 * .
 * The storage area is divided into constant sized sectors that are either a
 * whole divider or a multiple of the storage area erase blocks.
 *
 * Records are written consecutively to a sector. Each record is aligned to
 * the write size of the storage area. Records can be written to a sector
 * until space is exhausted (write return `-ENOSPC`).
 *
 * To create space for new record the storage area store can be `advanced` or
 * `compacted`. The `advance` method will simply take into use a next sector.
 * The `compact` method will move certain records to the front of the storage
 * area store. The `compact` method uses a callback routine `move` to
 * determine if a record should be kept.
 *
 * At the start of each sector a configurable `cookie` is (optionally) added,
 * this `cookie` can be used to describe the data format and or a data version
 * used inside a record.
 *
 * Several types of record stores can be created:
 * - Persistent circular buffer: uses user supplied check routines to move old
 *   data to the front of the buffer to ensure records are persistent,
 * - Simple circular buffer: data overwrites old data when space is exhausted,
 * - Read-only record store: when only reading is required,
 * .
 *
 * A Persistent circular buffer store is defined and retrieved as:
 * @code{.c}
 * STORAGE_AREA_STORE_PCB_DEFINE(name, ...);
 * struct storage_area_store *store = GET_STORAGE_AREA_STORE(name);
 * @endcode
 *
 * The part of data that is not included in the crc calculation can be updated
 * (if the storage area allows it). Updating the part of data that is not
 * included in the crc calculation can be used to mark a record as invalid.
 *
 * @defgroup storage_area_store Storage area store
 * @ingroup storage_apis
 * @{
 */

#ifndef ZEPHYR_INCLUDE_STORAGE_STORAGE_AREA_STORE_H_
#define ZEPHYR_INCLUDE_STORAGE_STORAGE_AREA_STORE_H_

#include <stdbool.h>
#include <zephyr/kernel.h>
#include <zephyr/storage/storage_area/storage_area.h>

#ifdef __cplusplus
extern "C" {
#endif

struct storage_area_record;

struct storage_area_store_compact_cb {
	/**
	 * move is called to evaluate if a record should be
	 * kept (moved to the front) or not.
	 */
	bool (*move)(const struct storage_area_record *record);
	/**
	 * move_cb is called after moving a record and can e.g.
	 * be used to update a hash table of records.
	 */
	void (*move_cb)(const struct storage_area_record *orig,
			const struct storage_area_record *dest);
};

struct storage_area_store;

struct storage_area_store_api {
	/* mount routine should always be defined, cb is NULL
	 * for read-only or simple circular buffer.
	 */
	int (*mount)(const struct storage_area_store *store,
		     const struct storage_area_store_compact_cb *cb);
	/* writev routine should be NULL for read-only, needs
	 * definition for simple or persistent circular buffer.
	 */
	int (*writev)(const struct storage_area_store *store,
		      const struct storage_area_iovec *iovec, size_t iovcnt);
	/* advance routine should be NULL for read-only, needs
	 * definition for simple or persistent circular buffer,
	 * for simple circular buffer cb is NULL.
	 */
	int (*advance)(const struct storage_area_store *store,
		       const struct storage_area_store_compact_cb *cb);
};

struct storage_area_store_data {
#ifdef CONFIG_STORAGE_AREA_STORE_SEMAPHORE
	struct k_sem semaphore;
#endif
	bool ready;
	/** current write sector */
	size_t sector;
	/** current write location */
	size_t loc;
	/** current wrap counter */
	uint8_t wrapcnt;
};

struct storage_area_store {
	const struct storage_area *area;
	const struct storage_area_store_api *api;
	struct storage_area_store_data *data;
	void *sector_cookie;
	size_t sector_cookie_size;
	size_t sector_size;
	size_t sector_cnt;
	size_t spare_sectors;
	/** bytes excluded from crc calculation */
	size_t crc_skip;
};

struct storage_area_record {
	struct storage_area_store *store;
	size_t sector;
	size_t loc;
	size_t size;
};

/**
 * @brief	Mount storage area store.
 *
 * @param store	storage area store.
 * @param cb	routines used during compacting (can be NULL).
 *
 * @retval	0 on success else negative errno code.
 */
int storage_area_store_mount(const struct storage_area_store *store,
			     const struct storage_area_store_compact_cb *cb);

/**
 * @brief	Unmount storage area store.
 *
 * @param store	storage area store.
 *
 * @retval	0 on success else negative errno code.
 */
int storage_area_store_unmount(const struct storage_area_store *store);

/**
 * @brief	Wipe storage area store (storage area needs to be unmounted).
 *
 * @param store	storage area store.
 *
 * @retval	0 on success else negative errno code.
 */
int storage_area_store_wipe(const struct storage_area_store *store);

/**
 * @brief	Write iovec to storage area store.
 *
 * @param store	 storage area store.
 * @param iovec	 io vector to write (see storage_area_iovec).
 * @param iovcnt iovec elements.
 *
 * @retval	0 on success else negative errno code.
 */
int storage_area_store_writev(const struct storage_area_store *store,
			      const struct storage_area_iovec *iovec,
			      size_t iovcnt);

/**
 * @brief	Write data to storage area store.
 *
 * @param store	storage area store.
 * @param data	data.
 * @param len	write size.
 *
 * @retval	0 on success else negative errno code.
 */
int storage_area_store_write(const struct storage_area_store *store,
			     const void *data, size_t len);

/**
 * @brief	Advance storage area store.
 *		Take a new sector into use. This might erase old data.
 *		REMARK: This can be a slow operation.
 *
 * @param store	storage area store.
 *
 * @retval	0 on success else negative errno code.
 */
int storage_area_store_advance(const struct storage_area_store *store);

/**
 * @brief	Compact storage area store.
 *		Reduce the used storage area space by removing obsolete
 *              record and moving records that need keeping.
 *		REMARK: This can be a slow operation.
 *
 * @param store	storage area store.
 * @param cb    pointer to compact callback routines
 *
 * @retval	0 on success else negative errno code.
 */
int storage_area_store_compact(const struct storage_area_store *store,
			       const struct storage_area_store_compact_cb *cb);

/**
 * @brief	 Retrieve the next record of the store. To get the first
 *		 record set the record.store to NULL.
 *
 * @param store	 storage area store.
 * @param record returned storage area record.
 *
 * @retval	 0 on success else negative errno code.
 */
int storage_area_record_next(const struct storage_area_store *store,
			     struct storage_area_record *record);

/**
 * @brief	 Validate a record (crc checks out)
 *
 * @param record storage area record.
 *
 * @retval	 true if record is valid else false.
 */
bool storage_area_record_valid(const struct storage_area_record *record);

/**
 * @brief	 Read iovec from a record
 *
 * @param record storage area record.
 * @param start  offset in the record (data).
 * @param iovec	 io vector to read (see storage_area_iovec).
 * @param iovcnt iovec elements.
 *
 * @retval	 0 on success else negative errno code.
 */
int storage_area_record_readv(const struct storage_area_record *record,
			      size_t start,
			      const struct storage_area_iovec *iovec,
			      size_t iovcnt);

/**
 * @brief	 Direct data read from a record
 *
 * @param record storage area record.
 * @param start  offset in the record (data).
 * @param data   data.
 * @param len	 read size.
 *
 * @retval	 0 on success else negative errno code.
 */
int storage_area_record_read(const struct storage_area_record *record,
			     size_t start, void *data, size_t len);

/**
 * @brief	 Update the start of record data. This is only possible if the
 *		 storage area supports multiple writes and the allowed update
 *		 data may be limited (e.g. only toggle bits from 1 to 0).
 *		 This can be used to invalidate records.
 *
 * @param record storage area record.
 * @param data	 new data to write.
 * @param len	 write size.
 * @retval	 0 on success else negative errno code.
 */
int storage_area_record_update(const struct storage_area_record *record,
			       void *data, size_t len);

/**
 * @brief	 Get the cookie of a sector
 *
 * @param store  storage area store.
 * @param sector the sector number.
 * @param cookie destination.
 * @param cksz   cookie size (size of the destination buffer).
 *
 * @retval	 0 on success else negative errno code.
 */
int storage_area_store_get_sector_cookie(const struct storage_area_store *store,
					 size_t sector, void *cookie,
					 size_t cksz);

/**
 * @brief Helper macro to create a storage area store on top of a storage area
 */
#define STORAGE_AREA_STORE(_area, _api, _data, _cookie, _cookie_size,           \
			   _sector_size, _sector_cnt, _spare_sectors,           \
			   _crc_skip)                                           \
	{                                                                       \
		.area = _area, .api = _api, .data = _data,                      \
		.sector_cookie = _cookie, .sector_cookie_size = _cookie_size,   \
		.sector_size = _sector_size, .sector_cnt = _sector_cnt,         \
		.spare_sectors = _spare_sectors, .crc_skip = _crc_skip,         \
	}

/* persistent circular buffer */
extern const struct storage_area_store_api storage_area_store_pcb_api;

/**
 * @brief Define a persistent circular buffer storage area store
 *
 *        A storage area store is used to store records in constant sized
 *        sectors. It is made persistent by designating a number of sectors
 *        to be spare (unused). When storage space is exhausted the persistent
 *        circular buffer store will use a user supplied move methods to
 *        identify the data that needs to be copied to the spare sectors. When
 *        the copy of all data in a sector is performed the old data sector
 *        becomes a spare sector.
 *
 * @param _name          storage area store name (used with
 *                       GET_STORAGE_AREA_STORE(_name))
 * @param _area          storage area
 * @param _cookie        pointer to a cookie that is added at the start of each
 *                       sector, this can be used to identify the underlying
 *                       data(format)
 * @param _cookie_size   size of the cookie
 * @param _sector_size   sector size
 * @param _sector_cnt    sector count
 * @param _spare_sectors number of unused sectors
 * @param _crc_skip      each record is stored with a crc32_ieee, _crc_skip
 *                       allows excluded the first _crc_skip bytes from the
 *                       crc calculation. This can be used to invalidate certain
 *                       records while keeping the crc valid.
 *
 */

#define STORAGE_AREA_STORE_PCB_DEFINE(_name, _area, _cookie, _cookie_size,      \
				      _sector_size, _sector_cnt,                \
				      _spare_sectors, _crc_skip)                \
	static struct storage_area_store_data                                   \
		_storage_area_store_##_name##_data;                             \
	const struct storage_area_store _storage_area_store_##_name =           \
		STORAGE_AREA_STORE(_area, &storage_area_store_pcb_api,          \
				   &(_storage_area_store_##_name##_data),       \
				   _cookie, _cookie_size, _sector_size,         \
				   _sector_cnt, _spare_sectors, _crc_skip)

/* simple circular buffer */
extern const struct storage_area_store_api storage_area_store_scb_api;

/**
 * @brief Define a simple circular buffer storage area store
 *
 *        A simple circular buffer is similar to the persistent circular
 *        buffer (@ref STORAGE_AREA_STORE_PCB_DEFINE) but it does not move
 *        old data, instead it erases or writes over old data. A simple
 *        circular buffer is typically used for storing measurement data in
 *        a fixed size space.
 *        For the parameters see (@ref STORAGE_AREA_PCB_DEFINE), except the
 *        spare sectors that are 0 in this case.
 *
 */
#define STORAGE_AREA_STORE_SCB_DEFINE(_name, _area, _cookie, _cookie_size,      \
				      _sector_size, _sector_cnt, _crc_skip)     \
	static struct storage_area_store_data                                   \
		_storage_area_store_##_name##_data;                             \
	const struct storage_area_store _storage_area_store_##_name =           \
		STORAGE_AREA_STORE(_area, &storage_area_store_scb_api,          \
				   &(_storage_area_store_##_name##_data),       \
				   _cookie, _cookie_size, _sector_size,         \
				   _sector_cnt, 0U, _crc_skip)

/* read-only api */
extern const struct storage_area_store_api storage_area_store_ro_api;

/**
 * @brief Define a read-only storage area store
 *
 *        When a storage area store is only used to read it can be defined
 *        in read-only mode that reduces the required rom space. The parameters
 *        of the read-only storage area store should be defined in accordance
 *        with the parameters used when creating the storage area store.
 *
 *        For the parameters see (@ref STORAGE_AREA_PCB_DEFINE).
 *
 */
#define STORAGE_AREA_STORE_RO_DEFINE(_name, _area, _cookie, _cookie_size,       \
				     _sector_size, _sector_cnt, _spare_sectors, \
				     _crc_skip)                                 \
	static struct storage_area_store_data                                   \
		_storage_area_store_##_name##_data;                             \
	const struct storage_area_store _storage_area_store_##_name =           \
		STORAGE_AREA_STORE(_area, &storage_area_store_ro_api,           \
				   &(_storage_area_store_##_name##_data),       \
				   _cookie, _cookie_size, _sector_size,         \
				   _sector_cnt, _spare_sectors, _crc_skip)

/**
 * @brief retrieve a pointer to a storage area store
 *
 * @param _name storage area store name
 */
#define GET_STORAGE_AREA_STORE(_name)                                           \
	(struct storage_area_store *)&(_storage_area_store_##_name)

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_STORAGE_STORAGE_AREA_STORE_H_ */
