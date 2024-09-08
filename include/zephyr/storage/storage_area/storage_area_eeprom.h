/*
 * Copyright (c) 2024 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Public API for storage area eeprom subsystem
 */

#ifndef ZEPHYR_INCLUDE_STORAGE_STORAGE_AREA_EEPROM_H_
#define ZEPHYR_INCLUDE_STORAGE_STORAGE_AREA_EEPROM_H_

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/storage/storage_area/storage_area.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief Storage_area_eeprom interface
 * @defgroup Storage_area_eeprom_interface Storage_area_eeprom interface
 * @ingroup Storage
 * @{
 */

struct storage_area_eeprom {
	const struct storage_area area;
	const struct device *dev;
	const size_t start;
	const size_t size;
};

extern const struct storage_area_api storage_area_eeprom_api;

#define eeprom_storage_area(_dev, _start, _size, _ws, _es, _props)		\
	{									\
		.area = {							\
			.api = &storage_area_eeprom_api,			\
			.props = _props,					\
		},								\
		.dev = _dev,							\
		.start = _start,						\
		.size = _size,							\
	}

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_STORAGE_STORAGE_AREA_EEPROM_H_ */