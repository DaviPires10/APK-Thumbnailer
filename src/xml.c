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

#include "xml.h"

#include "binary_reader.h"
#include "string_pool.h"

#include <stdlib.h>

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
  TYPE_FIRST_INT         = 0x10,
  TYPE_INT_DEC           = 0x10,
  TYPE_INT_HEX           = 0x11,
  TYPE_INT_BOOLEAN       = 0x12,
  TYPE_INT_COLOR_ARGB8   = 0x1c,
  TYPE_INT_COLOR_RGB8    = 0x1d,
  TYPE_INT_COLOR_ARGB4   = 0x1e,
  TYPE_INT_COLOR_RGB4    = 0x1f,
  TYPE_LAST_COLOR_INT    = 0x1f,
};

struct ResTable_ref {
  uint32_t ident;
};

struct Res_value {
  uint16_t size;
  uint8_t res0;

  uint8_t data_type;
  uint32_t data;
};

struct ResXMLTree_attrExt {
  struct ResStringPool_ref ns;
  struct ResStringPool_ref name;

  uint16_t attr_start;
  uint16_t attr_size;
  uint16_t attr_count;
};

struct ResXMLTree_attribute {
  struct ResStringPool_ref ns;
  struct ResStringPool_ref name;

  struct ResStringPool_ref raw_value;
  struct Res_value typed_value;
};

XmlElement
xml_parse_element(BinaryReader *reader, StringPool pool, size_t chunk_start) {
  XmlElement result = {0};

  struct ResXMLTree_attrExt node;
  node.ns.index   = read_u32(reader);
  node.name.index = read_u32(reader);
  node.attr_start = read_u16(reader);
  node.attr_size  = read_u16(reader);
  node.attr_count = read_u16(reader);

  XmlAttribute *attrs = malloc(node.attr_count * sizeof(XmlElement));

  for (size_t i = 0; i < node.attr_count; ++i) {
    seek(reader, chunk_start + node.attr_start + node.attr_size * i);
    struct ResXMLTree_attribute attr;
    attr.ns.index              = read_u32(reader);
    attr.name.index            = read_u32(reader);
    attr.raw_value.index       = read_u32(reader);
    attr.typed_value.size      = read_u16(reader);
    attr.typed_value.res0      = read_u8(reader);
    attr.typed_value.data_type = read_u8(reader);
    attr.typed_value.data      = read_u32(reader);

    attrs[i].ns        = string_pool_get(pool, attr.ns.index);
    attrs[i].name      = string_pool_get(pool, attr.name.index);
    attrs[i].data_type = attr.typed_value.data_type;
    attrs[i].data      = attr.typed_value.data;
  }

  result.ns         = string_pool_get(pool, node.ns.index);
  result.name       = string_pool_get(pool, node.name.index);
  result.attr_count = node.attr_count;
  result.attributes = attrs;

  return result;
}
