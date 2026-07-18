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

#include "resource_value.h"

struct Res_value {
  uint16_t size;
  uint8_t res0;

  uint8_t type;
  uint32_t data;
};

ResourceValue parse_resource_value(BinaryReader *reader) {
  ResourceValue value = {0};

  if (!reader) {
    return value;
  }

  struct Res_value resource_value = {0};
  resource_value.size             = read_u16(reader);
  resource_value.res0             = read_u8(reader);
  resource_value.type             = read_u8(reader);
  resource_value.data             = read_u32(reader);

  value.type = resource_value.type;
  value.data = resource_value.data;

  return value;
}
