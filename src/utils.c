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

#include "utils.h"

#include <stdlib.h>
#include <string.h>

Buffer create_buffer(const uint8_t *data, size_t size) {
  Buffer buf = {.data = data, .size = size, .pos = 0};
  return buf;
}

bool at_end(Buffer *buf) {
  return buf->pos >= buf->size;
}

void seek(Buffer *buf, size_t pos) {
  buf->pos = pos;
  if (buf->pos > buf->size)
    buf->pos = buf->size;
}

void skip(Buffer *buf, size_t bytes) {
  buf->pos += bytes;

  if (buf->pos > buf->size)
    buf->pos = buf->size;
}

uint8_t read_u8(Buffer *buf) {
  if (buf->pos + 1 > buf->size)
    return 0;
  return buf->data[buf->pos++];
}

uint16_t read_u16(Buffer *buf) {
  if (buf->pos + 2 > buf->size)
    return 0;
  uint16_t val = buf->data[buf->pos] | (buf->data[buf->pos + 1] << 8);
  buf->pos += 2;
  return val;
}

uint32_t read_u32(Buffer *buf) {
  if (buf->pos + 4 > buf->size)
    return 0;
  uint32_t val = buf->data[buf->pos] | (buf->data[buf->pos + 1] << 8) |
                 (buf->data[buf->pos + 2] << 16) |
                 (buf->data[buf->pos + 3] << 24);
  buf->pos += 4;
  return val;
}

size_t read_raw(Buffer *buf, void *dst, size_t bytes) {
  if (buf->pos + bytes > buf->size)
    bytes = buf->size - buf->pos;
  memcpy(dst, &buf->data[buf->pos], bytes);
  return bytes;
}

char *utf16_to_utf8(const uint16_t *in, size_t len) {
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
