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

#include "apk.h"

#include "chunk.h"

#include <stdlib.h>
#include <string.h>

uint32_t get_application_icon_resource_reference_id(const uint8_t *data,
                                                    size_t size) {
  BinaryReader reader = set_buffer(data, size);

  uint32_t str_manifest_index    = UINT32_MAX;
  uint32_t str_application_index = UINT32_MAX;
  uint32_t str_icon_index        = UINT32_MAX;
  bool in_manifest_node          = false;

  while (!at_end(&reader)) {
    uint32_t chunk_start  = reader.pos;
    ResChunkHeader header = read_chunk_header(&reader);

    switch (header.type) {
      case RES_XML_TYPE: {
        break;
      }

      case RES_XML_STRING_POOL_TYPE: {
        seek(&reader, chunk_start);
        StringPool pool = parse_string_pool(&reader);

        for (size_t i = 0; i < pool.count; i++) {
          char *s = string_pool_get(pool, i);
          if (s) {
            if (strcmp(s, "manifest") == 0) {
              str_manifest_index = i;
            } else if (strcmp(s, "application") == 0) {
              str_application_index = i;
            } else if (strcmp(s, "icon") == 0) {
              str_icon_index = i;
            }
          }
          if (str_manifest_index != UINT32_MAX &&
              str_application_index != UINT32_MAX &&
              str_icon_index != UINT32_MAX)
            break;
        }
        string_pool_free(&pool);
        skip(&reader, header.size - (reader.pos - chunk_start));
        break;
      }

      case RES_XML_START_TAG_TYPE: {
        skip(&reader, 2 * sizeof(uint32_t));
        skip(&reader,
             sizeof(uint32_t)); // skip namespace index
        uint32_t name = read_u32(&reader);

        if (name == str_manifest_index)
          in_manifest_node = true;

        if (!in_manifest_node || name != str_application_index) {
          skip(&reader, header.size - (reader.pos - chunk_start));
          break;
        }

        skip(&reader, 2 * sizeof(uint16_t));
        uint16_t attribute_count = read_u16(&reader);
        skip(&reader, 3 * sizeof(uint16_t));

        // attributes
        for (uint32_t i = 0; i < attribute_count; i++) {
          skip(&reader, 4);
          uint32_t name      = read_u32(&reader);
          uint32_t raw_value = read_u32(&reader);
          if (name != str_icon_index) {
            // skip typed data
            skip(&reader, 8);
            continue;
          }
          // typed data
          skip(&reader,
               sizeof(uint16_t) + sizeof(uint8_t)); // skip size and zero
          uint8_t data_type = read_u8(&reader);
          uint32_t data     = read_u32(&reader);
          if (raw_value == UINT32_MAX && data_type == 1 /*refernce type*/) {
            return data;
          }
        }
        break;
      }

      case RES_XML_END_TAG_TYPE: {
        skip(&reader, 12);
        uint32_t name = read_u32(&reader);

        if (name == str_manifest_index)
          in_manifest_node = false;
        break;
      }

      default:
        skip(&reader, header.size - 8);
        break;
    }
  }
  return 0;
}

StringPool get_application_icon_resource_path(const uint8_t *data,
                                              size_t size,
                                              uint32_t reference_id) {
  BinaryReader reader = set_buffer(data, size);

  uint32_t res_type  = (reference_id >> 16) & 0xff;
  uint32_t res_index = reference_id & 0xffff;

  StringPool pool  = {0};
  StringPool icons = {0};

  while (!at_end(&reader)) {
    uint32_t chunk_start  = reader.pos;
    ResChunkHeader header = read_chunk_header(&reader);

    switch (header.type) {
      case RES_XML_TABLE_TYPE: {
        skip(&reader, 4); // skip package_count
        break;
      }

      case RES_XML_STRING_POOL_TYPE: {
        if (pool.count == 0) {
          seek(&reader, chunk_start);
          pool = parse_string_pool(&reader);
        }
        goto skip_chunk;
        break;
      }

      case RES_XML_PACKAGE_TYPE: {
        skip_chunk_header_padding(&reader, header);
        break;
      }

      case RES_XML_TYPE_TYPE: {
        uint8_t id = read_u8(&reader);
        if (id != res_type)
          goto skip_chunk;

        (void)read_u8(&reader);
        (void)read_u16(&reader);
        uint32_t entry_count   = read_u32(&reader);
        uint32_t entries_start = read_u32(&reader);

        uint32_t type_spec_size = read_u32(&reader);
        skip(&reader, type_spec_size - 4);

        if (res_index >= entry_count)
          goto skip_chunk;

        skip(&reader, res_index * sizeof(uint32_t));
        uint32_t entry_index = read_u32(&reader);

        if (entry_index == 0xffffffff)
          goto skip_chunk;

        seek(&reader, chunk_start + entries_start + entry_index);
        // skip entry_size entry_flag entry_key
        // typed data, skip size zero data_type
        skip(&reader, 2 * sizeof(uint32_t));
        skip(&reader, 4);
        uint32_t string_index = read_u32(&reader);

        char *path = string_pool_get(pool, string_index);
        if (path) {
          char *dup_path = strdup(path);
          if (!dup_path) {
            continue;
          }
          char **new_arr =
              realloc(icons.strings, (icons.count + 1) * sizeof(char *));
          if (!new_arr) {
            free(dup_path);
            continue;
          }
          icons.strings = new_arr;

          icons.strings[icons.count++] = dup_path;
        }

        seek(&reader, chunk_start + header.size);
        break;
      }
      skip_chunk:
      default:
        seek(&reader, chunk_start + header.size);
        break;
    }
  }
  string_pool_free(&pool);

  return icons;
}

uint8_t *
get_data_from_file(zip_t *za, const char *file_name, size_t *data_size) {
  zip_stat_t sb;
  int err = zip_stat(za, file_name, 0, &sb);
  if (err == -1) {
    zip_error_t *error = zip_get_error(za);
    fprintf(stderr, "Failed to stat %s: %s\n", file_name,
            zip_error_strerror(error));
    zip_error_fini(error);
    return NULL;
  }

  zip_file_t *zf = zip_fopen(za, file_name, 0);
  if (zf == NULL) {
    zip_error_t *error = zip_get_error(za);
    fprintf(stderr, "Failed to open file for reading %s\n",
            zip_error_strerror(error));
    zip_error_fini(error);
    return NULL;
  }

  uint8_t *data = malloc(sb.size);
  zip_int64_t f = zip_fread(zf, data, sb.size);
  zip_fclose(zf);
  if (f == -1) {
    fprintf(stderr, "Failed to read file\n");
    return NULL;
  }
  *data_size = sb.size;
  return data;
}
