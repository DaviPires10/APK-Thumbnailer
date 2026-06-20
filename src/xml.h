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

#ifndef XML_H
#define XML_H

#include "binary_reader.h"
#include "string_pool.h"

#include <stddef.h>
#include <stdint.h>

typedef struct {
  char *ns;
  char *name;

  uint8_t data_type;
  uint32_t data;
} XmlAttribute;

typedef struct {
  char *ns;
  char *name;
  uint16_t attr_count;
  XmlAttribute *attributes;
} XmlElement;

XmlElement
xml_parse_element(BinaryReader *reader, StringPool pool, size_t chunk_start);

#endif
