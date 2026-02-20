/*
 * UBAC:ntc_history.c Firmware for ESP32 to monitor NTC sensors and control a flash-based
 * ring buffer for history.
 * Copyright (C) 2026 Côme VINCENT
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "ntc_history.h"

#include "esp_log.h"
#include "esp_partition.h"
#include "esp_rom_crc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <inttypes.h>
#include <math.h>
#include <string.h>
#include <time.h>

#define STORAGE_PARTITION_LABEL   "storage"
#define STORAGE_PARTITION_SUBTYPE 0x99

#define SECTOR_SIZE     4096u
#define SECTOR_HDR_SIZE 64u

// On-flash record layout is 32 bytes:
// seq(4) + timestamp(4) + temps(20) + crc(4)
#define RECORD_SIZE 32u

#define RECORDS_PER_SECTOR ((SECTOR_SIZE - SECTOR_HDR_SIZE) / RECORD_SIZE)
#define RAM_BUFFER_RECORDS 16

#define SECTOR_MAGIC   0x53454354u   // 'SECT'
#define FORMAT_VERSION 3u            // bumped: removed record magic

static const char *TAG = "NTC_HISTORY";

typedef struct __attribute__((packed))
{
  uint32_t magic;       // SECTOR_MAGIC (written last)
  uint32_t version;     // FORMAT_VERSION
  uint32_t seq_start;   // sequence number for first record in sector
  uint32_t hdr_crc32;   // CRC32 over version+seq_start
  uint8_t pad[SECTOR_HDR_SIZE - 16];
} sector_hdr_t;

_Static_assert(sizeof(sector_hdr_t) == SECTOR_HDR_SIZE, "sector_hdr_t size");

typedef struct __attribute__((packed))
{
  uint32_t seq;         // monotonic
  uint32_t timestamp;   // unix seconds
  int16_t temps_cC[NTC_CHANNELS_COUNT];
  uint32_t rec_crc32;   // CRC over seq+timestamp+temps_cC bytes
} record_flash_t;

_Static_assert(sizeof(record_flash_t) == RECORD_SIZE, "record_flash_t size");

typedef struct
{
  uint32_t timestamp;
  int16_t temps_cC[NTC_CHANNELS_COUNT];
} record_ram_t;

typedef struct
{
  uint32_t sector_idx;
  uint32_t seq_start;
} sector_info_t;

static const esp_partition_t *s_part = NULL;
static SemaphoreHandle_t s_lock = NULL;

static uint32_t s_sector_count = 0;
static uint32_t s_cur_sector = 0;
static uint32_t s_cur_slot = 0;
static uint32_t s_last_seq = 0;
static bool s_ready = false;

static record_ram_t s_ram_buf[RAM_BUFFER_RECORDS];
static size_t s_ram_count = 0;

static uint32_t crc32_le(const void *data, size_t len)
{
  return esp_rom_crc32_le(0, (const uint8_t *) data, len);
}

static uint32_t sector_offset(uint32_t sector_idx)
{
  return sector_idx * SECTOR_SIZE;
}

static uint32_t record_offset(uint32_t sector_idx, uint32_t slot_idx)
{
  return sector_offset(sector_idx) + SECTOR_HDR_SIZE + (slot_idx * RECORD_SIZE);
}

static int16_t float_to_cC(float temp_c)
{
  if (isnan(temp_c) || isinf(temp_c))
  {
    return INT16_MIN;   // sentinel for invalid
  }

  long v = lroundf(temp_c * (float) NTC_TEMP_SCALE);
  if (v > INT16_MAX)
  {
    return INT16_MAX;
  }
  if (v < INT16_MIN + 1)
  {
    // reserve INT16_MIN for invalid sentinel
    return INT16_MIN + 1;
  }
  return (int16_t) v;
}

static bool read_sector_hdr(uint32_t sector_idx, sector_hdr_t *out_hdr)
{
  if (esp_partition_read(s_part, sector_offset(sector_idx), out_hdr,
                         sizeof(*out_hdr)) != ESP_OK)
  {
    return false;
  }

  if (out_hdr->magic != SECTOR_MAGIC || out_hdr->version != FORMAT_VERSION)
  {
    return false;
  }

  uint32_t expected = crc32_le(&out_hdr->version, 8);
  return expected == out_hdr->hdr_crc32;
}

static bool read_record(uint32_t sector_idx, uint32_t slot_idx,
                        record_flash_t *out_rec)
{
  return esp_partition_read(s_part, record_offset(sector_idx, slot_idx), out_rec,
                            sizeof(*out_rec)) == ESP_OK;
}

static bool record_is_empty(const record_flash_t *r)
{
  return r->seq == 0xFFFFFFFFu;
}

static bool record_is_valid(const record_flash_t *r)
{
  if (r->seq == 0 || r->seq == 0xFFFFFFFFu)
  {
    return false;
  }

  uint32_t expected =
      crc32_le(&r->seq, 4 + 4 + (2 * NTC_CHANNELS_COUNT));   // seq+ts+temps
  return expected == r->rec_crc32;
}

static esp_err_t write_sector_hdr(uint32_t sector_idx, uint32_t seq_start)
{
  sector_hdr_t hdr;
  memset(&hdr, 0xFF, sizeof(hdr));

  hdr.magic = SECTOR_MAGIC;
  hdr.version = FORMAT_VERSION;
  hdr.seq_start = seq_start;
  hdr.hdr_crc32 = crc32_le(&hdr.version, 8);

  // Two-phase: write everything except magic, then magic last.
  esp_err_t err = esp_partition_write(
      s_part, sector_offset(sector_idx) + 4, ((uint8_t *) &hdr) + 4,
      sizeof(hdr) - 4);
  if (err != ESP_OK)
  {
    return err;
  }

  return esp_partition_write(s_part, sector_offset(sector_idx), &hdr.magic, 4);
}

static size_t collect_sectors(sector_info_t *out, size_t max)
{
  size_t n = 0;
  for (uint32_t i = 0; i < s_sector_count && n < max; i++)
  {
    sector_hdr_t hdr;
    if (!read_sector_hdr(i, &hdr))
    {
      continue;
    }
    out[n++] = (sector_info_t) {.sector_idx = i, .seq_start = hdr.seq_start};
  }
  return n;
}

static void sort_sectors_by_seq_start(sector_info_t *s, size_t n)
{
  for (size_t i = 0; i < n; i++)
  {
    size_t best = i;
    for (size_t j = i + 1; j < n; j++)
    {
      if (s[j].seq_start < s[best].seq_start)
      {
        best = j;
      }
    }
    if (best != i)
    {
      sector_info_t tmp = s[i];
      s[i] = s[best];
      s[best] = tmp;
    }
  }
}

static esp_err_t advance_sector(void)
{
  uint32_t next = (s_cur_sector + 1) % s_sector_count;

  esp_err_t err =
      esp_partition_erase_range(s_part, sector_offset(next), SECTOR_SIZE);
  if (err != ESP_OK)
  {
    return err;
  }

  err = write_sector_hdr(next, s_last_seq + 1);
  if (err != ESP_OK)
  {
    return err;
  }

  s_cur_sector = next;
  s_cur_slot = 0;
  return ESP_OK;
}

static void scan_current_sector_tail(void)
{
  for (uint32_t slot = 0; slot < RECORDS_PER_SECTOR; slot++)
  {
    record_flash_t r;
    if (!read_record(s_cur_sector, slot, &r))
    {
      s_cur_slot = RECORDS_PER_SECTOR;
      return;
    }

    if (record_is_empty(&r))
    {
      s_cur_slot = slot;
      return;
    }

    if (!record_is_valid(&r))
    {
      s_cur_slot = RECORDS_PER_SECTOR;
      return;
    }

    if (r.seq > s_last_seq)
    {
      s_last_seq = r.seq;
    }
  }

  s_cur_slot = RECORDS_PER_SECTOR;
}

static esp_err_t write_one_record(uint32_t timestamp,
                                  const int16_t temps_cC[NTC_CHANNELS_COUNT])
{
  if (s_cur_slot >= RECORDS_PER_SECTOR)
  {
    esp_err_t err = advance_sector();
    if (err != ESP_OK)
    {
      return err;
    }
  }

  record_flash_t rec;
  memset(&rec, 0xFF, sizeof(rec));

  rec.seq = s_last_seq + 1;
  rec.timestamp = timestamp;
  memcpy(rec.temps_cC, temps_cC, sizeof(rec.temps_cC));
  rec.rec_crc32 =
      crc32_le(&rec.seq, 4 + 4 + (2 * NTC_CHANNELS_COUNT));   // seq+ts+temps

  uint32_t off = record_offset(s_cur_sector, s_cur_slot);

  // Two-phase commit: payload (timestamp+temps+crc) first, seq last.
  esp_err_t err = esp_partition_write(
      s_part, off + 4, ((uint8_t *) &rec) + 4, sizeof(rec) - 4);
  if (err != ESP_OK)
  {
    return err;
  }

  err = esp_partition_write(s_part, off, &rec.seq, 4);
  if (err != ESP_OK)
  {
    return err;
  }

  s_last_seq = rec.seq;
  s_cur_slot++;
  return ESP_OK;
}

void ntc_history_init(void)
{
  s_part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                    STORAGE_PARTITION_SUBTYPE,
                                    STORAGE_PARTITION_LABEL);
  if (!s_part)
  {
    ESP_LOGE(TAG, "Could not find storage partition, will not log NTC history");
    s_ready = false;
    return;
  }

  if (s_part->erase_size != SECTOR_SIZE)
  {
    ESP_LOGE(TAG, "Unexpected erase size=%" PRIu32, s_part->erase_size);
    s_ready = false;
    return;
  }

  s_sector_count = s_part->size / SECTOR_SIZE;
  if (s_sector_count == 0)
  {
    ESP_LOGE(TAG, "Partition too small");
    s_ready = false;
    return;
  }

  if (!s_lock)
  {
    s_lock = xSemaphoreCreateMutex();
  }

  xSemaphoreTake(s_lock, portMAX_DELAY);

  bool found_any = false;
  uint32_t best_sector = 0;
  uint32_t best_seq_start = 0;

  for (uint32_t i = 0; i < s_sector_count; i++)
  {
    sector_hdr_t hdr;
    if (!read_sector_hdr(i, &hdr))
    {
      continue;
    }

    if (!found_any || hdr.seq_start > best_seq_start)
    {
      found_any = true;
      best_sector = i;
      best_seq_start = hdr.seq_start;
    }
  }

  s_last_seq = 0;
  s_cur_sector = 0;
  s_cur_slot = 0;

  if (!found_any)
  {
    ESP_LOGI(TAG, "No valid log (format v%u); initializing sector 0",
             (unsigned) FORMAT_VERSION);

    ESP_ERROR_CHECK(
        esp_partition_erase_range(s_part, sector_offset(0), SECTOR_SIZE));
    ESP_ERROR_CHECK(write_sector_hdr(0, 1));
  }
  else
  {
    s_cur_sector = best_sector;
    s_last_seq = best_seq_start - 1;
    scan_current_sector_tail();

    if (s_cur_slot >= RECORDS_PER_SECTOR)
    {
      ESP_ERROR_CHECK(advance_sector());
    }
  }

  s_ready = true;

  ESP_LOGI(TAG,
           "Init ok: sectors=%" PRIu32 " records/sector=%u capacity=%u "
           "cur_sector=%" PRIu32 " cur_slot=%" PRIu32 " last_seq=%" PRIu32,
           s_sector_count, (unsigned) RECORDS_PER_SECTOR,
           (unsigned) ntc_history_get_capacity(), s_cur_sector, s_cur_slot,
           s_last_seq);

  xSemaphoreGive(s_lock);
}

void ntc_history_add_record(const float temps[NTC_CHANNELS_COUNT])
{
  if (!s_ready)
  {
    return;
  }

  record_ram_t r;
  r.timestamp = (uint32_t) time(NULL);
  for (int i = 0; i < NTC_CHANNELS_COUNT; i++)
  {
    r.temps_cC[i] = float_to_cC(temps[i]);
  }

  xSemaphoreTake(s_lock, portMAX_DELAY);

  if (s_ram_count < RAM_BUFFER_RECORDS)
  {
    s_ram_buf[s_ram_count++] = r;
  }

  bool should_flush = (s_ram_count >= RAM_BUFFER_RECORDS);
  xSemaphoreGive(s_lock);

  if (should_flush)
  {
    ntc_history_flush();
  }
}

void ntc_history_flush(void)
{
  if (!s_ready)
  {
    return;
  }

  record_ram_t tmp[RAM_BUFFER_RECORDS];
  size_t n = 0;

  xSemaphoreTake(s_lock, portMAX_DELAY);
  n = s_ram_count;
  if (n > 0)
  {
    memcpy(tmp, s_ram_buf, n * sizeof(tmp[0]));
    s_ram_count = 0;
  }
  xSemaphoreGive(s_lock);

  for (size_t i = 0; i < n; i++)
  {
    xSemaphoreTake(s_lock, portMAX_DELAY);
    esp_err_t err = write_one_record(tmp[i].timestamp, tmp[i].temps_cC);
    xSemaphoreGive(s_lock);

    if (err != ESP_OK)
    {
      ESP_LOGE(TAG, "write_one_record failed: %s", esp_err_to_name(err));
      return;
    }
  }
}

size_t ntc_history_get_capacity(void)
{
  if (!s_part)
  {
    return 0;
  }
  return (size_t) s_sector_count * (size_t) RECORDS_PER_SECTOR;
}

size_t ntc_history_iterate(uint32_t since_ts, size_t max,
                           ntc_history_iter_cb_t cb, void *ctx)
{
  if (!s_ready)
  {
    return 0;
  }

  if (max == 0)
  {
    max = (size_t) -1;
  }

  xSemaphoreTake(s_lock, portMAX_DELAY);

  sector_info_t sectors[128];
  size_t nsec = collect_sectors(sectors, 128);
  sort_sectors_by_seq_start(sectors, nsec);

  size_t emitted = 0;

  for (size_t si = 0; si < nsec; si++)
  {
    uint32_t sector = sectors[si].sector_idx;

    for (uint32_t slot = 0; slot < RECORDS_PER_SECTOR; slot++)
    {
      record_flash_t rf;
      if (!read_record(sector, slot, &rf))
      {
        goto done;
      }

      if (record_is_empty(&rf) || !record_is_valid(&rf))
      {
        break;
      }

      if (since_ts != 0 && rf.timestamp < since_ts)
      {
        continue;
      }

      ntc_record_t r;
      r.timestamp = rf.timestamp;
      memcpy(r.temps_cC, rf.temps_cC, sizeof(r.temps_cC));

      if (cb && !cb(&r, ctx))
      {
        goto done;
      }

      emitted++;
      if (emitted >= max)
      {
        goto done;
      }
    }
  }

done:
  xSemaphoreGive(s_lock);
  return emitted;
}

typedef struct
{
  size_t count;
} count_ctx_t;

static bool count_cb(const ntc_record_t *rec, void *ctx)
{
  (void) rec;
  count_ctx_t *c = (count_ctx_t *) ctx;
  c->count++;
  return true;
}

typedef struct
{
  ntc_record_t *out;
  size_t max;
  size_t skip;
  size_t written;
  size_t seen;
} copy_ctx_t;

static bool copy_cb(const ntc_record_t *rec, void *ctx)
{
  copy_ctx_t *c = (copy_ctx_t *) ctx;

  if (c->seen++ < c->skip)
  {
    return true;
  }

  if (c->written >= c->max)
  {
    return false;
  }

  c->out[c->written++] = *rec;
  return c->written < c->max;
}

size_t ntc_history_get_records(ntc_record_t *out_records, size_t max_records)
{
  if (!s_ready || !out_records || max_records == 0)
  {
    return 0;
  }

  // Two-pass: count then copy newest max_records.
  count_ctx_t cc = {.count = 0};
  (void) ntc_history_iterate(0, 0, count_cb, &cc);

  size_t total = cc.count;
  size_t skip = (total > max_records) ? (total - max_records) : 0;

  copy_ctx_t cx = {
      .out = out_records,
      .max = max_records,
      .skip = skip,
      .written = 0,
      .seen = 0,
  };

  (void) ntc_history_iterate(0, 0, copy_cb, &cx);
  return cx.written;
}

esp_err_t ntc_history_erase_all(void)
{
  if (!s_part)
  {
    return ESP_ERR_INVALID_STATE;
  }

  if (!s_lock)
  {
    s_lock = xSemaphoreCreateMutex();
  }

  xSemaphoreTake(s_lock, portMAX_DELAY);

  esp_err_t err = esp_partition_erase_range(s_part, 0, s_part->size);
  if (err != ESP_OK)
  {
    xSemaphoreGive(s_lock);
    return err;
  }

  err = esp_partition_erase_range(s_part, sector_offset(0), SECTOR_SIZE);
  if (err != ESP_OK)
  {
    xSemaphoreGive(s_lock);
    return err;
  }

  err = write_sector_hdr(0, 1);
  if (err != ESP_OK)
  {
    xSemaphoreGive(s_lock);
    return err;
  }

  s_cur_sector = 0;
  s_cur_slot = 0;
  s_last_seq = 0;
  s_ram_count = 0;
  s_ready = true;

  xSemaphoreGive(s_lock);
  return ESP_OK;
}
