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

#ifndef BINARY_READER_H
#define BINARY_READER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
  const uint8_t *data;
  size_t size;
  size_t pos;
} BinaryReader;

BinaryReader set_buffer(const uint8_t *data, size_t size);
void clear_buffer(BinaryReader *reader);

bool at_end(BinaryReader *reader);
void seek(BinaryReader *reader, size_t pos);
void skip(BinaryReader *reader, size_t bytes);

uint8_t read_u8(BinaryReader *reader);
uint16_t read_u16(BinaryReader *reader);
uint32_t read_u32(BinaryReader *reader);
size_t read_raw(BinaryReader *reader, void *dst, size_t bytes);

#endif
