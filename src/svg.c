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

#include "svg.h"

#include "src/string_pool.h"
#include "src/xml.h"

#include <stdio.h>

typedef enum {
  TAG_VECTOR,
  TAG_PATH,
  TAG_GROUP,
  TAG_CLIP_PATH,
  TAG_GRADIENT,
  TAG_ITEM,
  TAG_UNKNOWN,
} Tag;

#define TAG_COUNT TAG_UNKNOWN

static const struct {
  const char *xml_tag;
  const char *svg_tag;
} tag_map[] = {
    {"vector",    "svg"      },
    {"path",      "path"     },
    {"group",     "g"        },
    {"clip-path", "clip-path"},
    {"gradient",  NULL       },
    {"item",      "stop"     },
};
static const size_t tag_map_size = sizeof(tag_map) / sizeof(tag_map[0]);

static const struct {
  const char *xml_attr;
  const char *svg_attr;
} path_attrs[] = {
    {"fillColor",        "fill"             },
    {"fillType",         "fill-rule"        },
    {"pathData",         "d"                },
    {"strokeAlpha",      "stroke-opacity"   },
    {"strokeColor",      "stroke"           },
    {"strokeLineCap",    "stroke-linecap"   },
    {"strokeMiterLimit", "stroke-miterlimit"},
    {"strokeWidth",      "stroke-width"     },
};

static const struct {
  const char *xml_attr;
  const char *svg_attr;
} linear_gradient_attrs[] = {
    {"startX", "x1"},
    {"startY", "y1"},
    {"endX",   "x2"},
    {"endY",   "y2"},
};

static const struct {
  const char *xml_attr;
  const char *svg_attr;
} radial_gradient_attrs[] = {
    {"centerX",        "cx"},
    {"centerY",        "cy"},
    {"gradientRadius", "r" },
};

static void svg_parse_vector(XmlElement *element, StringPool pool, StringPool resource_pool);
static void svg_parse_path(XmlElement *element, StringPool pool, StringPool resource_pool);
static void svg_parse_group(XmlElement *element, StringPool pool, StringPool resource_pool);
static void svg_parse_gradient(XmlElement *element, StringPool pool, StringPool resource_pool);
static void svg_parse_item(XmlElement *element, StringPool pool, StringPool resource_pool);

static Tag svg_element_get_tag(XmlElement *element, uint32_t *tag_indices) {
  if (!element || !tag_indices) {
    return TAG_UNKNOWN;
  }

  for (size_t i = 0; i < tag_map_size; ++i) {
    if (element->name.index == tag_indices[i]) {
      return i;
    }
  }

  return TAG_UNKNOWN;
}
static void svg_write_tag(FILE *fp, uint32_t gradient_type, Tag element_tag) {
  if (element_tag == TAG_GRADIENT) {
    if (gradient_type == 0) {
      fprintf(fp, "linearGradient");
    } else if (gradient_type == 1) {
      fprintf(fp, "radialGradient");
    }
  } else {
    fprintf(fp, "%s", tag_map[element_tag].svg_tag);
  }
}

void svg_write_element(FILE *fp,
                       XmlElement *element,
                       StringPool pool,
                       StringPool resource_pool,
                       Tag element_tag,
                       uint32_t *tags) {

  uint32_t gradient_type = UINT32_MAX;
  if (element_tag == TAG_GRADIENT) {
    XmlAttribute type_attr = xml_find_attribute(element, pool, "type");
    gradient_type          = type_attr.data;
  }

  if (element_tag != TAG_UNKNOWN) {
    fprintf(fp, "<");
    svg_write_tag(fp, gradient_type, element_tag);
  }

  switch (element_tag) {
    case TAG_VECTOR:
      svg_parse_vector(element, pool, resource_pool);
      break;

    case TAG_PATH:
    case TAG_CLIP_PATH:
      svg_parse_path(element, pool, resource_pool);
      break;

    case TAG_GROUP:
      svg_parse_group(element, pool, resource_pool);
      break;

    case TAG_GRADIENT: {
      svg_parse_gradient(element, pool, resource_pool);
    } break;

    case TAG_ITEM:
      svg_parse_item(element, pool, resource_pool);
      break;

    default:
      return;
  }

  if (element->children_count > 0) {
    fprintf(fp, ">\n");

    for (size_t i = 0; i < element->children_count; ++i) {
      XmlElement *child = element->children[i];
      Tag child_tag     = svg_element_get_tag(child, tags);
      svg_write_element(fp, child, pool, resource_pool, child_tag, tags);
    }
    fprintf(fp, "</");
    svg_write_tag(fp, gradient_type, element_tag);
    fprintf(fp, ">\n");
  } else {
    fprintf(fp, "/>\n");
  }
}

void svg_parse_document(FILE *fp, XmlElement *root, StringPool pool, StringPool resource_pool) {
  if (!root || !fp) {
    return;
  }

  uint32_t tag_indices[tag_map_size];
  const char *tags[tag_map_size];

  for (size_t i = 0; i < tag_map_size; ++i) {
    tags[i] = tag_map[i].xml_tag;
  }
  string_pool_get_indices_batch(pool, tags, tag_map_size, tag_indices);

  Tag root_tag = svg_element_get_tag(root, tag_indices);

  svg_write_element(fp, root, pool, resource_pool, root_tag, tag_indices);
}
