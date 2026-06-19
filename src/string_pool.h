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

#ifndef STRING_POOL_H
#define STRING_POOL_H

#include "binary_reader.h"

typedef struct {
  char **strings;
  size_t count;
} StringPool;

struct ResStringPool_ref {
  uint32_t index;
};

StringPool parse_string_pool(BinaryReader *reader);
char *string_pool_get(StringPool pool, size_t idx);
uint32_t string_pool_get_index(StringPool pool, const char *str);
void string_pool_free(StringPool *pool);

#endif
