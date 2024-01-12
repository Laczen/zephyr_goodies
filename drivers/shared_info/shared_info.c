/*
 * Copyright (c) 2024, Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT zephyr_shared_info

#include <string.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/shared_info.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <zephyr/linker/devicetree_regions.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(shared_info, CONFIG_SHARED_INFO_LOG_LEVEL);

struct shared_info_config {
	void *start;
	size_t size;
};

static int size(const struct device *dev, size_t *size)
{
	const struct shared_info_config *config = dev->config;

	*size = config->size;
	return 0;
}

static int read(const struct device *dev, size_t off, void *data, size_t len)
{
	const struct shared_info_config *config = dev->config;

	if ((config->size < len) || ((config->size - len) < off)) {
		return -EINVAL;
	}

	memcpy(data, ((uint8_t *)config->start + off), len);
	return 0;
}

static int prog(const struct device *dev, size_t off, const void *data,
		size_t len)
{
	const struct shared_info_config *config = dev->config;

	if ((config->size < len) || ((config->size - len) < off)) {
		return -EINVAL;
	}

	memcpy(((uint8_t *)config->start + off), data, len);
	return 0;
}

static const struct shared_info_driver_api shared_info_api = {
	.size = size,
	.read = read,
	.prog = prog,
};

#define MREGION_PHANDLE(n) DT_PHANDLE_BY_IDX(DT_DRV_INST(n), memory_region, 0)
#define MREGION_SIZE(n)    DT_REG_SIZE(MREGION_PHANDLE(n))
#define MREGION_NAME(n)    LINKER_DT_NODE_REGION_NAME(MREGION_PHANDLE(n))
#define MREGION_SECTION(n) Z_GENERIC_SECTION(MREGION_NAME(n))

#define SHARED_INFO_DEVICE(inst)                                                \
	static uint8_t sidata_##inst[MREGION_SIZE(inst)] MREGION_SECTION(inst); \
	static const struct shared_info_config shared_info_config_##inst = {    \
		.start = (void *)&sidata_##inst,                                \
		.size = MREGION_SIZE(inst),                                     \
	};                                                                      \
	DEVICE_DT_INST_DEFINE(inst, NULL, NULL, NULL,                           \
			      &shared_info_config_##inst, POST_KERNEL,          \
			      CONFIG_SHARED_INFO_INIT_PRIORITY,                 \
			      &shared_info_api);

DT_INST_FOREACH_STATUS_OKAY(SHARED_INFO_DEVICE)