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

#include <stdlib.h>
#include <string.h>

uint8_t *apk_extract_file(zip_t *za, const char *file_name, size_t *data_size) {
  zip_stat_t sb;
  int err = zip_stat(za, file_name, 0, &sb);
  if (err == -1) {
    zip_error_t *error = zip_get_error(za);
    fprintf(stderr, "Failed to stat %s: %s\n", file_name, zip_error_strerror(error));
    zip_error_fini(error);
    return NULL;
  }

  zip_file_t *zf = zip_fopen(za, file_name, 0);
  if (zf == NULL) {
    zip_error_t *error = zip_get_error(za);
    fprintf(stderr, "Failed to open file for reading %s\n", zip_error_strerror(error));
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
