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

#ifndef ARSC_H
#define ARSC_H

#include "resource_value.h"
#include "string_pool.h"

typedef struct {
  uint16_t index;
  uint32_t key;

  ResourceValue value;
} ArscEntry;
typedef struct {
  uint32_t id;

  ArscEntry *entries;
  size_t entry_count;
} ArscType;

typedef struct {
  uint8_t id;

  ArscType *types;
  size_t type_count;
  size_t types_capacity;
} ArscPackage;

typedef struct {
  StringPool global_pool;

  ArscPackage *packages;
  size_t package_count;
} ArscTable;

ArscTable parse_arsc_table(const uint8_t *data, size_t size);

ResourceValue arsc_table_resolve(ArscTable table, uint32_t id, int depth);

void arsc_table_free(ArscTable *table);

#endif
