/*
 * Copyright (c) 2024, Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT zephyr_shared_data

#include <string.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/zephyr_shared_data.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(zephyr_shared_data, CONFIG_ZEPHYR_SHARED_DATA_LOG_LEVEL);

struct shared_data_config {
	void *start;
	size_t size;
};

static int shared_data_size(const struct device *dev, size_t *size)
{
	const struct shared_data_config *config = dev->config;

	*size = config->size;
	return 0;
}

static int shared_data_read(const struct device *dev, size_t off, void *data,
			    size_t len)
{
	const struct shared_data_config *config = dev->config;

	memcpy(data, (config->start + off), len);
	return 0;
}

static int shared_data_prog(const struct device *dev, size_t off,
			    const void *data, size_t len)
{
	const struct shared_data_config *config = dev->config;

	memcpy((config->start + offset), buffer, size);
	return 0;
}

static const struct zephyr_shared_data_api zephyr_shared_data_api = {
	.size = shared_data_size,
	.read = shared_data_read,
	.prog = shared_data_prog,
};

#define MREGION_PHANDLE(n) DT_PHANDLE_BY_IDX(DT_DRV_INST(n), memory_region, 0)

#define ZEPHYR_SHARED_DATA_DEVICE(inst)                                         \
	static const struct shared_data_config                                  \
		zephyr_shared_data_config_##inst = {                            \
			.start = (void *)DT_REG_ADDR(MREGION_PHANDLE(inst)),    \
			.size = DT_REG_SIZE(MREGION_PHANDLE(inst)),             \
	};                                                                      \
	DEVICE_DT_INST_DEFINE(inst, NULL, NULL, NULL,                           \
			      &zephyr_shared_data_config_##inst, POST_KERNEL,   \
			      CONFIG_RETAINED_MEM_INIT_PRIORITY,                \
			      &zephyr_shared_data_api);

DT_INST_FOREACH_STATUS_OKAY(ZEPHYR_SHARED_DATA_DEVICE)