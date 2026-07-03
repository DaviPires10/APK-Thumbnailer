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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef countof
#define countof(arr) sizeof((arr)) / sizeof((arr)[0])
#endif

typedef struct {
  const char *xml_name;
  const char *svg_name;
} SvgMap;

static const char *tags[] = {
    "group", "path", "clip-path", "item", "gradient",
};

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

static const SvgMap linear_grad_attrs[] = {
    {"startX", "x1"},
    {"startY", "y1"},
    {"endX",   "x2"},
    {"endY",   "y2"},
};

static const SvgMap radial_grad_attrs[] = {
    {"centerX",        "cx"},
    {"centerY",        "cy"},
    {"gradientRadius", "r" },
};

static const SvgMap item_attrs[] = {
    {"offset", "offset"      },
    {"color",  "stop-color"  },
    {"alpha",  "stop-opacity"},
};

static char *
map_get(uint32_t index, StringPool pool, size_t map_count, const SvgMap map[map_count]) {
  for (size_t i = 0; i < map_count; ++i) {
    if (index == string_pool_get_index(pool, map[i].xml_name)) {
      return strdup(map[i].svg_name);
    }
  }

  return NULL;
}

SvgTag svg_get_tag(XmlElement *elem, StringPool pool, uint32_t *tag_indices) {
  if (!elem)
    return TAG_UNKNOWN;

  if (xml_element_has_name(elem, pool, "gradient")) {
    XmlAttribute type_attr = xml_find_attribute(elem, pool, "type");
    if (type_attr.name.index != UINT32_MAX) {
      if (type_attr.data == 0)
        return TAG_LINEAR_GRADIENT;
      if (type_attr.data == 1)
        return TAG_RADIAL_GRADIENT;
      // Ignoring type 2 (sweep gradient)
    }
    return TAG_UNKNOWN;
  }

  for (size_t i = 0; i < countof(tags); i++) {
    if (elem->name.index == tag_indices[i]) {
      return (SvgTag)i;
    }
  }

  return TAG_UNKNOWN;
}

SvgAttribute svg_parse_group(XmlElement *elem, StringPool pool) {
  SvgAttribute result;
  result.name = "transform";

  char buffer[256] = {0};
  size_t len       = 0;

  typedef struct {
    float scale_x;
    float scale_y;
    float translate_x;
    float translate_y;
    float rotation;
    float pivot_x;
    float pivot_y;
  } TransformProps;

  TransformProps props = {
      .scale_x     = 1,
      .scale_y     = 1,
      .translate_x = 0,
      .translate_y = 0,
      .rotation    = 0,
      .pivot_x     = 0,
      .pivot_y     = 0,
  };

  const char *target_attrs[] = {
      "scaleX",     "scaleY",              // scale
      "translateX", "translateY",          // translate
      "rotation",   "pivotX",     "pivotY" // rotate
  };

  float *props_array = (float *)&props;
  for (size_t i = 0; i < countof(target_attrs); ++i) {
    XmlAttribute attr = xml_find_attribute(elem, pool, target_attrs[i]);
    if (attr.data != UINT32_MAX) {
      char *value    = xml_parse_attribute(attr, pool);
      props_array[i] = atof(value);
      free(value);
    }
  }

  if (props.translate_x != 0 || props.translate_y != 0) {
    len += snprintf(&buffer[len], sizeof(buffer) - len, "translate(%g, %g)", props.translate_x,
                    props.translate_y);
  }

  if (props.rotation != 0) {
    if (len > 0) {
      buffer[len++] = ' ';
    }
    if (props.pivot_x != 0 || props.pivot_y != 0) {
      len += snprintf(&buffer[len], sizeof(buffer) - len, "rotate(%g, %g, %g)", props.rotation,
                      props.pivot_x, props.pivot_y);
    } else {
      len += snprintf(&buffer[len], sizeof(buffer) - len, "rotate(%g)", props.rotation);
    }
  }

  if (props.scale_x != 1 || props.scale_y != 1) {
    if (len > 0) {
      buffer[len++] = ' ';
    }
    len +=
        snprintf(&buffer[len], sizeof(buffer) - len, "scale(%g, %g)", props.scale_x, props.scale_y);
  }

  result.data = len > 0 ? strdup(buffer) : strdup("");

  return result;
}

SvgAttribute svg_parse_attribute(XmlAttribute attr, StringPool pool, SvgTag elem_tag) {
  SvgAttribute result = {0};

  switch (elem_tag) {
    case TAG_PATH:
    case TAG_CLIP_PATH:
      result.name = map_get(attr.name.index, pool, countof(path_attrs), path_attrs);
      result.data = xml_parse_attribute(attr, pool);
      break;

    case TAG_LINEAR_GRADIENT:
      result.name = map_get(attr.name.index, pool, countof(linear_grad_attrs), linear_grad_attrs);
      result.data = xml_parse_attribute(attr, pool);
      break;

    case TAG_RADIAL_GRADIENT:
      result.name = map_get(attr.name.index, pool, countof(radial_grad_attrs), radial_grad_attrs);
      result.data = xml_parse_attribute(attr, pool);
      break;

    case TAG_ITEM:
      result.name = map_get(attr.name.index, pool, countof(item_attrs), item_attrs);
      result.data = xml_parse_attribute(attr, pool);
      break;

    default:
      break;
  }

  return result;
}

SvgElement svg_parse_element(XmlElement *elem, StringPool pool, uint32_t *tag_indices) {
  SvgElement result = {0};
  result.tag        = svg_get_tag(elem, pool, tag_indices);

  if (result.tag == TAG_GROUP) {
    if (elem->attr_count > 0) {
      result.attributes    = malloc(sizeof(SvgAttribute));
      result.attributes[0] = svg_parse_group(elem, pool);
      result.attr_count    = 1;
    }
  } else {
    result.attributes = malloc(elem->attr_count * sizeof(SvgAttribute));
    if (result.attributes) {
      for (size_t i = 0; i < elem->attr_count; ++i) {
        SvgAttribute attr = svg_parse_attribute(elem->attributes[i], pool, result.tag);
        if (attr.data != NULL) {
          result.attributes[result.attr_count++] = attr;
        }
      }
    }
  }

  if (elem->children_count > 0) {
    result.children = malloc(elem->children_count * sizeof(SvgElement *));
    if (result.children) {
      for (size_t i = 0; i < elem->children_count; ++i) {
        SvgElement *child = malloc(sizeof(SvgElement));
        if (child) {
          *child             = svg_parse_element(elem->children[i], pool, tag_indices);
          result.children[i] = child;
          result.children_count++;
        }
      }
    }
  }

  return result;
}

SvgElement svg_parse_def(XmlElement *elem, StringPool pool) {
  uint32_t tag_indices[countof(tags)];
  string_pool_get_indices_batch(pool, tags, countof(tags), tag_indices);
  return svg_parse_element(elem, pool, tag_indices);
}

void svg_document_add_def(SvgDocument *doc, SvgElement def) {
  if (!doc)
    return;

  if (doc->defs_count >= doc->defs_capacity) {
    doc->defs_capacity = doc->defs_capacity ? doc->defs_capacity * 2 : 4;
    doc->defs          = realloc(doc->defs, doc->defs_capacity * sizeof(SvgElement));
  }

  if (doc->defs) {
    doc->defs[doc->defs_count++] = def;
  }
}

SvgDocument svg_parse_xml(XmlElement *root, StringPool pool) {
  SvgDocument doc = {0};

  uint32_t tag_indices[countof(tags)];
  string_pool_get_indices_batch(pool, tags, countof(tags), tag_indices);

  XmlAttribute width       = xml_find_attribute(root, pool, "width");
  XmlAttribute height      = xml_find_attribute(root, pool, "height");
  XmlAttribute view_width  = xml_find_attribute(root, pool, "viewportWidth");
  XmlAttribute view_height = xml_find_attribute(root, pool, "viewportHeight");

  doc.ns          = "http://www.w3.org/2000/svg";
  doc.width       = atof(xml_parse_attribute(width, pool));
  doc.height      = atof(xml_parse_attribute(height, pool));
  doc.view_width  = atof(xml_parse_attribute(view_width, pool));
  doc.view_height = atof(xml_parse_attribute(view_height, pool));

  doc.vector_count = root->children_count;
  doc.vector       = realloc(doc.vector, doc.vector_count * sizeof(SvgElement));
  for (size_t i = 0; i < doc.vector_count; ++i) {
    SvgElement elem = svg_parse_element(root->children[i], pool, tag_indices);
    doc.vector[i]   = elem;
  }

  return doc;
}
