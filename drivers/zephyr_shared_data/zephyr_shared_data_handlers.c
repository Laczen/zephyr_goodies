/*
 * Copyright (c) 2024, Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/zephyr_shared_data.h>
#include <zephyr/internal/syscall_handler.h>

static inline ssize_t z_vrfy_zephyr_shared_data_size(const struct device *dev,
						     size_t *size)
{
	K_OOPS(K_SYSCALL_OBJ(dev, K_OBJ_DRIVER_ZEPHYR_SHARED_DATA));
	K_OOPS(K_SYSCALL_MEMORY_WRITE(size, sizeof(size_t)));
	return z_impl_zephyr_shared_data_size(dev, size);
}
#include <syscalls/zephyr_shared_data_size_mrsh.c>

static inline int z_vrfy_zephyr_shared_data_read(const struct device *dev,
						 size_t off, void *data,
						 size_t len)
{
	K_OOPS(K_SYSCALL_OBJ(dev, K_OBJ_DRIVER_ZEPHYR_SHARED_DATA));
	K_OOPS(K_SYSCALL_MEMORY_WRITE(data, len));
	return z_impl_zephyr_shared_data_read(dev, off, data, len);
}
#include <syscalls/zephyr_shared_data_read_mrsh.c>

static inline int z_vrfy_zephyr_shared_data_prog(const struct device *dev,
						 size_t off, const void *data,
						 size_t len)
{
	K_OOPS(K_SYSCALL_OBJ(dev, K_OBJ_DRIVER_ZEPHYR_SHARED_DATA));
	K_OOPS(K_SYSCALL_MEMORY_READ(data, len));
	return z_impl_zephyr_shared_data_prog(dev, off, data, len);
}
#include <syscalls/zephyr_shared_data_prog_mrsh.c>