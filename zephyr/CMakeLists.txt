# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.13.1)

# find_package(Zephyr) in order to load application boilerplate:
# http://docs.zephyrproject.org/application/application.html
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

# Path to module directory
get_filename_component(MODULE_DIR ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY)

if(CONFIG_ZEPHYR_SHARED_DATA)

zephyr_include_directories(
    ${MODULE_DIR}/include
)
zephyr_library_sources(
    ${MODULE_DIR}/drivers/zephyr_shared_data/zephyr_shared_data.c
)

endif()