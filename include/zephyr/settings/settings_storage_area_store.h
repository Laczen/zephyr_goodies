/*
 * Copyright (c) 2024 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __SETTINGS_STORAGE_AREA_STORE_H_
#define __SETTINGS_STORAGE_AREA_STORE_H_

#include <zephyr/storage/storage_area/storage_area_store.h>
#include <zephyr/settings/settings.h>

#ifdef __cplusplus
extern "C" {
#endif

/* In the storage area store backend, each setting is stored in one record:
 *	1. setting's name size (uint8_t)
 *	2. setting's name
 *      3. setting's value
 *
 * Deleted settings are stored without a settings value
 */

struct settings_storage_area_store {
	struct settings_store store;
	struct storage_area_store *sa_store;
};

extern const struct settings_store_itf settings_storage_area_store_itf;

#define create_settings_storage_area_store(_name, _storage_area_store_ptr)	\
	struct settings_storage_area_store 					\
		_settings_storage_area_store_ ## _name = {			\
		.store.cs_itf = &settings_storage_area_store_itf,		\
		.sa_store = _storage_area_store_ptr,				\
	}
	
#define get_settings_storage_area_store(_name)					\
	&(_settings_storage_area_store_ ## _name)

#define get_settings_storage_area_store_settings_store(_name)			\
	&((_settings_storage_area_store_ ## _name).store)
#ifdef __cplusplus
}
#endif

#endif /* __SETTINGS_STORAGE_AREA_STORE_H_ */
