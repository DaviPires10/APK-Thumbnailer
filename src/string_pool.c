/* SPDX-License-Identifier: GPL-3.0-only or GPL-3.0-or-later
 *
 * Copyright (C) 2026 Davi Pires <davipiresalvesdacunha2@gmail.com>.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "string_pool.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct ResStringPool_header {
  uint32_t strings_count;
  uint32_t styles_count;
  uint32_t flags;
  uint32_t strings_start;
  uint32_t styles_start;
};

// FNV-1a Hash Algorithm
static uint32_t hash_string(const char *str) {
  uint32_t hash = 2166136261u;
  while (*str) {
    hash ^= (unsigned char)*str++;
    hash *= 16777619u;
  }
  return hash;
}

static void
hash_table_insert(StringPoolEntry *table, size_t table_capacity, const char *key, uint32_t index) {
  if (table_capacity == 0 || !key)
    return;

  uint32_t slot = hash_string(key) % table_capacity;
  while (table[slot].key != NULL) {
    ++slot;
    slot %= table_capacity;
  }
  table[slot].key   = key;
  table[slot].index = index;
}

static int decode_utf8_length(BinaryReader *reader) {
  uint8_t len = read_u8(reader);
  if (len & 0x80) {
    uint8_t extra = read_u8(reader);
    return ((len & 0x7F) << 8) | extra;
  }
  return len;
}

static int decode_utf16_length(BinaryReader *reader) {
  uint16_t len = read_u16(reader);
  if (len & 0x8000) {
    uint16_t extra = read_u16(reader);
    return ((len & 0x7FFF) << 16) | extra;
  }
  return len;
}

static char *utf16_to_utf8(const uint16_t *in, size_t len) {
  size_t out_cap = len * 3 + 1;
  char *out      = malloc(out_cap);
  char *p        = out;

  for (size_t i = 0; i < len; i++) {
    uint32_t c = in[i];

    if (c < 0x80) {
      *p++ = c;
    } else if (c < 0x800) {
      *p++ = 0xC0 | (c >> 6);
      *p++ = 0x80 | (c & 0x3F);
    } else {
      *p++ = 0xE0 | (c >> 12);
      *p++ = 0x80 | ((c >> 6) & 0x3F);
      *p++ = 0x80 | (c & 0x3F);
    }
  }

  *p = 0;
  return out;
}

StringPool parse_string_pool(BinaryReader *reader, size_t chunk_start) {
  StringPool result = {0};

  struct ResStringPool_header pool;
  pool.strings_count = read_u32(reader);
  pool.styles_count  = read_u32(reader);
  pool.flags         = read_u32(reader);
  pool.strings_start = read_u32(reader);
  pool.styles_start  = read_u32(reader);

  if (pool.strings_count == 0) {
    return result;
  }
  uint32_t *offsets = malloc(pool.strings_count * sizeof(uint32_t));
  char **strings    = malloc(pool.strings_count * sizeof(char *));
  if (!offsets || !strings) {
    free(offsets);
    free(strings);
    return result;
  }

  read_raw(reader, offsets, pool.strings_count * sizeof(uint32_t));

  for (size_t i = 0; i < pool.strings_count; ++i) {
    seek(reader, chunk_start + pool.strings_start + offsets[i]);
    if ((pool.flags & 0x100) != 0) {    // UTF-8
      (void)decode_utf8_length(reader); // skip char length
      int length = decode_utf8_length(reader);
      strings[i] = malloc(length + 1);
      if (strings[i]) {
        read_raw(reader, strings[i], length + 1);
      } else {
        strings[i] = strdup("");
      }
    } else { // UTF-16
      int length         = decode_utf16_length(reader);
      uint16_t *utf16str = malloc(length * sizeof(uint16_t));
      if (utf16str) {
        read_raw(reader, utf16str, length * sizeof(uint16_t));
        strings[i] = utf16_to_utf8(utf16str, length);
        free(utf16str);
      } else {
        strings[i] = strdup("");
      }
    }
  }
  free(offsets);

  result.strings  = strings;
  result.count    = pool.strings_count;
  result.capacity = pool.strings_count;

  // Build the initial hash map lookup cache from parsed data
  if (result.count > 0) {
    result.table_capacity = result.capacity * 2;
    result.hash_table     = calloc(result.table_capacity, sizeof(StringPoolEntry));
    if (result.hash_table) {
      for (size_t i = 0; i < result.count; ++i) {
        if (result.strings[i]) {
          hash_table_insert(result.hash_table, result.table_capacity, result.strings[i], i);
        }
      }
    }
  }

  return result;
}

void string_pool_append(StringPool *pool, char *str) {
  if (!pool || !str) {
    return;
  }

  if (string_pool_get_index(*pool, str) != UINT32_MAX) {
    return;
  }

  if (pool->count >= pool->capacity) {
    pool->capacity = pool->capacity ? pool->capacity * 2 : 8;
    pool->strings  = realloc(pool->strings, pool->capacity * sizeof(char *));

    // Scale and rehash the lookup cache to prevent collision degradation
    size_t new_table_capacity  = pool->capacity * 2;
    StringPoolEntry *new_table = calloc(new_table_capacity, sizeof(StringPoolEntry));
    if (new_table) {
      for (size_t i = 0; i < pool->count; ++i) {
        if (pool->strings[i]) {
          hash_table_insert(new_table, new_table_capacity, pool->strings[i], i);
        }
      }
      free(pool->hash_table);
      pool->hash_table     = new_table;
      pool->table_capacity = new_table_capacity;
    }
  }

  if (pool->strings) {
    char *dup_str              = strdup(str);
    pool->strings[pool->count] = dup_str;

    // Add new string record to the map cache
    if (pool->hash_table && dup_str) {
      hash_table_insert(pool->hash_table, pool->table_capacity, dup_str, pool->count);
    }
    pool->count++;
  }
}

char *string_pool_get(StringPool pool, size_t index) {
  if (index >= pool.count) {
    return NULL;
  }
  return pool.strings[index];
}

uint32_t string_pool_get_index(StringPool pool, const char *str) {
  if (!pool.strings || !pool.hash_table || !str || pool.table_capacity == 0) {
    return UINT32_MAX;
  }

  uint32_t slot = hash_string(str) % pool.table_capacity;

  while (pool.hash_table[slot].key != NULL) {
    if (strcmp(pool.hash_table[slot].key, str) == 0) {
      return pool.hash_table[slot].index;
    }
    ++slot;
    slot %= pool.table_capacity;
  }

  return UINT32_MAX;
}

void string_pool_get_indices_batch(StringPool pool,
                                   const char **strings,
                                   size_t count,
                                   uint32_t *out_indices) {
  if (!out_indices)
    return;

  for (size_t i = 0; i < count; ++i) {
    if (!strings[i]) {
      out_indices[i] = UINT32_MAX;
      continue;
    }
    out_indices[i] = string_pool_get_index(pool, strings[i]);
  }
}

void string_pool_free(StringPool *pool) {
  if (!pool) {
    return;
  }

  if (pool->strings) {
    for (size_t i = 0; i < pool->count; ++i) {
      free(pool->strings[i]);
    }
    free(pool->strings);
    pool->strings = NULL;
  }
  if (pool->hash_table) {
    free(pool->hash_table);
    pool->hash_table = NULL;
  }
  pool->count          = 0;
  pool->capacity       = 0;
  pool->table_capacity = 0;
}
