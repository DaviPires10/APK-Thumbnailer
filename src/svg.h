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

#ifndef SVG_H
#define SVG_H

#include "xml.h"

#include <stddef.h>

typedef enum {
  TAG_GROUP,
  TAG_PATH,
  TAG_CLIP_PATH,
  TAG_ITEM,
  TAG_LINEAR_GRADIENT,
  TAG_RADIAL_GRADIENT,
  TAG_UNKNOWN,
} SvgTag;

typedef struct {
  const char *name;
  ResourceValue value;
} SvgAttribute;

typedef struct SvgElement {
  SvgTag tag;
  uint32_t id;

  size_t attr_count;
  SvgAttribute *attributes;

  size_t child_count;
  struct SvgElement **children;
} SvgElement;

typedef struct {
  const char *ns;

  float width;
  float height;

  float view_width;
  float view_height;

  size_t def_count;
  size_t defs_capacity;
  SvgElement *defs;

  size_t vector_count;
  SvgElement *vector;
} SvgDocument;

SvgElement
svg_parse_element(SvgDocument *doc, XmlElement *elem, StringPool pool, uint32_t *tag_indices);
SvgDocument svg_parse_xml(XmlElement *root, StringPool pool);
SvgElement svg_parse_def(XmlElement *elem, StringPool pool, uint32_t id);
void svg_document_add_def(SvgDocument *doc, SvgElement def);
void svg_free_element(SvgElement *elem);
void svg_free_document(SvgDocument *doc);

#endif
