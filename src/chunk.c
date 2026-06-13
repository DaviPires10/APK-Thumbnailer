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

#include "chunk.h"

ResChunkHeader read_chunk_header(BinaryReader *reader) {
  ResChunkHeader header;
  header.type        = read_u16(reader);
  header.header_size = read_u16(reader);
  header.size        = read_u32(reader);
  return header;
}

void skip_chunk(BinaryReader *reader,
                size_t chunk_start_pos,
                ResChunkHeader header) {
  seek(reader, chunk_start_pos);
  skip(reader, header.size);
}

void skip_chunk_header_padding(BinaryReader *buf, ResChunkHeader header) {
  size_t current_header_read = 8;
  if (header.header_size > current_header_read) {
    skip(buf, header.header_size - current_header_read);
  }
}
