/*
 * Copyright (c) 2024, Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/shared_info.h>
#include <zephyr/internal/syscall_handler.h>

static inline ssize_t z_vrfy_shared_info_size(const struct device *dev,
					      size_t *size)
{
	K_OOPS(K_SYSCALL_OBJ(dev, K_OBJ_DRIVER_SHARED_INFO));
	K_OOPS(K_SYSCALL_MEMORY_WRITE(size, sizeof(size_t)));
	return z_impl_shared_info_size(dev, size);
}
#include <syscalls/shared_info_size_mrsh.c>

static inline int z_vrfy_shared_info_read(const struct device *dev, size_t off,
					  void *data, size_t len)
{
	K_OOPS(K_SYSCALL_OBJ(dev, K_OBJ_DRIVER_SHARED_INFO));
	K_OOPS(K_SYSCALL_MEMORY_WRITE(data, len));
	return z_impl_shared_info_read(dev, off, data, len);
}
#include <syscalls/shared_info_read_mrsh.c>

static inline int z_vrfy_shared_info_prog(const struct device *dev, size_t off,
					  const void *data, size_t len)
{
	K_OOPS(K_SYSCALL_OBJ(dev, K_OBJ_DRIVER_SHARED_INFO));
	K_OOPS(K_SYSCALL_MEMORY_READ(data, len));
	return z_impl_shared_info_prog(dev, off, data, len);
}
#include <syscalls/shared_info_prog_mrsh.c>