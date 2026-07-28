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

#include "arsc.h"

#include "chunk.h"

#include <stdlib.h>
#include <string.h>

ArscTable parse_arsc_table(const uint8_t *data, size_t size) {
  BinaryReader reader = set_buffer(data, size);

  ArscTable table = {0};

  uint32_t pkgs_cap = 0;
  ArscPackage *pkg  = NULL;

  while (!at_end(&reader)) {
    size_t chunk_start     = reader.pos;
    ResChunk_header header = read_chunk_header(&reader);

    switch (header.type) {
      case RES_TABLE_TYPE: {
        pkgs_cap       = read_u32(&reader);
        table.packages = calloc(pkgs_cap, sizeof(ArscPackage));
        if (!table.packages) {
          return table;
        }

        break;
      }

      case RES_STRING_POOL_TYPE: {
        if (table.global_pool.count == 0) {
          table.global_pool = parse_string_pool(&reader, chunk_start);
        }

        goto next_chunk;
      }

      case RES_TABLE_PACKAGE_TYPE: {
        if (!table.packages || table.package_count >= pkgs_cap) {
          goto next_chunk;
        }

        pkg     = &table.packages[table.package_count++];
        pkg->id = read_u32(&reader);

        skip_chunk_header_padding(&reader, chunk_start, header);
        break;
      }

      case RES_TABLE_TYPE_TYPE: {
        if (!pkg) {
          goto next_chunk;
        }

        ResourceValue *entries = NULL;
        ArscType *type         = NULL;

        uint8_t type_id = read_u8(&reader);
        skip(&reader, 3);
        uint32_t entry_count   = read_u32(&reader);
        uint32_t entries_start = read_u32(&reader);

        for (uint32_t i = 0; i < pkg->type_count; ++i) {
          if (pkg->types[i].type_id == type_id) {
            type = &pkg->types[i];
            break;
          }
        }

        if (!type) {
          if (pkg->type_count >= pkg->types_capacity) {
            size_t capacity = pkg->types_capacity ? pkg->types_capacity * 2 : 8;
            ArscType *types = realloc(pkg->types, capacity * sizeof(ArscType));
            if (!types) {
              goto next_chunk;
            }
            pkg->types          = types;
            pkg->types_capacity = capacity;
          }

          entries = calloc(entry_count, sizeof(ResourceValue));
          if (!entries) {
            goto next_chunk;
          }

          type              = &pkg->types[pkg->type_count++];
          type->type_id     = type_id;
          type->entry_count = entry_count;
          type->entries     = entries;
        }

        for (size_t i = 0; i < entry_count; ++i) {
          seek(&reader, chunk_start + header.header_size + i * sizeof(uint32_t));
          uint32_t offset = read_u32(&reader);
          if (offset == UINT32_MAX) {
            continue;
          }

          size_t entry_start = chunk_start + entries_start + offset;
          seek(&reader, entry_start);

          uint16_t config_size = read_u16(&reader);
          uint16_t flags       = read_u16(&reader);

          if (!(flags & 0x0001)) {
            seek(&reader, entry_start + config_size);
            type->entries[i] = parse_resource_value(&reader, table.global_pool);
          }
        }

        goto next_chunk;
      }

      next_chunk:
      default:
        skip_chunk(&reader, chunk_start, header);
        break;
    }
  }

  return table;
}

ResourceValue arsc_table_resolve(const ArscTable table, uint32_t id, int depth) {
  ResourceValue val = {0};

  if (depth > 20) {
    return val;
  }

  uint8_t pkg_id       = (id >> 24) & 0xFF;
  uint8_t type_id      = (id >> 16) & 0xFF;
  uint16_t entry_index = id & 0xFFFF;

  ArscPackage *pkg = NULL;
  for (size_t i = 0; i < table.package_count; ++i) {
    if (table.packages[i].id == pkg_id) {
      pkg = &table.packages[i];
      break;
    }
  }

  for (size_t i = 0; i < pkg->type_count; ++i) {
    if (pkg->types[i].type_id == type_id) {
      if (entry_index < pkg->types[i].entry_count) {
        val = pkg->types[i].entries[entry_index];

        if (val.type == TYPE_REFERENCE) {
          return arsc_table_resolve(table, val.raw, depth + 1);
        }
        return val;
      }
      break;
    }
  }

  return val;
}

void arsc_table_free(ArscTable *table) {
  if (!table)
    return;

  string_pool_free(&table->global_pool);

  for (size_t i = 0; i < table->package_count; ++i) {
    for (size_t j = 0; j < table->packages[i].type_count; ++j) {
      free(table->packages[i].types[j].entries);
    }
    free(table->packages[i].types);
  }

  free(table->packages);
}
