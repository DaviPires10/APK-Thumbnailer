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

  result.strings = strings;
  result.count   = pool.strings_count;

  return result;
}

void string_pool_append(StringPool *pool, char *str) {
  if (!pool || !str) {
    return;
  }

  if (pool->count >= pool->capacity) {
    pool->capacity = pool->capacity ? pool->capacity * 2 : 8;
    pool->strings  = realloc(pool->strings, pool->capacity * sizeof(char *));
  }

  if (pool->strings) {
    pool->strings[pool->count++] = str;
  }
}

char *string_pool_get(StringPool pool, size_t index) {
  if (index >= pool.count) {
    return NULL;
  }
  return strdup(pool.strings[index]);
}

uint32_t string_pool_get_index(StringPool pool, const char *str) {
  if (!pool.strings || !str) {
    return UINT32_MAX;
  }

  for (size_t i = 0; i < pool.count; ++i) {
    if (strcmp(pool.strings[i], str) == 0) {
      return i;
    }
  }

  return UINT32_MAX;
}

void string_pool_free(StringPool *pool) {
  if (!pool || !pool->strings) {
    return;
  }
  for (size_t i = 0; i < pool->count; ++i) {
    free(pool->strings[i]);
  }
  free(pool->strings);
  pool->strings = NULL;
  pool->count   = 0;
}
