# SPDX-License-Identifier: Apache-2.0

zephyr_library()
zephyr_library_sources(storage_area.c)
zephyr_library_sources_ifdef(CONFIG_STORAGE_AREA_FLASH storage_area_flash.c)
zephyr_library_sources_ifdef(CONFIG_STORAGE_AREA_EEPROM storage_area_eeprom.c)
