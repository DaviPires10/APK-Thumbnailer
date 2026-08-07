/* SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (C) 2026 Davi Pires <davipiresalvesdacunha2@gmail.com>
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

#ifndef RESOURCE_VALUE_H
#define RESOURCE_VALUE_H

#include "binary_reader.h"
#include "string_pool.h"

#include <stdbool.h>
#include <stdint.h>

enum : uint8_t {
  TYPE_NULL              = 0x00,
  TYPE_REFERENCE         = 0x01,
  TYPE_ATTRIBUTE         = 0x02,
  TYPE_STRING            = 0x03,
  TYPE_FLOAT             = 0x04,
  TYPE_DIMENSION         = 0x05,
  TYPE_FRACTION          = 0x06,
  TYPE_DYNAMIC_REFERENCE = 0x07,
  TYPE_DYNAMIC_ATTRIBUTE = 0x08,

  TYPE_INT_DEC         = 0x10,
  TYPE_INT_HEX         = 0x11,
  TYPE_INT_BOOLEAN     = 0x12,
  TYPE_INT_COLOR_ARGB8 = 0x1c,
  TYPE_INT_COLOR_RGB8  = 0x1d,
  TYPE_INT_COLOR_ARGB4 = 0x1e,
  TYPE_INT_COLOR_RGB4  = 0x1f,
};

typedef struct {
  uint8_t type;
  uint32_t raw;
  union {
    char *string;
    int32_t integer;
    float floating;
  } data;
} ResourceValue;

ResourceValue parse_resource_value(BinaryReader *reader, StringPool pool);

#endif
