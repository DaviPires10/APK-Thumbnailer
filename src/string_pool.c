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
  uint32_t string_count;
  uint32_t style_count;
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

static void string_pool_build_hash(StringPool *pool) {
  if (!pool) {
    return;
  }

  if (pool->hash_table) {
    free(pool->hash_table);
  }
  pool->hash_table     = NULL;
  pool->table_capacity = 0;

  if (pool->capacity == 0) {
    return;
  }

  size_t capacity = pool->capacity < 8 ? 8 : pool->capacity * 2;
  uint32_t *table = malloc(capacity * sizeof(uint32_t));
  if (!table) {
    return;
  }

  for (size_t i = 0; i < capacity; ++i) {
    table[i] = UINT32_MAX;
  }

  pool->hash_table     = table;
  pool->table_capacity = capacity;

  for (size_t i = 0; i < pool->count; ++i) {
    if (!pool->strings[i]) {
      continue;
    }

    size_t slot = hash_string(pool->strings[i]) % capacity;
    while (table[slot] != UINT32_MAX) {
      ++slot;
      slot %= pool->table_capacity;
    }
    table[slot] = i;
  }

  return;
}

static int decode_utf8_length(BinaryReader *reader) {
  if (!reader) {
    return 0;
  }

  uint8_t len = read_u8(reader);
  if (len & 0x80) {
    uint8_t extra = read_u8(reader);
    return ((len & 0x7F) << 8) | extra;
  }
  return len;
}

static int decode_utf16_length(BinaryReader *reader) {
  if (!reader) {
    return 0;
  }

  uint16_t len = read_u16(reader);
  if (len & 0x8000) {
    uint16_t extra = read_u16(reader);
    return ((len & 0x7FFF) << 16) | extra;
  }
  return len;
}

static char *utf16_to_utf8(const uint16_t *str, size_t length) {
  char *result   = NULL;
  size_t out_cap = length * 3 + 1;
  char *out      = malloc(out_cap);

  if (!out) {
    return strdup("");
  }

  char *p = out;
  for (size_t i = 0; i < length; i++) {
    uint32_t c = str[i];

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

  result = strdup(out);
  free(out);

  return result;
}

static char *parse_utf8_string(BinaryReader *reader) {
  int char_length = decode_utf8_length(reader);
  int byte_length = decode_utf8_length(reader);
  if (char_length == 0 || byte_length == 0) {
    return NULL;
  }

  char *string = malloc(byte_length + 1);
  if (!string) {
    return NULL;
  }

  if (read_raw(reader, string, byte_length) != byte_length) {
    free(string);
    return NULL;
  }

  string[byte_length] = '\0';
  return string;
}

static char *parse_utf16_string(BinaryReader *reader) {
  int char_length = decode_utf16_length(reader);
  if (char_length == 0) {
    return NULL;
  }

  uint16_t *utf16_str = malloc(char_length * sizeof(uint16_t));
  if (!utf16_str) {
    return NULL;
  }

  for (size_t i = 0; i < char_length; ++i) {
    utf16_str[i] = read_u16(reader);
  }

  char *string = utf16_to_utf8(utf16_str, char_length);
  free(utf16_str);

  return string;
}

StringPool parse_string_pool(BinaryReader *reader, size_t chunk_start) {
  StringPool result = {0};

  if (!reader || !reader->data) {
    return result;
  }

  struct ResStringPool_header pool_header;
  pool_header.string_count  = read_u32(reader);
  pool_header.style_count   = read_u32(reader);
  pool_header.flags         = read_u32(reader);
  pool_header.strings_start = read_u32(reader);
  pool_header.styles_start  = read_u32(reader);

  if (pool_header.string_count == 0) {
    return result;
  }
  uint32_t *offsets = malloc(pool_header.string_count * sizeof(uint32_t));
  char **strings    = malloc(pool_header.string_count * sizeof(char *));
  if (!offsets || !strings) {
    free(offsets);
    free(strings);
    return result;
  }

  read_raw(reader, offsets, pool_header.string_count * sizeof(uint32_t));

  for (size_t i = 0; i < pool_header.string_count; ++i) {
    seek(reader, chunk_start + pool_header.strings_start + offsets[i]);

    if (pool_header.flags & 0x100) {
      strings[i] = parse_utf8_string(reader);
    } else {
      strings[i] = parse_utf16_string(reader);
    }

    if (!strings[i]) {
      strings[i] = strdup("");
    }
  }

  free(offsets);

  result.strings  = strings;
  result.count    = pool_header.string_count;
  result.capacity = pool_header.string_count;

  string_pool_build_hash(&result);

  return result;
}

void string_pool_append(StringPool *pool, const char *str) {
  if (!pool || !str || string_pool_get_index(*pool, str) != UINT32_MAX) {
    return;
  }

  if (pool->count >= pool->capacity) {
    size_t tmp_cap = pool->capacity ? pool->capacity * 2 : 8;
    if (tmp_cap < pool->capacity || tmp_cap > SIZE_MAX / sizeof(char *)) {
      return;
    }

    char **tmp = realloc(pool->strings, tmp_cap * sizeof(char *));
    if (!tmp) {
      return;
    }

    pool->strings  = tmp;
    pool->capacity = tmp_cap;
  }

  char *dup_str = strdup(str);
  if (!dup_str) {
    return;
  }

  pool->strings[pool->count++] = dup_str;
  string_pool_build_hash(pool);
}

char *string_pool_get(StringPool pool, size_t index) {
  if (!pool.strings || index >= pool.count) {
    return NULL;
  }
  return pool.strings[index];
}

uint32_t string_pool_get_index(StringPool pool, const char *str) {
  if (!pool.strings || !pool.hash_table || !str || pool.table_capacity == 0) {
    return UINT32_MAX;
  }

  uint32_t slot = hash_string(str) % pool.table_capacity;

  while (pool.hash_table[slot] != UINT32_MAX) {
    uint32_t index = pool.hash_table[slot];
    if (strcmp(pool.strings[index], str) == 0) {
      return index;
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
  if (!out_indices) {
    return;
  }

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
