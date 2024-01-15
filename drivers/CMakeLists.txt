# Copyright (c) 2024 Laczen
# SPDX-License-Identifier: Apache-2.0

add_subdirectory(disk)
add_subdirectory_ifdef(CONFIG_SHARED_INFO shared_info)
