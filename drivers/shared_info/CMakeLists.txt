# SPDX-License-Identifier: Apache-2.0

# Find path to module directory
get_filename_component(ZSD_DIR ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY)
get_filename_component(DRV_DIR ${ZSD_DIR} DIRECTORY)
get_filename_component(MOD_DIR ${DRV_DIR} DIRECTORY)

zephyr_syscall_header(${DRV_DIR}/include/zephyr/drivers/shared_info.h)

zephyr_library()
zephyr_library_sources_ifdef(CONFIG_USERSPACE shared_info_handlers.c)
zephyr_library_sources(shared_info.c)