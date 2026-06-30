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

#include "binary_reader.h"

#include <stdlib.h>
#include <string.h>

BinaryReader set_buffer(const uint8_t *data, size_t size) {
  BinaryReader reader;
  reader.data = data;
  reader.size = size;
  reader.pos  = 0;
  return reader;
}

void clear_buffer(BinaryReader *reader) {
  reader->data = NULL;
  reader->pos  = 0;
  reader->size = 0;
}

bool at_end(BinaryReader *reader) {
  return reader->pos >= reader->size;
}

void seek(BinaryReader *reader, size_t pos) {
  reader->pos = pos;
  if (reader->pos > reader->size)
    reader->pos = reader->size;
}

void skip(BinaryReader *reader, size_t bytes) {
  reader->pos += bytes;

  if (reader->pos > reader->size)
    reader->pos = reader->size;
}

uint8_t read_u8(BinaryReader *reader) {
  if (reader->pos + 1 > reader->size)
    return 0;
  return reader->data[reader->pos++];
}

uint16_t read_u16(BinaryReader *reader) {
  if (reader->pos + 2 > reader->size)
    return 0;
  uint16_t val = reader->data[reader->pos] | (reader->data[reader->pos + 1] << 8);
  reader->pos += 2;
  return val;
}

uint32_t read_u32(BinaryReader *reader) {
  if (reader->pos + 4 > reader->size)
    return 0;
  uint32_t val = reader->data[reader->pos] | (reader->data[reader->pos + 1] << 8) |
                 (reader->data[reader->pos + 2] << 16) | (reader->data[reader->pos + 3] << 24);
  reader->pos += 4;
  return val;
}

size_t read_raw(BinaryReader *reader, void *dst, size_t bytes) {
  if (reader->pos + bytes > reader->size)
    bytes = reader->size - reader->pos;
  memcpy(dst, &reader->data[reader->pos], bytes);
  reader->pos += bytes;
  return bytes;
}
