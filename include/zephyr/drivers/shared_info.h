/*
 * Copyright (c) 2024 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Public API for shared info driver
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_SHARED_INFO_H_
#define ZEPHYR_INCLUDE_DRIVERS_SHARED_INFO_H_

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Shared info driver interface
 * @defgroup shared_info_interface Shared info driver interface
 * @ingroup io_interfaces
 * @{
 */

/**
 * @brief Shared info driver API
 *
 * API to store data in a shared info area.
 */
__subsystem struct shared_info_driver_api {
	int (*size)(const struct device *dev, size_t *size);
	int (*read)(const struct device *dev, size_t off, void *data,
		    size_t len);
	int (*prog)(const struct device *dev, size_t off, const void *data,
		    size_t len);
};

/**
 * @brief		Get the size of a shared info area.
 *
 * @param dev		Shared info device to use.
 * @param size		Destination for size.
 *
 * @retval		0 on success else negative errno code.
 */
__syscall int shared_info_size(const struct device *dev, size_t *size);

static inline int z_impl_shared_info_size(const struct device *dev,
						 size_t *size)
{
	struct shared_info_driver_api *api =
		(struct shared_info_driver_api *)dev->api;

	if (api->size == NULL) {
		return -ENOTSUP;
	}

	return api->size(dev, size);
}

/**
 * @brief		Read data from a shared info area.
 *
 * @param dev		Shared info device to use.
 * @param off		Offset in shared info area.
 * @param data		Destination for data.
 * @param len		Read length.
 *
 * @retval		0 on success else negative errno code.
 */
__syscall int shared_info_read(const struct device *dev, size_t off,
				      void *data, size_t len);

static inline int z_impl_shared_info_read(const struct device *dev,
						 size_t off, void *data,
						 size_t len)
{
	struct shared_info_driver_api *api =
		(struct shared_info_driver_api *)dev->api;

	if (api->read == NULL) {
		return -ENOTSUP;
	}

	return api->read(dev, off, data, len);
}

/**
 * @brief		Program data to a shared info area.
 *
 * @param dev		Shared info device to use.
 * @param off		Offset in shared info area.
 * @param data		Data to program.
 * @param len		Data length.
 *
 * @retval		0 on success else negative errno code.
 */
__syscall int shared_info_prog(const struct device *dev, size_t off,
				      const void *data, size_t len);

static inline int z_impl_shared_info_prog(const struct device *dev,
						 size_t off, const void *data,
						 size_t len)
{
	struct shared_info_driver_api *api =
		(struct shared_info_driver_api *)dev->api;

	if (api->prog == NULL) {
		return -ENOTSUP;
	}

	return api->prog(dev, off, data, len);
}

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#include <syscalls/shared_info.h>

#endif /* ZEPHYR_INCLUDE_DRIVERS_SHARED_INFO_H_ */