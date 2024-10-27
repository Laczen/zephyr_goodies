/*
 * Copyright (c) 2024 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Public API for storage area disk subsystem
 */

#ifndef ZEPHYR_INCLUDE_STORAGE_STORAGE_AREA_DISK_H_
#define ZEPHYR_INCLUDE_STORAGE_STORAGE_AREA_DISK_H_

#include <zephyr/storage/disk_access.h>
#include <zephyr/storage/storage_area/storage_area.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Storage_area_disk interface
 * @defgroup storage_area_disk_interface Storage_area_disk interface
 * @ingroup storage_apis
 * @{
 */

struct storage_area_disk {
	const struct storage_area area;
	const uint32_t start;
	const size_t ssize;
	const char *name;
};

extern const struct storage_area_api storage_area_disk_api;

#define STORAGE_AREA_DISK(_dname, _start, _ssize, _ws, _es, _size, _props)      \
	{                                                                       \
		.area =                                                         \
			{                                                       \
				.api = ((_ws == 0) ||                           \
					((_ws & (_ws - 1)) != 0) ||             \
					((_es % _ws) != 0) ||                   \
					((_size % _es) != 0))                   \
					       ? NULL                           \
					       : &storage_area_disk_api,        \
				.write_size = _ws,                              \
				.erase_size = _es,                              \
				.erase_blocks = _size / _es,                    \
				.props = _props | STORAGE_AREA_PROP_FOVRWRITE,  \
			},                                                      \
		.name = _dname, .start = _start, .ssize = _ssize,               \
	}

#define STORAGE_AREA_DISK_DEFINE(_name, _dname, _start, _ssize, _ws, _es, _size,\
				  _props)					\
	BUILD_ASSERT(_ws != 0, "Invalid write size");				\
	BUILD_ASSERT((_ws & (_ws - 1)) == 0, "Invalid write size");		\
	BUILD_ASSERT((_es % _ws) == 0, "Invalid erase size");			\
	BUILD_ASSERT((_size % _ws) == 0, "Invalid size");			\
	BUILD_ASSERT((_ssize % _ws) == 0, "Invalid sector size");		\
	const struct storage_area_disk _storage_area_##_name =			\
		STORAGE_AREA_DISK(_dname, _start, _ssize, _ws, _es, _size,	\
				  _props)

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_STORAGE_STORAGE_AREA_DISK_H_ */
