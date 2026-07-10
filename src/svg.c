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

static const float RADIX_MULTS[4] = {
    1.0f / (1 << 8),
    1.0f / (1 << 15),
    1.0f / (1 << 23),
    1.0f / (1 << 31),
};

static const char *tags[] = {
    "group", "path", "clip-path", "item", "gradient",
};

static const SvgMap path_attrs[] = {
    {"pathData",         "d"                },
    {"fillColor",        "fill"             },
    {"fillType",         "fill-rule"        },
    {"strokeAlpha",      "stroke-opacity"   },
    {"strokeColor",      "stroke"           },
    {"strokeLineCap",    "stroke-linecap"   },
    {"strokeLineJoin",   "stroke-linejoin"  },
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

static const char *
map_get(uint32_t index, StringPool pool, size_t map_count, const SvgMap map[map_count]) {
  for (size_t i = 0; i < map_count; ++i) {
    if (index == string_pool_get_index(pool, map[i].xml_name)) {
      return map[i].svg_name;
    }
  }

  return NULL;
}

static SvgValue svg_parse_value(SvgDocument *doc, XmlAttribute *attr, StringPool pool) {
  SvgValue value = {0};

  if (!attr) {
    return value;
  }

  value.type    = attr->data_type;
  uint32_t data = attr->data;

  switch (value.type) {
    case TYPE_REFERENCE:
      if (doc) {
        SvgElement def = {
            .id  = data,
            .tag = TAG_UNKNOWN,
        };
        svg_document_add_def(doc, def);
      }
      value.data.integer = data;
      break;

    case TYPE_STRING: {
      char *s           = string_pool_get(pool, data);
      value.data.string = s ? strdup(s) : strdup("");
      break;
    }

    case TYPE_FLOAT: {
      float *ret = &value.data.floating;
      memcpy(ret, &data, sizeof(float));
      break;
    }
    case TYPE_INT_DEC:
    case TYPE_INT_HEX:
      value.data.integer = data;
      break;

    case TYPE_DIMENSION: {
      int radix           = (data >> 4) & 0x3;
      int mantissa        = data & 0xFFFFFF00;
      float ret           = mantissa * RADIX_MULTS[radix];
      value.data.floating = ret;
      break;
    }

    case TYPE_FRACTION: {
      int radix           = (data >> 4) & 0x3;
      int mantissa        = data & 0xFFFFFF00;
      float ret           = (mantissa * RADIX_MULTS[radix]) * 100.0f;
      value.data.floating = ret;
      break;
    }

    case TYPE_INT_BOOLEAN:
      value.data.string = data ? strdup("true") : strdup("false");
      break;

    case TYPE_INT_COLOR_ARGB8: {
      // Svg's use RGBA instead of ARGB
      uint32_t alpha     = data >> 24;
      uint32_t rgb       = data << 8;
      uint32_t ret       = rgb | alpha;
      value.data.integer = ret;
      break;
    }

    case TYPE_INT_COLOR_RGB8:
      value.data.integer = data & 0x00FFFFFF;
      break;

    case TYPE_INT_COLOR_ARGB4: {
      uint32_t alpha     = data >> 12;
      uint32_t rgb       = data << 4;
      uint32_t ret       = rgb | alpha;
      value.data.integer = ret;
      break;
    }

    case TYPE_INT_COLOR_RGB4:
      value.data.integer = data & 0x0FFF;
      break;

    default:
      value.type = TYPE_NULL;
      break;
  }

  return value;
}

static SvgTag svg_get_tag(XmlElement *elem, StringPool pool, uint32_t *tag_indices) {
  if (!elem || !tag_indices) {
    return TAG_UNKNOWN;
  }

  for (size_t i = 0; i < countof(tags); i++) {
    if (elem->name.index == tag_indices[i]) {
      return (SvgTag)i;
    }
  }

  return TAG_UNKNOWN;
}

static SvgElement
svg_parse_group(SvgDocument *doc, XmlElement *elem, StringPool pool, uint32_t *tag_indices) {
  SvgElement result = {0};
  result.tag        = TAG_GROUP;
  result.id         = UINT32_MAX;

  if (!elem) {
    return result;
  }

  if (elem->attr_count == 0) {
    goto children_append;
  }
  result.attr_count = 1;
  result.attributes = malloc(sizeof(SvgAttribute));
  if (!result.attributes) {
    return result;
  }

  SvgAttribute transform = {0};
  transform.name         = "transform";

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
    XmlAttribute *attr = xml_find_attribute(elem, pool, target_attrs[i]);
    if (attr) {
      props_array[i] = svg_parse_value(NULL, attr, pool).data.floating;
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

  transform.value.type        = TYPE_STRING;
  transform.value.data.string = len > 0 ? strdup(buffer) : strdup("");
  result.attributes[0]        = transform;

children_append:
  if (elem->children_count > 0) {
    result.children = malloc(elem->children_count * sizeof(SvgElement *));
    if (result.children) {
      for (size_t i = 0; i < elem->children_count; ++i) {
        SvgElement *child = malloc(sizeof(SvgElement));
        if (child) {
          *child             = svg_parse_element(doc, elem->children[i], pool, tag_indices);
          result.children[i] = child;
          result.children_count++;
        }
      }
    }
  }

  return result;
}

static SvgElement svg_parse_path(SvgDocument *doc, XmlElement *elem, StringPool pool) {
  SvgElement result = {0};
  result.tag        = TAG_PATH;
  result.id         = UINT32_MAX;

  if (!elem) {
    return result;
  }

  XmlAttribute *fill      = xml_find_attribute(elem, pool, "fillColor");
  XmlAttribute *fill_rule = xml_find_attribute(elem, pool, "fillType");
  XmlAttribute *linecap   = xml_find_attribute(elem, pool, "strokeLineCap");
  XmlAttribute *linejoin  = xml_find_attribute(elem, pool, "strokeLineJoin");

  const char *fill_rule_str[] = {
      "evenodd",
      "nonzero",
  };

  const char *linecap_str[] = {
      "butt",
      "round",
      "square",
  };

  const char *linejoin_str[] = {
      "miter",
      "round",
      "bevel",
  };

  if (!fill) {
    result.attributes = malloc((elem->attr_count + 1) * sizeof(SvgAttribute));
  } else {
    result.attributes = malloc(elem->attr_count * sizeof(SvgAttribute));
  }

  if (!result.attributes) {
    return result;
  }
  for (size_t i = 0; i < elem->attr_count; ++i) {
    XmlAttribute *xml_attr = &elem->attributes[i];
    if (xml_attr == fill_rule || //
        xml_attr == linecap ||   //
        xml_attr == linejoin) {
      continue;
    }

    SvgAttribute attr = {0};

    attr.name  = map_get(xml_attr->name.index, pool, countof(path_attrs), path_attrs);
    attr.value = svg_parse_value(doc, &elem->attributes[i], pool);

    result.attributes[result.attr_count++] = attr;
  }

  if (!fill) {
    SvgAttribute fill_attr = {0};

    fill_attr.name              = "fill";
    fill_attr.value.type        = TYPE_STRING;
    fill_attr.value.data.string = strdup("none");

    result.attributes[result.attr_count++] = fill_attr;
  }

  if (fill_rule) {
    SvgAttribute fill_rule_attr = {0};

    fill_rule_attr.name = map_get(fill_rule->name.index, pool, countof(path_attrs), path_attrs);
    if (fill_rule->data < countof(fill_rule_str)) {
      fill_rule_attr.value.type        = TYPE_STRING;
      fill_rule_attr.value.data.string = strdup(fill_rule_str[fill_rule->data]);
    }

    result.attributes[result.attr_count++] = fill_rule_attr;
  }

  if (linecap) {
    SvgAttribute linecap_attr = {0};

    linecap_attr.name = map_get(linecap->name.index, pool, countof(path_attrs), path_attrs);
    if (linecap->data < countof(linecap_str)) {
      linecap_attr.value.type        = TYPE_STRING;
      linecap_attr.value.data.string = strdup(linecap_str[linecap->data]);
    }

    result.attributes[result.attr_count++] = linecap_attr;
  }

  if (linejoin) {
    SvgAttribute linejoin_attr = {0};

    linejoin_attr.name = map_get(linejoin->name.index, pool, countof(path_attrs), path_attrs);
    if (linejoin->data < countof(linejoin_str)) {
      linejoin_attr.value.type        = TYPE_STRING;
      linejoin_attr.value.data.string = strdup(linejoin_str[linejoin->data]);
    }

    result.attributes[result.attr_count++] = linejoin_attr;
  }

  return result;
}

static SvgElement svg_parse_gradient(XmlElement *elem, StringPool pool) {
  SvgElement result = {0};
  result.tag        = TAG_UNKNOWN;
  result.id         = UINT32_MAX;

  if (!elem) {
    return result;
  }

  XmlAttribute *type = xml_find_attribute(elem, pool, "type");
  if (!type) {
    return result;
  }

  if (type->data == 0) {
    result.tag = TAG_LINEAR_GRADIENT;
  } else if (type->data == 1) {
    result.tag = TAG_RADIAL_GRADIENT;
  } else {
    return result;
  }

  result.attributes = malloc(elem->attr_count * sizeof(SvgAttribute));
  if (!result.attributes) {
    return result;
  }

  for (size_t i = 0; i < elem->attr_count; ++i) {
    SvgAttribute attr   = {0};
    uint32_t name_index = elem->attributes[i].name.index;

    const SvgMap *map;
    size_t map_count;
    if (result.tag == TAG_LINEAR_GRADIENT) {
      map       = linear_grad_attrs;
      map_count = countof(linear_grad_attrs);
    } else {
      map       = radial_grad_attrs;
      map_count = countof(radial_grad_attrs);
    }

    attr.name = map_get(name_index, pool, map_count, map);
    if (!attr.name) {
      continue;
    }
    attr.value = svg_parse_value(NULL, &elem->attributes[i], pool);

    result.attributes[result.attr_count++] = attr;
  }

  SvgAttribute units = {
      .name              = "gradientUnits",
      .value.type        = TYPE_STRING,
      .value.data.string = strdup("userSpaceOnUse"),
  };
  result.attributes[result.attr_count++] = units;

  SvgAttribute *tmp = realloc(result.attributes, result.attr_count * sizeof(SvgAttribute));
  if (!tmp) {
    free(units.value.data.string);
    free(result.attributes);
    result.attributes = NULL;
    return result;
  }
  result.attributes = tmp;

  if (elem->children_count > 0) {
    result.children_count = elem->children_count;
    result.children       = calloc(1, result.children_count * sizeof(SvgElement *));
    if (result.children) {
      for (size_t i = 0; i < result.children_count; ++i) {
        XmlAttribute *attrs = elem->children[i]->attributes;
        SvgElement *stop    = malloc(sizeof(SvgElement));
        if (stop) {
          stop->tag        = TAG_ITEM;
          stop->id         = UINT32_MAX;
          stop->attr_count = elem->children[i]->attr_count;
          stop->attributes = malloc(stop->attr_count * sizeof(SvgAttribute));
          if (!stop->attributes) {
            free(stop);
            continue;
          }
          for (size_t j = 0; j < stop->attr_count; ++j) {
            uint32_t name_index       = attrs[j].name.index;
            stop->attributes[j].name  = map_get(name_index, pool, countof(item_attrs), item_attrs);
            stop->attributes[j].value = svg_parse_value(NULL, &attrs[j], pool);
          }
          stop->children_count = 0;
          stop->children       = NULL;
          result.children[i]   = stop;
        }
      }
    }
  } else {
    XmlAttribute *start_color  = xml_find_attribute(elem, pool, "startColor");
    XmlAttribute *center_color = xml_find_attribute(elem, pool, "centerColor");
    XmlAttribute *end_color    = xml_find_attribute(elem, pool, "endColor");

    XmlAttribute *color_items[3] = {0};

    size_t stop_count = 0;
    if (start_color) {
      color_items[stop_count++] = start_color;
    }
    if (center_color) {
      color_items[stop_count++] = center_color;
    }
    if (end_color) {
      color_items[stop_count++] = end_color;
    }

    if (stop_count == 0) {
      return result;
    }

    result.children_count = stop_count;
    result.children       = malloc(stop_count * sizeof(SvgElement *));
    if (!result.children) {
      result.children_count = 0;
      return result;
    }
    for (size_t i = 0; i < stop_count; ++i) {
      result.children[i] = NULL;
    }

    float offset = 0.0f;
    float inc    = (stop_count > 1) ? 1.0f / (stop_count - 1) : 0.0f;

    for (size_t i = 0; i < stop_count; ++i) {
      XmlAttribute *color_attr = color_items[i];
      if (!color_attr) {
        goto cleanup;
      }

      SvgElement *stop = malloc(sizeof(SvgElement));
      if (!stop) {
        goto cleanup;
      }

      stop->tag        = TAG_ITEM;
      stop->id         = UINT32_MAX;
      stop->attr_count = 2; // offset + color
      stop->attributes = malloc(2 * sizeof(SvgAttribute));
      if (!stop->attributes) {
        free(stop);
        goto cleanup;
      }
      stop->children_count = 0;
      stop->children       = NULL;

      stop->attributes[0].name                = "offset";
      stop->attributes[0].value.type          = TYPE_FLOAT;
      stop->attributes[0].value.data.floating = offset;
      offset += inc;

      stop->attributes[1].name  = "stop-color";
      stop->attributes[1].value = svg_parse_value(NULL, color_attr, pool);

      result.children[i] = stop;
    }
    return result;

  cleanup:
    for (size_t i = 0; i < result.children_count; ++i) {
      if (result.children[i]) {
        SvgElement *stop = result.children[i];
        for (size_t j = 0; j < stop->attr_count; ++j) {
          if (stop->attributes[j].value.type == TYPE_STRING) {
            free(stop->attributes[j].value.data.string);
          }
        }
        free(stop->attributes);
        free(stop);
      }
    }
    free(result.children);
    result.children       = NULL;
    result.children_count = 0;
    return result;
  }
  return result;
}

static SvgAttribute
svg_parse_attribute(SvgDocument *doc, XmlAttribute attr, StringPool pool, SvgTag elem_tag) {
  SvgAttribute result = {0};

  switch (elem_tag) {
    case TAG_CLIP_PATH:
      result.name  = map_get(attr.name.index, pool, countof(path_attrs), path_attrs);
      result.value = svg_parse_value(doc, &attr, pool);
      break;

    default:
      break;
  }

  return result;
}

SvgElement
svg_parse_element(SvgDocument *doc, XmlElement *elem, StringPool pool, uint32_t *tag_indices) {
  SvgElement result = {0};
  result.tag        = TAG_UNKNOWN;
  result.id         = UINT32_MAX;
  if (!elem || !tag_indices) {
    return result;
  }

  result.tag = svg_get_tag(elem, pool, tag_indices);
  switch (result.tag) {
    case TAG_GROUP:
      result = svg_parse_group(doc, elem, pool, tag_indices);
      return result;

    case TAG_LINEAR_GRADIENT:
    case TAG_RADIAL_GRADIENT:
      result = svg_parse_gradient(elem, pool);
      return result;

    case TAG_PATH:
      result = svg_parse_path(doc, elem, pool);
      return result;

    default:
      result.attributes = malloc(elem->attr_count * sizeof(SvgAttribute));
      if (result.attributes) {
        for (size_t i = 0; i < elem->attr_count; ++i) {
          SvgAttribute attr = svg_parse_attribute(doc, elem->attributes[i], pool, result.tag);
          if (attr.value.type != TYPE_NULL) {
            result.attributes[result.attr_count++] = attr;
          }
        }
      }
      break;
  }

  return result;
}

SvgElement svg_parse_def(XmlElement *elem, StringPool pool, uint32_t id) {
  SvgElement result = {0};

  uint32_t tag_indices[countof(tags)];
  string_pool_get_indices_batch(pool, tags, countof(tags), tag_indices);

  result    = svg_parse_element(NULL, elem, pool, tag_indices);
  result.id = id;

  return result;
}

void svg_document_add_def(SvgDocument *doc, SvgElement def) {
  if (!doc)
    return;

  for (size_t i = 0; i < doc->defs_count; ++i) {
    if (doc->defs[i].id == def.id && doc->defs[i].tag == TAG_UNKNOWN && def.tag != TAG_UNKNOWN) {
      doc->defs[i] = def;
      return;
    }
  }

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
  if (!root) {
    return doc;
  }

  uint32_t tag_indices[countof(tags)];
  string_pool_get_indices_batch(pool, tags, countof(tags), tag_indices);

  XmlAttribute *width       = xml_find_attribute(root, pool, "width");
  XmlAttribute *height      = xml_find_attribute(root, pool, "height");
  XmlAttribute *view_width  = xml_find_attribute(root, pool, "viewportWidth");
  XmlAttribute *view_height = xml_find_attribute(root, pool, "viewportHeight");

  doc.ns          = "http://www.w3.org/2000/svg";
  doc.width       = svg_parse_value(NULL, width, pool).data.floating;
  doc.height      = svg_parse_value(NULL, height, pool).data.floating;
  doc.view_width  = svg_parse_value(NULL, view_width, pool).data.floating;
  doc.view_height = svg_parse_value(NULL, view_height, pool).data.floating;

  doc.vector_count = root->children_count;
  doc.vector       = realloc(doc.vector, doc.vector_count * sizeof(SvgElement));
  for (size_t i = 0; i < doc.vector_count; ++i) {
    SvgElement elem = svg_parse_element(&doc, root->children[i], pool, tag_indices);
    doc.vector[i]   = elem;
  }

  return doc;
}
