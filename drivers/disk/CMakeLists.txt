# Copyright (c) 2024 Laczen
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_DISK_DRIVERS)

zephyr_library()

zephyr_library_sources_ifdef(CONFIG_DISK_DRIVER_EEPROM eepromdisk.c)

endif()