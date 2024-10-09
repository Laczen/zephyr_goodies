# Copyright (c) 2024 Laczen
# SPDX-License-Identifier: Apache-2.0

add_subdirectory(storage)
add_subdirectory_ifdef(CONFIG_SETTINGS_STORAGE_AREA_STORE settings)
