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
#include "utils.h"

#include <stdlib.h>
#include <string.h>

static char *
read_string_from_pool(const uint8_t *pool_start,
                      size_t pool_size,
                      uint32_t string_index) {
  char *path = NULL;
  Buffer in  = create_buffer(pool_start, pool_size);

  // skip chunk header string_count style_count
  skip(&in, 4 * sizeof(uint32_t));
  uint32_t flags         = read_u32(&in);
  uint32_t strings_start = read_u32(&in);

  skip(&in, sizeof(uint32_t)); // skip styles_start

  // table of string indices
  skip(&in, string_index * sizeof(uint32_t));
  uint32_t string_offset = read_u32(&in);

  // get string data at string_offset
  seek(&in, strings_start + string_offset);

  if (flags & (1 << 8)) {       /* UTF8_FLAG */
    (void)read_u8(&in);         // skip length in characters
    uint8_t len = read_u8(&in); // length in bytes
    path        = malloc(len + 1);
    if (path) {
      read_raw(&in, path, len);
      path[len] = '\0';
    }
  } else { /* UTF16_FLAG */
    uint16_t len       = read_u16(&in);
    char16_t *utf16str = malloc(len * sizeof(char16_t));
    if (utf16str) {
      read_raw(&in, (char *)utf16str, len * sizeof(char16_t));
      path = utf16_to_utf8(utf16str, len);
      free(utf16str);
    }
  }
  return path;
}

uint32_t
get_application_icon_resource_reference_id(const uint8_t *data, size_t size) {
  Buffer in = create_buffer(data, size);

  uint32_t str_manifest_index    = (uint32_t)-1;
  uint32_t str_application_index = (uint32_t)-1;
  uint32_t str_icon_index        = (uint32_t)-1;
  bool in_manifest_node          = false;

  while (!at_end(&in)) {
    uint32_t chunk_start = in.pos;
    uint16_t chunk_type  = read_u16(&in);
    (void)skip(&in, sizeof(uint16_t)); // skip chunk_header_size
    uint32_t chunk_size = read_u32(&in);

    switch (chunk_type) {
      case 0x0003: { /*RES_XML_TYPE*/
        break;
      }

      case 0x0001: { /*RES_STRING_POOL_TYPE*/
        uint32_t string_count = read_u32(&in);
        skip(&in, sizeof(uint32_t)); // skip style_count
        skip(&in, sizeof(uint32_t)); // skip flags
        uint32_t strings_start = read_u32(&in);
        skip(&in,
             sizeof(uint32_t)); // skip styles_start

        uint32_t *string_offsets = malloc(string_count * sizeof(uint32_t));
        for (uint32_t i = 0; i < string_count; i++) {
          string_offsets[i] = read_u32(&in);
        }

        for (uint32_t i = 0; i < string_count; i++) {
          seek(&in, chunk_start + strings_start + string_offsets[i]);
          uint16_t len = read_u16(&in);
          if (len == 8 || len == 11 || len == 4) {
            char16_t *utf16str = malloc(len * sizeof(char16_t));
            read_raw(&in, (char *)utf16str, len * sizeof(char16_t));
            char *s = utf16_to_utf8(utf16str, len);
            if (strcmp(s, "manifest") == 0) {
              str_manifest_index = i;
            } else if (strcmp(s, "application") == 0) {
              str_application_index = i;
            } else if (strcmp(s, "icon") == 0) {
              str_icon_index = i;
            }
            free(s);
            free(utf16str);
            if (str_manifest_index != (uint32_t)-1 &&
                str_application_index != (uint32_t)-1 &&
                str_icon_index != (uint32_t)-1)
              break;
          }
        }
        free(string_offsets);
        skip(&in, chunk_size - (in.pos - chunk_start));
        break;
      }

      case 0x0102: { /*RES_XML_START_ELEMENT_TYPE*/
        skip(&in, 2 * sizeof(uint32_t));
        skip(&in,
             sizeof(uint32_t)); // skip namespace index
        uint32_t name = read_u32(&in);

        if (name == str_manifest_index)
          in_manifest_node = true;

        if (!in_manifest_node || name != str_application_index) {
          skip(&in, chunk_size - (in.pos - chunk_start));
          break;
        }

        skip(&in, 2 * sizeof(uint16_t));
        uint16_t attribute_count = read_u16(&in);
        skip(&in, 3 * sizeof(uint16_t));

        // attributes
        for (uint32_t i = 0; i < attribute_count; i++) {
          skip(&in, sizeof(uint32_t));
          uint32_t name      = read_u32(&in);
          uint32_t raw_value = read_u32(&in);
          if (name != str_icon_index) {
            // skip typed data
            skip(&in, 2 * sizeof(uint32_t));
            continue;
          }
          // typed data
          skip(&in,
               sizeof(uint16_t) + sizeof(uint8_t)); // skip size and zero
          uint8_t data_type = read_u8(&in);
          uint32_t data     = read_u32(&in);
          if (raw_value == 0xffffffff && data_type == 1 /*refernce type*/) {
            return data;
          }
        }
        break;
      }

      case 0x0103: { /*RES_XML_END_ELEMENT_TYPE*/
        skip(&in, 2 * sizeof(uint32_t));
        skip(&in, sizeof(uint32_t));
        uint32_t name = read_u32(&in);

        if (name == str_manifest_index)
          in_manifest_node = false;
        break;
      }

      default:
        skip(&in, chunk_size - 8);
        break;
    }
  }
  return 0;
}

char **
get_application_icon_resource_path(const uint8_t *data,
                                   size_t size,
                                   uint32_t reference_id) {
  uint32_t res_type          = (reference_id >> 16) & 0xff;
  uint32_t res_index         = reference_id & 0xffff;
  Buffer in                  = create_buffer(data, size);
  uint32_t string_pool_start = 0;

  char **icon_paths    = NULL;
  size_t icon_count    = 0;
  size_t icon_capacity = 0;

  while (!at_end(&in)) {
    uint32_t chunk_start       = in.pos;
    uint16_t chunk_type        = read_u16(&in);
    uint16_t chunk_header_size = read_u16(&in);
    uint32_t chunk_size        = read_u32(&in);

    switch (chunk_type) {
      case 0x0002: { /*RES_TABLE_TYPE*/
        skip(&in,
             sizeof(uint32_t)); // skip package_count
        break;
      }

      case 0x0001: { /*RES_STRING_POOL_TYPE*/
        if (string_pool_start == 0)
          string_pool_start = chunk_start;
        skip(&in, chunk_size - (in.pos - chunk_start));
        break;
      }

      case 0x0200: { /*RES_TABLE_PACKAGE_TYPE*/
        (void)read_u32(&in);
        skip(&in, 128 * sizeof(uint16_t));
        skip(&in, 4 * sizeof(uint32_t));
        if (chunk_header_size == 288) {
          // "old" size (without typeIdOffset) is
          // 284 typeIdOffset was added
          // platform_frameworks_base/@f90f2f8dc36e7243b85e0b6a7fd5a590893c827e
          // which is only in split/new
          // applications. See
          // https://github.com/iBotPeaches/Apktool/blob/29355f876dc1383bd7d7a8f0840aa36840e73063/brut.apktool/apktool-lib/src/main/java/brut/androlib/res/decoder/ARSCDecoder.java#L108
          skip(&in, sizeof(uint32_t));
        }
        break;
      }

      case 0x0201: { /*RES_TABLE_TYPE_TYPE*/
        uint8_t id = read_u8(&in);
        if (id != res_type) {
          skip(&in, chunk_size - (in.pos - chunk_start));
          break;
        }
        (void)read_u8(&in);
        (void)read_u16(&in);
        uint32_t entry_count   = read_u32(&in);
        uint32_t entries_start = read_u32(&in);
        {
          uint32_t size = read_u32(&in);
          skip(&in, size - 4);
        }
        if (res_index >= entry_count)
          goto skip_chunk;

        skip(&in, res_index * sizeof(uint32_t));
        uint32_t entry_index = read_u32(&in);

        if (entry_index == 0xffffffff)
          goto skip_chunk;

        seek(&in, chunk_start + entries_start + entry_index);
        // skip entry_size entry_flag entry_key
        // typed data, skip size zero data_type
        skip(&in, 2 * sizeof(uint32_t));
        skip(&in, sizeof(uint32_t));
        uint32_t string_index = read_u32(&in);

        char *path = read_string_from_pool(
            data + string_pool_start, size - string_pool_start, string_index);
        if (path) {
          if (icon_count + 1 >= icon_capacity) {
            icon_capacity = icon_capacity ? icon_capacity * 2 : 4;
            char **new_arr =
                realloc(icon_paths, icon_capacity * sizeof(char *));
            if (!new_arr) {
              free(path);
              break;
            }
            icon_paths = new_arr;
          }
          icon_paths[icon_count++] = path;
        }

        seek(&in, chunk_start + chunk_size);
        break;
      }
      skip_chunk:
      default:
        skip(&in, chunk_size - 8);
        break;
    }
  }

  // NULL-terminate the array
  if (icon_paths) {
    if (icon_count + 1 > icon_capacity) {
      char **new_arr = realloc(icon_paths, (icon_count + 1) * sizeof(char *));
      if (new_arr)
        icon_paths = new_arr;
    }
    if (icon_paths)
      icon_paths[icon_count] = NULL;
  }
  return icon_paths;
}

uint8_t *
get_data_from_file(zip_t *za, const char *file_name, size_t *data_size) {
  zip_stat_t sb;
  int err = zip_stat(za, file_name, 0, &sb);
  if (err == -1) {
    zip_error_t *error = zip_get_error(za);
    fprintf(
        stderr, "Failed to stat %s: %s\n", file_name, zip_error_strerror(error));
    zip_error_fini(error);
    return NULL;
  }

  zip_file_t *zf = zip_fopen(za, file_name, 0);
  if (zf == NULL) {
    zip_error_t *error = zip_get_error(za);
    fprintf(stderr,
            "Failed to open file for reading %s\n",
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
