/*
 * Copyright (c) 2024 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/storage/storage_area/storage_area_store.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sa_store, CONFIG_STORAGE_AREA_LOG_LEVEL);

#define SAS_MAGIC	0xF0	/* record magic */
#define SAS_HDRSIZE	4	/* magic + wrapcnt + size */
#define SAS_CRCINIT	0
#define SAS_CRCSIZE	4
#define SAS_MINBUFSIZE	32
#define SAS_FILLVALUE	0xFF

#define SAS_MIN(a, b)		  (a < b ? a : b)
#define SAS_MAX(a, b)		  (a < b ? b : a)
#define SAS_ALIGNUP(num, align) (((num) + ((align) - 1)) & ~((align) - 1))
#define SAS_ALIGNDOWN(num, align) ((num) & ~((align) - 1))

static uint32_t get_le32(const uint8_t *buf)
{
	return (uint32_t)buf[0] + ((uint32_t)buf[1] << 8) +
	       ((uint32_t)buf[2] << 16) + ((uint32_t)buf[3] << 24);
}

static uint16_t get_le16(const uint8_t *buf)
{
	return (uint16_t)buf[0] + ((uint16_t)buf[1] << 8);
}

#ifndef CONFIG_STORAGE_AREA_STORE_READONLY
static void put_le32(uint8_t *buf, uint32_t value)
{
	buf[0] = (uint8_t)(value & 0x000000ff);
	buf[1] = (uint8_t)((value & 0x0000ff00) >> 8);
	buf[2] = (uint8_t)((value & 0x00ff0000) >> 16);
	buf[3] = (uint8_t)((value & 0xff000000) >> 24);
}

static void put_le16(uint8_t *buf, uint16_t value)
{
	buf[0] = (uint8_t)(value & 0x000000ff);
	buf[1] = (uint8_t)((value & 0x0000ff00) >> 8);
}
#endif /* CONFIG_STORAGE_AREA_STORE_READONLY */

static uint32_t crc32(uint32_t crc, const void *buf, size_t len)
{
	const uint8_t *data = (const uint8_t *)buf;
	/* crc table generated from polynomial 0xedb88320 */
	static const uint32_t sas_crc32_table[16] = {
		0x00000000U, 0x1db71064U, 0x3b6e20c8U, 0x26d930acU,
		0x76dc4190U, 0x6b6b51f4U, 0x4db26158U, 0x5005713cU,
		0xedb88320U, 0xf00f9344U, 0xd6d6a3e8U, 0xcb61b38cU,
		0x9b64c2b0U, 0x86d3d2d4U, 0xa00ae278U, 0xbdbdf21cU,
	};

	crc = ~crc;

	for (size_t i = 0; i < len; i++) {
		uint8_t byte = data[i];

		crc = (crc >> 4) ^ sas_crc32_table[(crc ^ byte) & 0x0f];
		crc = (crc >> 4) ^ sas_crc32_table[(crc ^ (byte >> 4)) & 0x0f];
	}

	return (~crc);
}

static void sector_advance(const struct storage_area_store *store,
			   size_t *sector, size_t cnt)
{
	while (cnt > 0U) {
		(*sector)++;
		cnt--;
		if ((*sector) == store->sector_cnt) {
			*sector = 0U;
		}
	}
}

#if defined(CONFIG_STORAGE_AREA_STORE_SEMAPHORE)
static int store_init_semaphore(const struct storage_area_store *store)
{
	return k_sem_init(&store->data->semaphore, 1, 1);
}

static int store_take_semaphore(const struct storage_area_store *store)
{
	return k_sem_take(&store->data->semaphore, K_FOREVER);
}

static void store_give_semaphore(const struct storage_area_store *store)
{
	(void)k_sem_give(&store->data->semaphore);
}
#else
static int store_init_semaphore(const struct storage_area_store *store)
{
	return 0;
}

static int store_take_semaphore(const struct storage_area_store *store)
{
	return 0;
}

static void store_give_semaphore(const struct storage_area_store *store)
{
}
#endif /* CONFIG_STORAGE_AREA_STORE_SEMAPHORE */

static bool store_record_valid(const struct storage_area_record *record)
{
	const struct storage_area_store *store = record->store;
	const struct storage_area *area = store->area;
	const size_t off = record->sector * store->sector_size + record->loc +
			   SAS_HDRSIZE;
	uint32_t crc = SAS_CRCINIT;
	uint8_t buf[SAS_MAX(SAS_MINBUFSIZE, area->write_size)];
	struct storage_area_chunk rd = {
		.data = &buf,
	};
	size_t start = 0U;

	while (start < record->size) {
		rd.len = SAS_MIN(sizeof(buf), record->size - start);
		if (storage_area_read(area, off + start, &rd, 1U) != 0) {
			goto end;
		}

		crc = crc32(crc, buf, rd.len);
		start += rd.len;
	}

	rd.len = SAS_CRCSIZE;
	if (storage_area_read(area, off + start, &rd, 1U) != 0) {
		goto end;
	}

	if (crc != get_le32(buf)) {
		goto end;
	} 

	return true;
end:
	return false;
}

static int store_record_next_in_sector(struct storage_area_record *record,
				       bool recover)
{
	const struct storage_area_store *store = record->store;
	const struct storage_area_store_data *data = store->data;
	const struct storage_area *area = store->area;
	const size_t off = record->sector * store->sector_size;
	bool crc_ok = data->ready;
	int rc = -ENOENT;

	if ((record->loc == 0U) && (store->sector_cookie != NULL) &&
	    (store->sector_cookie_size != 0U)) {
		const size_t cksz = store->sector_cookie_size;

		record->loc = SAS_ALIGNUP(cksz, area->write_size);
	}

	while (true) {
		uint8_t buf[SAS_HDRSIZE];
		struct storage_area_chunk rd = {
			.data = &buf,
			.len = sizeof(buf),
		};

		if (record->size != 0U) {
			record->loc += (SAS_HDRSIZE + record->size + SAS_CRCSIZE);
			record->loc = SAS_ALIGNUP(record->loc, area->write_size);
		}

		if ((record->loc == store->sector_size) ||
		    ((data->ready) && (data->sector == record->sector) &&
		    (data->loc <= record->loc))) {
			record->size = 0U;
			rc = -ENOENT;
			break;
		}

		rc = storage_area_read(area, off + record->loc, &rd, 1U);
		if (rc != 0) {
			break;
		}

		size_t rsize = (size_t)get_le16(&buf[2]);
		size_t asize = store->sector_size - record->loc - SAS_CRCSIZE -
			       SAS_HDRSIZE;
		bool size_ok = ((rsize > 0U) && (rsize < asize));

		if (size_ok) {
			record->size = rsize;
			if (!crc_ok) {
				crc_ok = store_record_valid(record);
			}
		}

		if (record->sector > data->sector) {
			buf[1]++;
		}

		if (!data->ready) {
			buf[1] = data->wrapcnt;
		}

		if ((buf[0] == SAS_MAGIC) && (buf[1] == data->wrapcnt) && 
		    (size_ok) && (crc_ok)) {
			rc = 0;
			break;
		}

		if (!recover) {
			rc = -ENOENT;
			break;
		}
		
		record->size = 0U;
		record->loc += area->write_size;
		crc_ok = false;
	}

	return rc;
}

#ifdef CONFIG_STORAGE_AREA_STORE_READONLY
static int store_add_cookie(const struct storage_area_store *store)
{
	return 0;
}
#else
static int store_add_cookie(const struct storage_area_store *store)
{
	if ((store->data->loc != 0) || (store->sector_cookie == NULL) ||
	    (store->sector_cookie_size == 0U)) {
		return 0;
	}

	const size_t wroff = store->data->sector * store->sector_size;
	const size_t cksize = store->sector_cookie_size;
	uint8_t fill[SAS_ALIGNUP(cksize, store->area->write_size) - cksize];
	struct storage_area_chunk wr[2] = {
		{
			.data = store->sector_cookie,
			.len = cksize,
		},{
			.data = fill,
			.len = sizeof(fill),
		},
	};
	int rc;

	memset(fill, SAS_FILLVALUE, sizeof(fill));
	rc = storage_area_prog(store->area, wroff, wr, 2U);
	if (rc != 0) {
		goto end;
	}
		
	store->data->loc = cksize + sizeof(fill);
end:
	return rc;
}
#endif /* CONFIG_STORAGE_AREA_STORE_READONLY */

static int store_get_sector_cookie(const struct storage_area_store *store,
				   uint16_t sector, void *cookie,
				   size_t cksz)
{
	const size_t off = sector * store->sector_size;
	struct storage_area_chunk rd = {
		.data = cookie,
		.len = SAS_MIN(cksz, store->sector_cookie_size),
	};

	return storage_area_read(store->area, off, &rd, 1U);
}

static int store_fill_sector(const struct storage_area_store *store)
{
	struct storage_area_store_data *data = store->data;
	const struct storage_area *area = store->area;
	const size_t wroff = data->sector * store->sector_size;
	uint8_t buf[SAS_MAX(SAS_MINBUFSIZE, area->write_size)];
	struct storage_area_chunk wr = {
		.data = buf,
	};
	int rc = 0;

	memset(buf, SAS_FILLVALUE, sizeof(buf));
	while (data->loc < store->sector_size) {
		wr.len = SAS_MIN(sizeof(buf), store->sector_size - data->loc);
		rc = storage_area_prog(area, wroff + data->loc, &wr, 1U);
		if (rc != 0) {
			break;
		}

		data->loc += wr.len;
	}

	return rc;
}

static int store_erase_block(const struct storage_area_store *store)
{
	const struct storage_area *area = store->area;
	struct storage_area_store_data *data = store->data;
	int rc = 0;

	if ((data->sector * store->sector_size) % area->erase_size != 0U) {
		goto end;
	}

	size_t block = (data->sector * store->sector_size) / area->erase_size;
	rc = storage_area_erase(area, block, 1U);
end:
	return rc;
}

static int store_advance(const struct storage_area_store *store)
{
	const struct storage_area *area = store->area;
	struct storage_area_store_data *data = store->data;
	int rc = 0;

	if (STORAGE_AREA_OVRWRITE(area)) {
		rc = store_fill_sector(store);
		if (rc != 0) {
			goto end;
		}
	}

	sector_advance(store, &data->sector, 1U);
	if (data->sector == 0U) {
		data->wrapcnt++;
	}
	data->loc = 0U;

	if ((!IS_ENABLED(CONFIG_STORAGE_AREA_STORE_READONLY)) && 
	    (!STORAGE_AREA_OVRWRITE(area))) {
		rc = store_erase_block(store);
		if (rc != 0) {
			goto end;
		} 
	}

	rc = store_add_cookie(store);
end:
	return rc;
}

#ifndef CONFIG_STORAGE_AREA_STORE_DISABLECOMPACT
static int store_move_record(struct storage_area_record *record)
{
	if ((!record->store->move(record)) || (!store_record_valid(record))) {
		return 0;
	}

	struct storage_area_store *store = record->store;
	struct storage_area_store_data *data = store->data;
	const struct storage_area *area = store->area;
	const struct storage_area_record dest = {
		.store = store,
		.sector = data->sector,
		.loc = data->loc,
		.size = record->size
	};
	const size_t rdoff = record->sector * store->sector_size;
	const size_t wroff = data->sector * store->sector_size + data->loc;
	const size_t alsize = SAS_ALIGNUP(
		SAS_HDRSIZE + record->size + SAS_CRCSIZE, area->write_size
	);
	uint8_t buf[SAS_MAX(SAS_MINBUFSIZE, area->write_size)];
	struct storage_area_chunk rdwr = {
		.data = buf,
	};
	size_t start = 0U;
	int rc = 0;

	if ((store->sector_size - alsize) < data->loc) {
		rc = -ENOSPC;
		goto end;
	}

	while (start < alsize) {
		rdwr.len = SAS_MIN(sizeof(buf), alsize - start);
		rc = storage_area_read(area, rdoff + start, &rdwr, 1U);
		if (rc != 0) {
			goto end;
		}

		if (start == 0U) {
			buf[1] = data->wrapcnt;
		}

		rc = storage_area_prog(area, wroff + start, &rdwr, 1U);
		if (rc != 0) {
			goto end;
		}

		data->loc += rdwr.len;
		start += rdwr.len;
	}

	if (record->store->move_cb != NULL) {
		record->store->move_cb(record, &dest);
	}

end:
	return rc;
}

static int store_compact(const struct storage_area_store *store)
{
	int rc = store_advance(store);

	if ((store->move == NULL) || (rc != 0)) {
		goto end;
	}

	const struct storage_area *area = store->area;
	const struct storage_area_store_data *data = store->data;

	if ((data->sector * store->sector_size) % area->erase_size != 0U) {
		goto end;
	}

	size_t scnt = area->erase_size / store->sector_size;
	struct storage_area_record walk = {
		.store = (struct storage_area_store *)store,
		.sector = data->sector,
	};

	sector_advance(store, &walk.sector, store->spare_sectors);
		
	while (scnt > 0U) {
		walk.loc = 0U;
		walk.size = 0U;
		while (store_record_next_in_sector(&walk, true) == 0) {
			while (true) {
				rc = store_move_record(&walk);
				if ((rc == 0) || (rc != -ENOSPC)) {
					break;
				}

				rc = store_advance(store);
				if (rc != 0) {
					break;
				}
			}

			if (rc != 0) {
				break;
			}
		}

		sector_advance(store, &walk.sector, 1U);
		scnt--;
	}

end:
	return rc;
}
#endif /* CONFIG_STORAGE_AREA_STORE_DISABLECOMPACT */

#ifdef CONFIG_STORAGE_AREA_STORE_DISABLECOMPACT
static bool store_recovery(const struct storage_area_store *store)
{
	return 0;
}
#else
static void sector_reverse(const struct storage_area_store *store,
			   size_t *sector, size_t cnt)
{
	while (cnt > 0U) {
		if (*sector == 0U) {
			*sector = store->sector_cnt;
		}

		(*sector)--;
		cnt--;
	}
}

static bool store_recovery(const struct storage_area_store *store)
{
	if (store->move == NULL) {
		return false;
	}

	const struct storage_area *area = store->area;
	struct storage_area_store_data *data = store->data;
	struct storage_area_record walk = {
		.store = (struct storage_area_store *)store,
		.sector = data->sector,
	};
	size_t scnt = area->erase_size / store->sector_size;
	int rc = 0;
	bool needs_recovery = false;

	while (((walk.sector * store->sector_size) % area->erase_size) != 0U) {
		sector_reverse(store, &walk.sector, 1U);
	}

	while ((scnt > 0U) && (!needs_recovery)) {
		walk.loc = 0U;
		walk.size = 0U;
		while (store_record_next_in_sector(&walk, true) == 0) {
			if ((store->move(&walk)) &&
			    (store_record_valid(&walk))) {
				needs_recovery = true;
				break;
			}
		}

		sector_advance(store, &walk.sector, 1U);
		scnt--;
	}

	if (!needs_recovery) {
		goto end;
	}

	while ((data->sector * store->sector_size) % area->erase_size != 0U) {
		sector_reverse(store, &data->sector, 1U);
	}

	data->loc = store->sector_size;
	sector_reverse(store, &data->sector, 1U);
	rc = store_compact(store);
end:
	return rc;
}
#endif /* CONFIG_STORAGE_AREA_STORE_DISABLECOMPACT */

static size_t store_chunk_size(const struct storage_area_chunk *ch, size_t cnt)
{
	size_t rv = 0U;

	for (size_t i = 0U; i < cnt; i++) {
		rv += ch[i].len;
	}

	return rv;
}

#ifdef CONFIG_STORAGE_AREA_STORE_READONLY
static int store_write(const struct storage_area_store *store,
		       const struct storage_area_chunk *ch, size_t cnt)
{
	return -ENOTSUP;
}
#else
static int store_write(const struct storage_area_store *store,
		       const struct storage_area_chunk *ch, size_t cnt)
{
	struct storage_area_store_data *data = store->data;
	const size_t len = SAS_HDRSIZE + store_chunk_size(ch, cnt) +
			   SAS_CRCSIZE;
	
	if ((store->sector_size - len) < data->loc) {
		return -ENOSPC;
	}

	const struct storage_area *area = store->area;
	const size_t wroff = data->sector * store->sector_size;
	struct storage_area_chunk wr[cnt + 2];
	uint8_t hbuf[SAS_HDRSIZE];
	uint8_t cbuf[SAS_CRCSIZE + SAS_ALIGNUP(len, area->write_size) - len];
	uint32_t crc = SAS_CRCINIT;
	int rc;

	hbuf[0] = SAS_MAGIC;
	hbuf[1] = data->wrapcnt;
	put_le16(&hbuf[2], (uint16_t)store_chunk_size(ch, cnt));
	wr[0].data = hbuf;
	wr[0].len = sizeof(hbuf);
	wr[cnt + 1].data = cbuf;
	wr[cnt + 1].len = sizeof(cbuf);

	for (size_t i = 1; i < (cnt + 1); i++) {
		wr[i].data = ch[i - 1].data;
		wr[i].len = ch[i - 1].len;
		crc = crc32(crc, wr[i].data, wr[i].len);
	}

	put_le32(cbuf, crc);
	memset(&cbuf[SAS_CRCSIZE], SAS_FILLVALUE, sizeof(cbuf) -SAS_CRCSIZE);
	while (true) {
		rc = storage_area_prog(area, wroff + data->loc, wr, cnt + 2);
		if (rc == 0) {
			data->loc += SAS_ALIGNUP(len, area->write_size);
			break;
		}

		data->loc += area->write_size;
		if ((store->sector_size - len) < data->loc) {
			rc = -ENOSPC;
			break;
		}
	}

	return rc;
}
#endif /* CONFIG_STORAGE_AREA_STORE_READONLY */

static bool store_valid(const struct storage_area_store *store)
{
	if ((store == NULL) || (store->data == NULL) || (store->area == NULL)) {
		return false;
	}

	return true;
}

static bool store_ready(const struct storage_area_store *store)
{
	if ((!store_valid(store)) || (!store->data->ready)) {
		return false;
	}

	return true;
}

int storage_area_store_advance(const struct storage_area_store *store)
{
	if (!store_ready(store)) {
		return -EINVAL;
	}

	int rc;

	(void)store_take_semaphore(store);
	rc = store_advance(store);
	store_give_semaphore(store);
	return rc;
}

#ifdef CONFIG_STORAGE_AREA_STORE_DISABLECOMPACT
int storage_area_store_compact(const struct storage_area_store *store)
{
	return storage_area_store_advance(store);
}
#else
int storage_area_store_compact(const struct storage_area_store *store)
{
	if (!store_ready(store)) {
		return -EINVAL;
	}

	int rc;

	(void)store_take_semaphore(store);
	rc = store_compact(store);
	store_give_semaphore(store);
	return rc;
}
#endif /* CONFIG_STORAGE_AREA_STORE_DISABLECOMPACT */

bool storage_area_record_valid(const struct storage_area_record *record)
{
	if (!store_ready(record->store)) {
		return -EINVAL;
	}

	return store_record_valid(record);
}

int storage_area_store_write(const struct storage_area_store *store,
			     const struct storage_area_chunk *ch, size_t cnt)
{
	if (!store_ready(store)) {
		return -EINVAL;
	}

	int rc;

	(void)store_take_semaphore(store);
	rc = store_write(store, ch, cnt);
	store_give_semaphore(store);
	return rc;
}

int storage_area_store_dwrite(const struct storage_area_store *store,
			      const void *data, size_t len)
{
	const struct storage_area_chunk ch = {
		.data = (void *)data,
		.len = len,
	};

	return storage_area_store_write(store, &ch, 1U);
}

int storage_area_record_next(const struct storage_area_store *store,
			     struct storage_area_record *record)
{
	if (!store_ready(store)) {
		return -EINVAL;
	}

	if (record->store == NULL) {
		record->store = (struct storage_area_store *)store;
		record->loc = 0U;
		record->size = 0U;
		record->sector = store->data->sector;
		sector_advance(store, &record->sector, store->spare_sectors);
	}

	int rc = 0;

	while (true) {
		rc = store_record_next_in_sector(record, true);
		if (rc != -ENOENT) {
			break;
		}
		
		if (record->sector == store->data->sector) {
			break;
		}

		sector_advance(store, &record->sector, 1U);
		record->loc = 0U;
		record->size = 0U;
	}

	return rc;
}

int storage_area_record_read(const struct storage_area_record *record,
			     size_t start, const struct storage_area_chunk *ch,
			     size_t cnt)
{
	if ((record == NULL) || (record->store == NULL) ||
	    (!store_ready(record->store)) ||
	    (record->loc > record->store->sector_size) ||
	    (record->size > record->store->sector_size) ||
	    (record->size < store_chunk_size(ch, cnt))) {
		return -EINVAL;
	}

	const struct storage_area_store *store = record->store;
	const struct storage_area *area = store->area;
	const size_t rdoff = record->sector * store->sector_size + record->loc;
	return storage_area_read(area, start + rdoff + SAS_HDRSIZE, ch, cnt);
}

int storage_area_record_dread(const struct storage_area_record *record,
			      size_t start, void *data, size_t len)
{
	const struct storage_area_chunk ch = {
		.data = data,
		.len = SAS_MIN(len, record->size),
	};

	return storage_area_record_read(record, start, &ch, 1U);
}

int storage_area_store_get_sector_cookie(const struct storage_area_store *store,
					 size_t sector, void *cookie,
					 size_t cksz)
{
	if ((!store_valid(store)) || (store->sector_cookie == NULL) ||
	    (store->sector_cookie_size == 0)) {
		return -EINVAL;
	}

	return store_get_sector_cookie(store, sector, cookie, cksz);
}

int storage_area_store_mount(const struct storage_area_store *store)
{
	if ((!store_valid(store)) || (store_init_semaphore(store) != 0)) {
		return -EINVAL;
	}

	struct storage_area_store_data *data = store->data;
	const struct storage_area *area = store->area;
	const size_t sa_size = area->erase_size * area->erase_blocks;
	const size_t st_size = store->sector_size * store->sector_cnt;
	
	if (((store->sector_size & (store->sector_size - 1)) != 0U) ||
	    ((store->sector_size & (area->write_size - 1)) != 0U) ||
	    ((area->erase_size & (store->sector_size - 1)) != 0U) ||
	    ((store->move != NULL) && 
	     (store->spare_sectors < (area->erase_size / store->sector_size))) ||
	    (sa_size < st_size)) {
		return -EINVAL;
	}

	if (data->ready) {
		return -EALREADY;
	}

	(void)store_take_semaphore(store);
	struct storage_area_record record = {
		.store = (struct storage_area_store *)store,
	};
	uint8_t rd_wrapcnt;
	struct storage_area_chunk rd = {
		.data = &rd_wrapcnt,
		.len = 1U,
	};
	int rc = 0;

	data->sector = store->sector_cnt;
	data->loc = store->sector_size;

	for (size_t i = 0U; i < store->sector_cnt; i++) {
		record.sector = i;
		record.loc = 0U;
		
		if (store_record_next_in_sector(&record, false) != 0) {
			continue;
		}

		const size_t rdoff = i * store->sector_size + record.loc + 1U;
		if (storage_area_read(area, rdoff, &rd, 1U) != 0) {
			continue;
		}

		if (data->sector > i) {
			data->wrapcnt = rd_wrapcnt;
		}

		if (rd_wrapcnt != data->wrapcnt) {
			break;
		}

		data->sector = i;
	}

	if (data->sector == store->sector_cnt) {
		data->sector--;
		rc = store_advance(store);
		goto end;
	}

	size_t loc = 0U;

	record.sector = data->sector;
	record.loc = 0U;
	record.size = 0U;
	while (store_record_next_in_sector(&record, false) == 0) {
		loc = record.loc + SAS_ALIGNUP(SAS_HDRSIZE + record.size +
					       SAS_CRCSIZE, area->write_size);
	}

	data->loc = loc;

	rc = store_recovery(store);
end:
	if (rc == 0) {
		store->data->ready = true;
	}

	store_give_semaphore(store);
	return rc;
}

int storage_area_store_unmount(const struct storage_area_store *store)
{
	if (!store_valid(store)) {
		return -EINVAL;
	}

	if (store->data->ready) {
		store->data->ready = false;
	}

	return 0;
}

int storage_area_store_wipe(const struct storage_area_store *store)
{
	if (!store_valid(store)) {
		return -EINVAL;
	}

	if (store->data->ready) {
		return -EINVAL;
	}

	return storage_area_erase(store->area, 0, store->area->erase_blocks);
}