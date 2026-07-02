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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

typedef enum {
  TAG_VECTOR,
  TAG_PATH,
  TAG_GROUP,
  TAG_CLIP_PATH,
  TAG_GRADIENT,
  TAG_ITEM,
  TAG_UNKNOWN,
} Tag;

typedef struct {
  const char *xml_name;
  const char *svg_name;
} SvgMap;

static const SvgMap tag_map[] = {
    {"vector",    "svg"      },
    {"path",      "path"     },
    {"group",     "g"        },
    {"clip-path", "clip-path"},
    {"gradient",  NULL       },
    {"item",      "stop"     },
};
#define TAG_COUNT (sizeof(tag_map) / sizeof(tag_map[0]))

static const SvgMap path_attrs[] = {
    {"fillColor",        "fill"             },
    {"fillType",         "fill-rule"        },
    {"pathData",         "d"                },
    {"strokeAlpha",      "stroke-opacity"   },
    {"strokeColor",      "stroke"           },
    {"strokeLineCap",    "stroke-linecap"   },
    {"strokeMiterLimit", "stroke-miterlimit"},
    {"strokeWidth",      "stroke-width"     },
};

static const SvgMap linear_gradient_attrs[] = {
    {"startX", "x1"},
    {"startY", "y1"},
    {"endX",   "x2"},
    {"endY",   "y2"},
};

static const SvgMap radial_gradient_attrs[] = {
    {"centerX",        "cx"},
    {"centerY",        "cy"},
    {"gradientRadius", "r" },
};

static const SvgMap item_attrs[] = {
    {"offset", "offset"      },
    {"color",  "stop-color"  },
    {"alpha",  "stop-opacity"},
};

static void write_attr(FILE *fp,
                       XmlElement *elem,
                       StringPool pool,
                       StringPool resource_pool,
                       const char *xml_name,
                       const char *svg_name) {
  XmlAttribute attr = xml_find_attribute(elem, pool, xml_name);
  if (attr.name.index == UINT32_MAX) {
    return;
  }
  char *value = xml_parse_attribute(attr, resource_pool);
  fprintf(fp, " %s=\"%s\"", svg_name, value);
  free(value);
}

static void write_attr_fmt(FILE *fp, const char *restrict svg_attr, const char *restrict fmt, ...) {
  va_list args;
  va_start(args, fmt);
  fprintf(fp, " %s=\"", svg_attr);
  vfprintf(fp, fmt, args);
  fprintf(fp, "\"");
  va_end(args);
}

static void write_attr_map(FILE *fp,
                           XmlElement *element,
                           StringPool pool,
                           StringPool resource_pool,
                           const size_t map_count,
                           const SvgMap map[map_count]) {

  for (size_t i = 0; i < map_count; ++i) {
    XmlAttribute attr = xml_find_attribute(element, pool, map[i].xml_name);
    if (attr.name.index == UINT32_MAX) {
      continue;
    }
    char *value = xml_parse_attribute(attr, resource_pool);
    fprintf(fp, " %s=\"%s\"", map[i].svg_name, value);
    free(value);
  }
}

static void
svg_parse_vector(FILE *fp, XmlElement *elem, StringPool pool, StringPool resource_pool) {
  fprintf(fp, " xmlns=\"http://www.w3.org/2000/svg\"");

  write_attr(fp, elem, pool, resource_pool, "width", "width");
  write_attr(fp, elem, pool, resource_pool, "height", "height");

  XmlAttribute vw = xml_find_attribute(elem, pool, "viewportWidth");
  XmlAttribute vh = xml_find_attribute(elem, pool, "viewportHeight");
  char *w         = xml_parse_attribute(vw, resource_pool);
  char *h         = xml_parse_attribute(vh, resource_pool);
  write_attr_fmt(fp, " viewBox", "0 0 %s %s", w, h);
  free(w);
  free(h);

  write_attr(fp, elem, pool, resource_pool, "alpha", "opacity");
}

static void
svg_parse_group(FILE *fp, XmlElement *element, StringPool pool, StringPool resource_pool) {
  // TODO: Handle rotation, scale, translation
}

static Tag svg_element_get_tag(XmlElement *element, uint32_t *tag_indices) {
  if (!element || !tag_indices) {
    return TAG_UNKNOWN;
  }

  for (size_t i = 0; i < TAG_COUNT; ++i) {
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
    fprintf(fp, "%s", tag_map[element_tag].svg_name);
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
      svg_parse_vector(fp, element, pool, resource_pool);
      break;

    case TAG_CLIP_PATH:
      write_attr(fp, element, pool, resource_pool, "pathData", "d");
      break;

    case TAG_PATH:
      write_attr_map(fp, element, pool, resource_pool, sizeof(path_attrs) / sizeof(path_attrs[0]),
                     path_attrs);
      break;

    case TAG_GROUP:
      svg_parse_group(fp, element, pool, resource_pool);
      break;

    case TAG_GRADIENT: {
      if (gradient_type == 0) {
        write_attr_map(fp, element, pool, resource_pool,
                       sizeof(linear_gradient_attrs) / sizeof(linear_gradient_attrs[0]),
                       linear_gradient_attrs);
      } else if (gradient_type == 1) {
        write_attr_map(fp, element, pool, resource_pool,
                       sizeof(radial_gradient_attrs) / sizeof(radial_gradient_attrs[0]),
                       radial_gradient_attrs);
      }
    } break;

    case TAG_ITEM:
      write_attr_map(fp, element, pool, resource_pool, sizeof(item_attrs) / sizeof(item_attrs[0]),
                     item_attrs);
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

  uint32_t tag_indices[TAG_COUNT];
  const char *tags[TAG_COUNT];

  for (size_t i = 0; i < TAG_COUNT; ++i) {
    tags[i] = tag_map[i].xml_name;
  }
  string_pool_get_indices_batch(pool, tags, TAG_COUNT, tag_indices);

  Tag root_tag = svg_element_get_tag(root, tag_indices);

  svg_write_element(fp, root, pool, resource_pool, root_tag, tag_indices);
}
