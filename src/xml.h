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

#ifndef XML_H
#define XML_H

#include "binary_reader.h"
#include "resource_value.h"
#include "string_pool.h"

#include <stddef.h>
#include <stdint.h>

typedef struct {
  struct ResStringPool_ref name;
  ResourceValue value;
} XmlAttribute;

typedef struct XmlElement {
  struct XmlElement *parent;

  struct ResStringPool_ref name;

  size_t attr_count;
  XmlAttribute *attributes;

  size_t child_count;
  size_t children_capacity;
  struct XmlElement **children;
} XmlElement;

XmlElement *xml_parse_element(BinaryReader *reader);
void xml_free_element(XmlElement *elem);
XmlElement *xml_find_child(XmlElement *elem, StringPool pool, const char *name);
bool xml_element_has_name(XmlElement *elem, StringPool pool, const char *name);
XmlElement *xml_parse_document(const uint8_t *data, size_t size, StringPool *out_pool);

XmlAttribute *xml_find_attribute(XmlElement *elem, StringPool pool, const char *name);

#endif
