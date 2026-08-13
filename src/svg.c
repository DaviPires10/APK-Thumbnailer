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

#include "svg.h"

#include "resource_value.h"

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
    {"pathData",         "d"                },
    {"fillColor",        "fill"             },
    {"fillAlpha",        "fill-opacity"     },
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

static const char *fill_rule_str[] = {
    "evenodd",
    "nonzero",
};

static const char *linecap_str[] = {
    "butt",
    "round",
    "square",
};

static const char *linejoin_str[] = {
    "miter",
    "round",
    "bevel",
};

static const char *
map_get(XmlAttribute *attr, StringPool pool, size_t map_count, const SvgMap map[map_count]) {
  if (!attr) {
    return NULL;
  }

  for (size_t i = 0; i < map_count; ++i) {
    if (attr->name.index == string_pool_get_index(pool, map[i].xml_name)) {
      return map[i].svg_name;
    }
  }

  return NULL;
}

static float get_float_attr(XmlElement *elem, StringPool pool, const char *name) {
  float result = 0.0f;

  XmlAttribute *attr = xml_find_attribute(elem, pool, name);
  if (attr) {
    result = attr->value.data.floating;
  }

  return result;
}

static SvgAttribute make_string_attr(const char *name, const char *value) {
  SvgAttribute attr      = {0};
  attr.name              = name;
  attr.value.type        = TYPE_STRING;
  attr.value.data.string = value ? strdup(value) : strdup("");
  return attr;
}

static SvgAttribute make_enum_attr(XmlAttribute *xml_attr,
                                   StringPool pool,
                                   size_t map_count,
                                   const SvgMap map[map_count],
                                   size_t str_count,
                                   const char *str_values[str_count]) {
  SvgAttribute attr = {0};

  if (!xml_attr) {
    return attr;
  }

  attr.name = map_get(xml_attr, pool, map_count, map);
  if (!attr.name) {
    return attr;
  }

  if (xml_attr->value.data.integer < str_count) {
    attr.value.type        = TYPE_STRING;
    attr.value.raw         = xml_attr->value.raw;
    attr.value.data.string = strdup(str_values[xml_attr->value.data.integer]);
  }

  return attr;
}

static SvgElement *create_stop_element(float offset, ResourceValue color) {
  SvgElement *stop = malloc(sizeof(SvgElement));
  if (!stop) {
    return NULL;
  }

  stop->tag        = TAG_ITEM;
  stop->id         = UINT32_MAX;
  stop->attr_count = 2;
  stop->attributes = malloc(2 * sizeof(SvgAttribute));
  if (!stop->attributes) {
    free(stop);
    return NULL;
  }

  stop->attributes[0].name                = "offset";
  stop->attributes[0].value.type          = TYPE_FLOAT;
  stop->attributes[0].value.data.floating = offset;

  stop->attributes[1].name  = "stop-color";
  stop->attributes[1].value = color;
  stop->child_count         = 0;
  stop->children            = NULL;

  return stop;
}

static SvgTag svg_get_tag(XmlElement *elem, uint32_t *tag_indices) {
  if (!elem || !tag_indices) {
    return TAG_UNKNOWN;
  }

  for (size_t i = 0; i < countof(tags); ++i) {
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

  char transform[256] = {0};
  size_t len          = 0;

  typedef struct {
    float scaleX;
    float scaleY;
    float translateX;
    float translateY;
    float rotation;
    float pivotX;
    float pivotY;
  } TransformProps;

  TransformProps props = {0};

  const char *target_attrs[] = {
      "scaleX",     "scaleY",              // scale
      "translateX", "translateY",          // translate
      "rotation",   "pivotX",     "pivotY" // rotate
  };

  float *props_array = (float *)&props;
  for (size_t i = 0; i < countof(target_attrs); ++i) {
    props_array[i] = get_float_attr(elem, pool, target_attrs[i]);
  }

  if (props.scaleX == 0.0f) {
    props.scaleX = 1.0f;
  }
  if (props.scaleY == 0.0f) {
    props.scaleY = 1.0f;
  }

  if (props.translateX != 0.0f || props.translateY != 0.0f) {
    len += snprintf(&transform[len], sizeof(transform) - len, "translate(%g, %g)", props.translateX,
                    props.translateY);
  }

  if (props.rotation != 0.0f) {
    if (len > 0) {
      transform[len++] = ' ';
    }
    if (props.pivotX != 0.0f || props.pivotY != 0.0f) {
      len += snprintf(&transform[len], sizeof(transform) - len, "rotate(%g, %g, %g)",
                      props.rotation, props.pivotX, props.pivotY);
    }
    else {
      len += snprintf(&transform[len], sizeof(transform) - len, "rotate(%g)", props.rotation);
    }
  }

  if (props.scaleX != 1.0f || props.scaleY != 1.0f) {
    if (len > 0) {
      transform[len++] = ' ';
    }
    len += snprintf(&transform[len], sizeof(transform) - len, "scale(%g, %g)", props.scaleX,
                    props.scaleY);
  }

  result.attributes[0] = make_string_attr("transform", transform);

children_append:
  if (elem->child_count > 0) {
    result.children = malloc(elem->child_count * sizeof(SvgElement *));
    if (result.children) {
      for (size_t i = 0; i < elem->child_count; ++i) {
        SvgElement *child = malloc(sizeof(SvgElement));
        if (child) {
          *child = svg_parse_element(doc, elem->children[i], pool, tag_indices);
          result.children[result.child_count++] = child;
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

  size_t attr_count = elem->attr_count;
  if (!fill) {
    attr_count++; // extra space for fill="none"
  }

  result.attributes = malloc(attr_count * sizeof(SvgAttribute));
  if (!result.attributes) {
    return result;
  }

  // let fill come before pathData
  if (!fill) {
    result.attributes[result.attr_count++] = make_string_attr("fill", "none");
  }

  for (size_t i = 0; i < elem->attr_count; ++i) {
    XmlAttribute *xml_attr = &elem->attributes[i];
    if (xml_attr == fill_rule || xml_attr == linecap || xml_attr == linejoin) {
      continue;
    }

    SvgAttribute attr = {0};

    attr.name  = map_get(xml_attr, pool, countof(path_attrs), path_attrs);
    attr.value = xml_attr->value;
    if (attr.value.type == TYPE_STRING) {
      attr.value.data.string = strdup(xml_attr->value.data.string);
    }
    else if (attr.value.type == TYPE_REFERENCE) {
      if (doc) {
        SvgElement def = {
            .id  = xml_attr->value.raw,
            .tag = TAG_UNKNOWN,
        };
        svg_document_add_def(doc, def);
      }
    }

    result.attributes[result.attr_count++] = attr;
  }

  if (fill_rule) {
    SvgAttribute attr = make_enum_attr(fill_rule, pool, countof(path_attrs), path_attrs,
                                       countof(fill_rule_str), fill_rule_str);
    if (attr.value.type != TYPE_NULL) {
      result.attributes[result.attr_count++] = attr;
    }
  }

  if (linecap) {
    SvgAttribute attr = make_enum_attr(linecap, pool, countof(path_attrs), path_attrs,
                                       countof(linecap_str), linecap_str);
    if (attr.value.type != TYPE_NULL) {
      result.attributes[result.attr_count++] = attr;
    }
  }

  if (linejoin) {
    SvgAttribute attr = make_enum_attr(linejoin, pool, countof(path_attrs), path_attrs,
                                       countof(linejoin_str), linejoin_str);
    if (attr.value.type != TYPE_NULL) {
      result.attributes[result.attr_count++] = attr;
    }
  }

  return result;
}

static SvgElement svg_parse_clip_path(XmlElement *elem, StringPool pool) {
  SvgElement result = {0};
  result.tag        = TAG_CLIP_PATH;
  result.id         = UINT32_MAX;

  XmlAttribute *path_data = xml_find_attribute(elem, pool, "pathData");
  result.attributes       = malloc(sizeof(SvgAttribute));

  if (!result.attributes) {
    return result;
  }

  result.attr_count                      = 1;
  result.attributes[0].name              = "d";
  result.attributes[0].value             = path_data->value;
  result.attributes[0].value.data.string = strdup(path_data->value.data.string);

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

  if (type->value.raw == 0) {
    result.tag = TAG_LINEAR_GRADIENT;
  }
  else if (type->value.raw == 1) {
    result.tag = TAG_RADIAL_GRADIENT;
  }
  else {
    return result;
  }

  result.attributes = malloc(elem->attr_count * sizeof(SvgAttribute));
  if (!result.attributes) {
    return result;
  }

  const SvgMap *map;
  size_t map_count;
  if (result.tag == TAG_LINEAR_GRADIENT) {
    map       = linear_grad_attrs;
    map_count = countof(linear_grad_attrs);
  }
  else {
    map       = radial_grad_attrs;
    map_count = countof(radial_grad_attrs);
  }

  for (size_t i = 0; i < elem->attr_count; ++i) {
    XmlAttribute *xml_attr = &elem->attributes[i];
    SvgAttribute attr      = {0};

    attr.name = map_get(xml_attr, pool, map_count, map);
    if (!attr.name) {
      continue;
    }
    attr.value = xml_attr->value;

    result.attributes[result.attr_count++] = attr;
  }

  result.attributes[result.attr_count++] = make_string_attr("gradientUnits", "userSpaceOnUse");

  // handle stops
  if (elem->child_count > 0) {
    result.children = calloc(elem->child_count, sizeof(SvgElement *));
    if (!result.children) {
      return result;
    }

    for (size_t i = 0; i < elem->child_count; ++i) {
      XmlElement *item = elem->children[i];

      XmlAttribute *color_attr = xml_find_attribute(item, pool, "color");
      if (!color_attr) {
        continue;
      }

      float offset        = get_float_attr(item, pool, "offset");
      ResourceValue color = color_attr->value;

      SvgElement *stop = create_stop_element(offset, color);
      if (stop) {
        result.children[result.child_count++] = stop;
      }
    }
  }
  else {
    XmlAttribute *color_items[3] = {0};
    size_t stop_count            = 0;

    XmlAttribute *start_color  = xml_find_attribute(elem, pool, "startColor");
    XmlAttribute *center_color = xml_find_attribute(elem, pool, "centerColor");
    XmlAttribute *end_color    = xml_find_attribute(elem, pool, "endColor");

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

    result.child_count = stop_count;
    result.children    = malloc(stop_count * sizeof(SvgElement *));
    if (!result.children) {
      result.child_count = 0;
      return result;
    }

    float offset = 0.0f;
    float inc    = (stop_count > 1) ? 1.0f / (stop_count - 1) : 0.0f;

    for (size_t i = 0; i < stop_count; ++i) {
      ResourceValue color = color_items[i]->value;
      SvgElement *stop    = create_stop_element(offset, color);
      if (!stop) {
        svg_free_element(&result);
        memset(&result, 0, sizeof(result));
        result.tag = TAG_UNKNOWN;
        result.id  = UINT32_MAX;
        return result;
      }
      result.children[i] = stop;
      offset += inc;
    }
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

  result.tag = svg_get_tag(elem, tag_indices);
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

    case TAG_CLIP_PATH: {
      result = svg_parse_clip_path(elem, pool);
      return result;
    }

    // TAG_ITEM is already handled by svg_parse_gradient
    default:
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

SvgElement svg_parse_color(ResourceValue color, uint32_t id) {
  SvgElement result = {0};
  result.tag        = TAG_LINEAR_GRADIENT;
  result.id         = id;

  SvgElement *stop = create_stop_element(0.0f, color);

  if (stop) {
    result.child_count = 1;
    result.children    = malloc(sizeof(SvgElement *));

    if (result.children) {
      result.children[0] = stop;
    }
    else {
      free(stop);
      result.child_count = 0;
    }
  }

  return result;
}

void svg_document_add_def(SvgDocument *doc, SvgElement def) {
  if (!doc)
    return;

  for (size_t i = 0; i < doc->def_count; ++i) {
    if (doc->defs[i].id == def.id && doc->defs[i].tag == TAG_UNKNOWN && def.tag != TAG_UNKNOWN) {
      doc->defs[i] = def;
      return;
    }
  }

  if (doc->def_count >= doc->defs_capacity) {
    doc->defs_capacity = doc->defs_capacity ? doc->defs_capacity * 2 : 4;
    doc->defs          = realloc(doc->defs, doc->defs_capacity * sizeof(SvgElement));
  }

  if (doc->defs) {
    doc->defs[doc->def_count++] = def;
  }
}

SvgDocument svg_parse_xml(XmlElement *root, StringPool pool) {
  SvgDocument doc = {0};
  if (!root) {
    return doc;
  }

  uint32_t tag_indices[countof(tags)];
  string_pool_get_indices_batch(pool, tags, countof(tags), tag_indices);

  doc.ns          = "http://www.w3.org/2000/svg";
  doc.width       = get_float_attr(root, pool, "width");
  doc.height      = get_float_attr(root, pool, "height");
  doc.view_width  = get_float_attr(root, pool, "viewportWidth");
  doc.view_height = get_float_attr(root, pool, "viewportHeight");

  doc.vector_count = root->child_count;
  if (doc.vector_count > 0) {
    doc.vector = calloc(doc.vector_count, sizeof(SvgElement));
    if (!doc.vector) {
      doc.vector_count = 0;
      return doc;
    }
  }

  for (size_t i = 0; i < doc.vector_count; ++i) {
    SvgElement elem = svg_parse_element(&doc, root->children[i], pool, tag_indices);
    doc.vector[i]   = elem;
  }

  return doc;
}

void svg_free_element(SvgElement *elem) {
  if (!elem)
    return;

  if (elem->attributes) {
    for (size_t i = 0; i < elem->attr_count; ++i) {
      SvgAttribute attr = elem->attributes[i];
      if (attr.value.type == TYPE_STRING) {
        free(attr.value.data.string);
        attr.value.data.string = NULL;
      }
    }
    free(elem->attributes);
    elem->attributes = NULL;
  }

  if (elem->children) {
    for (size_t i = 0; i < elem->child_count; ++i) {
      svg_free_element(elem->children[i]);
      elem->children[i] = NULL;
    }
    free(elem->children);
    elem->children = NULL;
  }

  elem->attr_count  = 0;
  elem->child_count = 0;
}

void svg_free_document(SvgDocument *doc) {
  if (!doc)
    return;

  if (doc->defs) {
    for (size_t i = 0; i < doc->def_count; ++i) {
      svg_free_element(&doc->defs[i]);
    }
    free(doc->defs);
    doc->defs = NULL;
  }
  doc->def_count     = 0;
  doc->defs_capacity = 0;

  if (doc->vector) {
    for (size_t i = 0; i < doc->vector_count; ++i) {
      svg_free_element(&doc->vector[i]);
    }
    free(doc->vector);
    doc->vector = NULL;
  }
  doc->vector_count = 0;
}
