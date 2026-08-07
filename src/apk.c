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

#include "apk.h"

#include <stdlib.h>
#include <string.h>

uint8_t *apk_extract_file(zip_t *apk, const char *file_name, size_t *data_size) {
  if (!apk || !file_name || !data_size) {
    return NULL;
  }

  zip_stat_t stat_buf;
  int stat_err = zip_stat(apk, file_name, 0, &stat_buf);
  if (stat_err == -1) {
    zip_error_t *error = zip_get_error(apk);
    fprintf(stderr, "Failed to stat '%s': %s\n", file_name, zip_error_strerror(error));
    zip_error_fini(error);
    return NULL;
  }

  zip_file_t *zip_file = zip_fopen(apk, file_name, 0);
  if (!zip_file) {
    zip_error_t *error = zip_get_error(apk);
    fprintf(stderr, "Failed to open '%s' for reading: %s\n", file_name, zip_error_strerror(error));
    zip_error_fini(error);
    return NULL;
  }

  size_t file_size = stat_buf.size;
  uint8_t *buffer  = malloc(file_size);
  if (!buffer) {
    fprintf(stderr, "Out of memory while reading '%s'\n", file_name);
    zip_fclose(zip_file);
    return NULL;
  }

  zip_int64_t bytes_read = zip_fread(zip_file, buffer, file_size);
  zip_fclose(zip_file);

  if (bytes_read == -1) {
    fprintf(stderr, "Failed to read data from '%s'\n", file_name);
    free(buffer);
    return NULL;
  }

  if (bytes_read != file_size) {
    fprintf(stderr, "Read %zu bytes from '%s', expected %zu\n", bytes_read, file_name, file_size);
    free(buffer);
    return NULL;
  }

  *data_size = file_size;
  return buffer;
}
