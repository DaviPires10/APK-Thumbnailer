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

#include "resource_value.h"

#include <string.h>

static const float RADIX_MULTS[4] = {
    1.0f / (1 << 8),
    1.0f / (1 << 15),
    1.0f / (1 << 23),
    1.0f / (1 << 31),
};

struct Res_value {
  uint16_t size;
  uint8_t res0;

  uint8_t type;
  uint32_t data;
};

ResourceValue parse_resource_value(BinaryReader *reader, StringPool pool) {
  ResourceValue value = {0};

  struct Res_value resource_value = {0};
  resource_value.size             = read_u16(reader);
  resource_value.res0             = read_u8(reader);
  resource_value.type             = read_u8(reader);
  resource_value.data             = read_u32(reader);

  value.type = resource_value.type;
  value.raw  = resource_value.data;

  switch (value.type) {
    case TYPE_REFERENCE:
    case TYPE_FLOAT: // Why not take advantage of the way floats are represented in binary
    case TYPE_INT_DEC:
    case TYPE_INT_HEX:
    case TYPE_INT_BOOLEAN:
      value.data.integer = value.raw;
      break;

    case TYPE_STRING: {
      char *string      = string_pool_get(pool, value.raw);
      value.data.string = string ? string : strdup("");
      break;
    }

    case TYPE_DIMENSION: {
      int radix           = (value.raw >> 4) & 0x3;
      int mantissa        = value.raw & 0xFFFFFF00;
      float ret           = mantissa * RADIX_MULTS[radix];
      value.data.floating = ret;
      break;
    }

    case TYPE_FRACTION: {
      int radix           = (value.raw >> 4) & 0x3;
      int mantissa        = value.raw & 0xFFFFFF00;
      float ret           = (mantissa * RADIX_MULTS[radix]) * 100.0f;
      value.data.floating = ret;
      break;
    }

    case TYPE_INT_COLOR_ARGB8: {
      uint32_t alpha     = value.raw >> 24;
      uint32_t rgb       = value.raw << 8;
      uint32_t ret       = rgb | alpha;
      value.data.integer = ret;
      break;
    }

    case TYPE_INT_COLOR_RGB8:
      value.data.integer = value.raw & 0x00FFFFFF;
      break;

    case TYPE_INT_COLOR_ARGB4: {
      uint32_t alpha     = value.raw >> 12;
      uint32_t rgb       = value.raw << 4;
      uint32_t ret       = rgb | alpha;
      value.data.integer = ret;
      break;
    }

    case TYPE_INT_COLOR_RGB4:
      value.data.integer = value.raw & 0x0FFF;
      break;

    default:
      value.type         = TYPE_NULL;
      value.raw          = 0;
      value.data.integer = 0;
      break;
  }

  return value;
}
