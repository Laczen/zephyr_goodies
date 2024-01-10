/*
 * Copyright (c) 2024 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Public API for zephyr shared data driver
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_ZEPHYR_SHARED_DATA_H_
#define ZEPHYR_INCLUDE_DRIVERS_ZEPHYR_SHARED_DATA_H_

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
 * @brief Zephyr shared data driver interface
 * @defgroup zephyr_shared_data_interface Zephyr shared data driver interface
 * @ingroup io_interfaces
 * @{
 */

/**
 * @brief Zephyr shared data driver API
 *
 * API to store data in a shared data area.
 */
__subsystem struct zephyr_shared_data_driver_api {
	int (*size)(const struct device *dev, size_t *size);
	int (*read)(const struct device *dev, size_t off, void *data,
		    size_t len);
	int (*prog)(const struct device *dev, size_t off, const void *data,
		    size_t len);
};

/**
 * @brief		Get the size of the zephyr shared data area.
 *
 * @param dev		Zephyr shared data device to use.
 * @param size		Destination for size.
 *
 * @retval		0 on success else negative errno code.
 */
__syscall int zephyr_shared_data_size(const struct device *dev, size_t *size);

static inline int z_impl_zephyr_shared_data_size(const struct device *dev,
						 size_t *size)
{
	struct zephyr_shared_data_driver_api *api =
		(struct zephyr_shared_data_driver_api *)dev->api;

	if (api->size == NULL) {
		return -ENOTSUP;
	}

	return api->size(dev, size);
}

/**
 * @brief		Read data from the zephyr shared data area.
 *
 * @param dev		Zephyr shared data device to use.
 * @param off		Offset in zephyr shared data area.
 * @param data		Destination for data.
 * @param len		Read length.
 *
 * @retval		0 on success else negative errno code.
 */
__syscall int zephyr_shared_data_read(const struct device *dev, size_t off,
				      void *data, size_t len);

static inline int z_impl_zephyr_shared_data_read(const struct device *dev,
						 size_t off, void *data,
						 size_t len)
{
	struct zephyr_shared_data_driver_api *api =
		(struct zephyr_shared_data_driver_api *)dev->api;

	if (api->read == NULL) {
		return -ENOTSUP;
	}

	return api->read(dev, off, data, len);
}

/**
 * @brief		Program data to the zephyr shared data area.
 *
 * @param dev		Zephyr shared data device to use.
 * @param off		Offset in zephyr shared data area.
 * @param data		Data to program.
 * @param len		Data length.
 *
 * @retval		0 on success else negative errno code.
 */
__syscall int zephyr_shared_data_prog(const struct device *dev, size_t off,
				      const void *data, size_t len);

static inline int z_impl_zephyr_shared_data_prog(const struct device *dev,
						 size_t off, const void *data,
						 size_t len)
{
	struct zephyr_shared_data_driver_api *api =
		(struct zephyr_shared_data_driver_api *)dev->api;

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

#include <syscalls/zephyr_shared_data.h>

#endif /* ZEPHYR_INCLUDE_DRIVERS_ZEPHYR_SHARED_DATA_H_ */