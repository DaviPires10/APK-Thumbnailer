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

#ifndef CHUNK_H
#define CHUNK_H

#include "binary_reader.h"

#include <stdbool.h>
#include <stdint.h>

enum {
  RES_XML_NULL_TYPE        = 0x0000,
  RES_XML_STRING_POOL_TYPE = 0x0001,
  RES_XML_TABLE_TYPE       = 0x0002,
  RES_XML_TYPE             = 0x0003,

  // AXML specific Chunk Types
  RES_XML_START_NS_TYPE  = 0x0100,
  RES_XML_END_NS_TYPE    = 0x0101,
  RES_XML_START_TAG_TYPE = 0x0102,
  RES_XML_END_TAG_TYPE   = 0x0103,
  RES_XML_TEXT_TYPE      = 0x0104,
  RES_XML_RES_MAP_TYPE   = 0x0180,

  // ARSC specific Chunk Types
  RES_XML_PACKAGE_TYPE   = 0x0200,
  RES_XML_TYPE_TYPE      = 0x0201,
  RES_XML_TYPE_SPEC_TYPE = 0x0202,
};

typedef struct {
  uint16_t type;
  uint16_t header_size;
  uint32_t size;
} ResChunk_header;

ResChunk_header read_chunk_header(BinaryReader *reader);
void skip_chunk(BinaryReader *reader, size_t chunk_start_pos, ResChunk_header header);
void skip_chunk_header_padding(BinaryReader *buf, ResChunk_header header);

#endif
