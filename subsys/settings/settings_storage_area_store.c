/*
 * Copyright (c) 2024 Laczen
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <string.h>
#include <zephyr/settings/settings.h>
#include <zephyr/settings/settings_storage_area_store.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(settings_storage_area_store, CONFIG_SETTINGS_LOG_LEVEL);

#define SASS_VALUE_BUF_SIZE	32

struct settings_sas_read_fn_arg {
	struct storage_area_record *record;
	size_t dstart;
};

static ssize_t settings_sas_read_fn(void *back_end, void *data, size_t len)
{
	struct settings_sas_read_fn_arg *rd_fn_arg =
		(struct settings_sas_read_fn_arg *)back_end;
	ssize_t rc;

	rc = storage_area_record_read(rd_fn_arg->record, rd_fn_arg->dstart,
				      data, len);
	return rc == 0 ? (ssize_t)len : (ssize_t)rc;
}

static uint8_t sas_get_name_size(const struct storage_area_record *record)
{
	uint8_t rv = 0;

	if (storage_area_record_read(record, 0U, &rv, sizeof(rv)) != 0) {
		return 0;
	}

	return rv;
}

static int sas_get_name(const struct storage_area_record *record, char *name,
			size_t nsz)
{
	return storage_area_record_read(record, 1U, name, nsz);
}

static bool settings_sas_skip(const struct storage_area_record *record,
			      const struct settings_load_arg *arg)
{
	size_t slen = ((arg == NULL) || (arg->subtree == NULL)) ? 
		      0U : strlen(arg->subtree);

	if ((sas_get_name_size(record) == 0U) ||
	    (sas_get_name_size(record) < slen)) {
		return true;
	}
	
	char name[sas_get_name_size(record)];

	if (sas_get_name(record, name, sizeof(name)) != 0) {
		return true;
	}

	if ((slen != 0U) && (memcmp(name, arg->subtree, slen) != 0)) {
		return true;
	}

	struct storage_area_record walk = {
		.store = record->store,
		.sector = record->sector,
		.loc = record->loc,
		.size = record->size,
	};
	bool rv = false;

	while (storage_area_record_next(record->store, &walk) == 0) {
		if (sas_get_name_size(&walk) != sizeof(name)) {
			continue;
		}

		char wname[sizeof(name)];

		if (sas_get_name(&walk, wname, sizeof(name)) != 0) {
			continue;
		}

		if ((memcmp(name, wname, sizeof(name)) == 0) && 
		    (storage_area_record_valid(&walk))) {
			rv = true;
			break;
		}
	}

	if ((!rv) && (!storage_area_record_valid(record))) {
		rv = true;
	}

	return rv;
}

static bool settings_sas_move(const struct storage_area_record *record)
{
	if (settings_sas_skip(record, NULL)) {
		return false;
	}

	if ((sas_get_name_size(record) + 1U) == record->size) {
		return false;
	}

	return true;
}

static int settings_sas_init(const struct storage_area_store *store)
{
	if (store->data->ready) {
		return 0;
	}

	const struct storage_area_store_compact_cb cb = {
		.move = settings_sas_move,
	};
	int rc;

	rc = storage_area_store_mount(store, &cb);
	if (rc != 0) {
		LOG_DBG("mount failed");
	}

	return rc;
}

static int settings_sas_load(struct settings_store *store,
			     const struct settings_load_arg *arg)
{
	const struct settings_storage_area_store *ssas =
		CONTAINER_OF(store, struct settings_storage_area_store, store);
	const struct storage_area_store *sa_store = ssas->sa_store;

	if (settings_sas_init(sa_store) != 0) {
		/* allow other backends to be processed */
		return 0;
	}

	struct settings_sas_read_fn_arg read_fn_arg;
	struct storage_area_record record = {
		.store = NULL,
	};
	int rc = 0;

	while (storage_area_record_next(sa_store, &record) == 0) {
		if (settings_sas_skip(&record, arg)) {
			continue;
		}

		char name[sas_get_name_size(&record) + 1U];
		size_t dsize;

		rc = sas_get_name(&record, name, sizeof(name) - 1);
		if (rc != 0) {
			break;
		}

		dsize = record.size - sizeof(name);
		if (dsize == 0U) {
			continue;
		}

		name[sizeof(name) - 1] = '\0';
		read_fn_arg.record = &record;
		read_fn_arg.dstart = sizeof(name);
		rc = settings_call_set_handler(name, dsize, settings_sas_read_fn,
					       &read_fn_arg, (void *)arg);
		if (rc != 0) {
			break;
		}
	}

	return rc;
}

static bool settings_sas_duplicate(const struct storage_area_store *sa_store,
				   const char *name, const void *value,
				   size_t val_len)
{
	const struct settings_load_arg load_arg = {
		.subtree = name,
	};
	struct storage_area_record record = {
		.store = NULL,
	};
	size_t dstart = 1U + strlen(name);
	uint8_t *value8 = (uint8_t *)value;
	uint8_t buf[SASS_VALUE_BUF_SIZE];
	bool rv = false;

	while (storage_area_record_next(sa_store, &record) == 0) {
		if (settings_sas_skip(&record, &load_arg)) {
			continue;
		}

		break;
	}

	if (val_len != (record.size - dstart)) {
		goto end;
	}

	while (dstart < record.size) {
		size_t rdsz = MIN(sizeof(buf), record.size - dstart);

		if (storage_area_record_read(&record, dstart, buf, rdsz) != 0) {
			break;
		}

		if (memcmp(value8, buf, rdsz) != 0) {
			break;
		}

		dstart += rdsz;
		value8 += rdsz;
	}

	if (dstart == record.size) {
		rv = true;
	}
end:
	return rv;
}

static int settings_sas_save(struct settings_store *store, const char *name,
			     const char *value, size_t val_len)
{
	const struct settings_storage_area_store *ssas =
		CONTAINER_OF(store, struct settings_storage_area_store, store);
	const struct storage_area_store *sa_store = ssas->sa_store;
	
	if ((name == NULL) || (settings_sas_init(sa_store) != 0)) {
		return -EINVAL;
	}

	val_len = (value == NULL) ? 0 : val_len;
	if (settings_sas_duplicate(sa_store, name, value, val_len)) {
		return 0;
	}
	
	uint8_t nsz = strlen(name);
	struct storage_area_iovec wr[] = {
		{
			.data = (void *)&nsz,
			.len = sizeof(nsz),
		}, {
			.data = (void *)name,
			.len = nsz,
		}, {
			.data = (void *)(val_len == 0U ? NULL : value),
			.len = val_len,
		},
	};
	int rc = 0;

	for (size_t i = 0; i < sa_store->sector_cnt; i++) {
		rc = storage_area_store_writev(sa_store, wr, ARRAY_SIZE(wr));
		if ((rc == 0) || (rc != -ENOSPC)) {
			break;
		}

		rc = storage_area_store_compact(sa_store);
		if (rc != 0) {
			break;
		}

		rc = -ENOSPC;
	}

	return rc;
}

static void *settings_sas_storage_get(struct settings_store *store)
{
	struct settings_storage_area_store *sass =
		CONTAINER_OF(store, struct settings_storage_area_store, store);
	const struct storage_area_store *sa_store = sass->sa_store;

	return (void *)sa_store;
}

const struct settings_store_itf settings_storage_area_store_itf = {
	.csi_load = settings_sas_load,
	.csi_save = settings_sas_save,
	.csi_storage_get = settings_sas_storage_get
};