# SPDX-License-Identifier: Apache-2.0

zephyr_library()
zephyr_library_sources_ifdef(CONFIG_SETTINGS_STORAGE_AREA_STORE
	settings_storage_area_store.c)
