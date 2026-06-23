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
#include "string_pool.h"

#include <stddef.h>
#include <stdint.h>

typedef struct {
  char *ns;
  char *name;

  uint8_t data_type;
  uint32_t data;
} XmlAttribute;

typedef struct XmlElement {
  struct XmlElement *parent;

  char *ns;
  char *name;
  uint16_t attr_count;
  XmlAttribute *attributes;

  size_t children_count;
  struct XmlElement **children;
} XmlElement;

XmlElement *xml_parse_element(BinaryReader *reader, StringPool pool);
void xml_free_element(XmlElement *element);
XmlElement *xml_find_child(XmlElement *element, const char *name);
XmlElement *xml_parse_document(const uint8_t *data, size_t size);

XmlAttribute *xml_find_attribute(XmlElement *element, const char *name);

#endif
