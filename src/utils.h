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

#ifndef UTILS_H
#define UTILS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <uchar.h>

typedef struct {
  const uint8_t *data;
  size_t size;
  size_t pos;
} Buffer;

Buffer create_buffer(const uint8_t *data, size_t size);

bool at_end(Buffer *buf);
void seek(Buffer *buf, size_t pos);
void skip(Buffer *buf, size_t bytes);

uint8_t read_u8(Buffer *buf);
uint16_t read_u16(Buffer *buf);
uint32_t read_u32(Buffer *buf);
size_t read_raw(Buffer *buf, void *dst, size_t bytes);

char *utf16_to_utf8(const char16_t *utf16, size_t len);

#endif
