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

#include "svg_writer.h"

void svg_write_attribute(FILE *fp, SvgAttribute attr) {
  const char *name    = attr.name;
  ResourceValue value = attr.value;

  switch (attr.value.type) {
    case TYPE_REFERENCE:
      fprintf(fp, "%s=\"url(#res%#08X)\"", name, value.data.integer);
      break;

    case TYPE_STRING:
      fprintf(fp, "%s=\"%s\"", name, value.data.string);
      break;

    case TYPE_FLOAT:
    case TYPE_DIMENSION:
      fprintf(fp, "%s=\"%g\"", name, value.data.floating);
      break;

    case TYPE_FRACTION:
      fprintf(fp, "%s=\"%g%%\"", name, value.data.floating);
      break;

    case TYPE_INT_DEC:
      fprintf(fp, "%s=\"%d\"", name, value.data.integer);
      break;

    case TYPE_INT_HEX:
      fprintf(fp, "%s=\"%#08X\"", name, value.data.integer);
      break;

    case TYPE_INT_BOOLEAN:
      fprintf(fp, "%s=\"%s\"", name, value.data.integer ? "true" : "false");
      break;

    case TYPE_INT_COLOR_ARGB8:
      fprintf(fp, "%s=\"#%08X\"", name, value.data.integer);
      break;

    case TYPE_INT_COLOR_RGB8:
      fprintf(fp, "%s=\"#%06X\"", name, value.data.integer);
      break;

    case TYPE_INT_COLOR_ARGB4:
      fprintf(fp, "%s=\"#%04X\"", name, value.data.integer);
      break;

    case TYPE_INT_COLOR_RGB4:
      fprintf(fp, "%s=\"#%03X\"", name, value.data.integer);
      break;

    default:
      break;
  }
}

void svg_write_element(FILE *fp, SvgElement *elem) {
  const char *name = NULL;
  switch (elem->tag) {
    case TAG_GROUP:
      name = "g";
      break;

    case TAG_PATH:
      name = "path";
      break;

    case TAG_CLIP_PATH:
      name = "clipPath";
      break;

    case TAG_ITEM:
      name = "stop";
      break;

    case TAG_LINEAR_GRADIENT:
      name = "linearGradient";
      break;

    case TAG_RADIAL_GRADIENT:
      name = "radialGradient";
      break;

    default:
      return;
  }

  fprintf(fp, "<%s", name);
  if (elem->id != UINT32_MAX) {
    fprintf(fp, " id=\"res%#08X\"", elem->id);
  }

  for (size_t i = 0; i < elem->attr_count; ++i) {
    SvgAttribute attr = elem->attributes[i];
    if (attr.value.type == TYPE_NULL || attr.name == NULL) {
      continue;
    }
    fputc(' ', fp);
    svg_write_attribute(fp, attr);
  }

  if (elem->child_count > 0) {
    fprintf(fp, ">\n");
    for (size_t i = 0; i < elem->child_count; ++i) {
      svg_write_element(fp, elem->children[i]);
    }
    fprintf(fp, "</%s>\n", name);
  }
  else {
    fprintf(fp, "/>\n");
  }
}

void svg_write_document(FILE *fp, SvgDocument doc) {
  if (!fp) {
    return;
  }
  fprintf(fp, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n");
  fprintf(fp, "<svg width=\"%g\" height=\"%g\" viewBox=\"0 0 %g %g\" xmlns=\"%s\">\n", //
          doc.width, doc.height, doc.view_width, doc.view_height, doc.ns);

  if (doc.def_count > 0) {
    fprintf(fp, "<defs>\n");

    for (size_t i = 0; i < doc.def_count; ++i) {
      svg_write_element(fp, &doc.defs[i]);
    }
    fprintf(fp, "</defs>\n");
  }
  for (size_t i = 0; i < doc.vector_count; ++i) {
    svg_write_element(fp, &doc.vector[i]);
  }

  fprintf(fp, "</svg>\n");
}
