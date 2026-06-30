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

#include "xml.h"

#include "binary_reader.h"
#include "chunk.h"
#include "string_pool.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum : uint8_t {
  TYPE_NULL              = 0x00,
  TYPE_REFERENCE         = 0x01,
  TYPE_ATTRIBUTE         = 0x02,
  TYPE_STRING            = 0x03,
  TYPE_FLOAT             = 0x04,
  TYPE_DIMENSION         = 0x05,
  TYPE_FRACTION          = 0x06,
  TYPE_DYNAMIC_REFERENCE = 0x07,
  TYPE_DYNAMIC_ATTRIBUTE = 0x08,
  TYPE_INT_DEC           = 0x10,
  TYPE_INT_HEX           = 0x11,
  TYPE_INT_BOOLEAN       = 0x12,
  TYPE_INT_COLOR_ARGB8   = 0x1c,
  TYPE_INT_COLOR_RGB8    = 0x1d,
  TYPE_INT_COLOR_ARGB4   = 0x1e,
  TYPE_INT_COLOR_RGB4    = 0x1f,
};

struct ResTable_ref {
  uint32_t ident;
};

struct Res_value {
  uint16_t size;
  uint8_t res0;

  uint8_t data_type;
  uint32_t data;
};

struct ResXMLTree_attrExt {
  struct ResStringPool_ref ns;
  struct ResStringPool_ref name;

  uint16_t attr_start;
  uint16_t attr_size;
  uint16_t attr_count;
};

struct ResXMLTree_attribute {
  struct ResStringPool_ref ns;
  struct ResStringPool_ref name;

  struct ResStringPool_ref raw_value;
  struct Res_value typed_value;
};

static const float RADIX_MULTS[4] = {
    1.0f / (1 << 8),
    1.0f / (1 << 15),
    1.0f / (1 << 23),
    1.0f / (1 << 31),
};

static void xml_add_child(XmlElement *parent, XmlElement *child) {
  if (!parent || !child) {
    return;
  }

  if (parent->children_count >= parent->children_capacity) {
    parent->children_capacity = parent->children_capacity ? parent->children_capacity * 2 : 8;
    parent->children = realloc(parent->children, parent->children_capacity * sizeof(XmlElement *));
  }

  if (parent->children) {
    parent->children[parent->children_count++] = child;
  }
}

XmlElement *xml_parse_element(BinaryReader *reader) {
  XmlElement *result = calloc(1, sizeof(XmlElement));

  if (!result) {
    return NULL;
  };

  struct ResXMLTree_attrExt node;
  node.ns.index   = read_u32(reader);
  node.name.index = read_u32(reader);
  node.attr_start = read_u16(reader);
  node.attr_size  = read_u16(reader);
  node.attr_count = read_u16(reader);

  // Skip id_index, class_index, style_index
  skip(reader, 3 * sizeof(uint16_t));

  if (node.attr_count > 0) {
    result->attributes = malloc(node.attr_count * sizeof(XmlAttribute));
    if (!result->attributes) {
      free(result);
      return NULL;
    }
  }
  for (size_t i = 0; i < node.attr_count; ++i) {
    struct ResXMLTree_attribute attr;
    attr.ns.index              = read_u32(reader);
    attr.name.index            = read_u32(reader);
    attr.raw_value.index       = read_u32(reader);
    attr.typed_value.size      = read_u16(reader);
    attr.typed_value.res0      = read_u8(reader);
    attr.typed_value.data_type = read_u8(reader);
    attr.typed_value.data      = read_u32(reader);

    result->attributes[i].name.index = attr.name.index;
    result->attributes[i].data_type  = attr.typed_value.data_type;
    result->attributes[i].data       = attr.typed_value.data;
  }

  result->name.index = node.name.index;
  result->attr_count = node.attr_count;

  return result;
}

XmlElement *xml_parse_document(const uint8_t *data, size_t size, StringPool *out_pool) {
  BinaryReader reader = set_buffer(data, size);

  XmlElement *root = NULL;
  StringPool pool  = {0};

  XmlElement **stack = NULL;
  size_t stack_top   = 0;
  size_t stack_cap   = 0;

  while (!at_end(&reader)) {
    uint32_t chunk_start   = reader.pos;
    ResChunk_header header = read_chunk_header(&reader);

    switch (header.type) {
      case RES_XML_TYPE: {
        break;
      }

      case RES_XML_STRING_POOL_TYPE: {
        if (pool.count == 0) {
          pool = parse_string_pool(&reader, chunk_start);
        }
        goto next_chunk;
      }

      case RES_XML_START_TAG_TYPE: {
        skip(&reader, 8); // skip line number and comment
        XmlElement *elem = xml_parse_element(&reader);

        if (!elem) {
          goto next_chunk;
        }

        if (stack_top == 0) {
          root = elem;
        } else {
          elem->parent = stack[stack_top - 1];
          xml_add_child(elem->parent, elem);
        }

        if (stack_top >= stack_cap) {
          stack_cap        = stack_cap ? stack_cap * 2 : 8;
          XmlElement **tmp = realloc(stack, stack_cap * sizeof(XmlElement *));
          if (!tmp) {
            string_pool_free(&pool);
            xml_free_element(root);
            free(stack);
            return NULL;
          }
          stack = tmp;
        }
        stack[stack_top++] = elem;

        goto next_chunk;
      }

      case RES_XML_END_TAG_TYPE: {
        if (stack_top > 0) {
          --stack_top;
        }
        goto next_chunk;
      }

      next_chunk:
      default:
        skip_chunk(&reader, chunk_start, header);
        break;
    }
  }
  free(stack);

  if (out_pool) {
    *out_pool = pool;
  } else {
    string_pool_free(&pool);
  }

  return root;
}

XmlElement *xml_find_child(XmlElement *element, StringPool pool, const char *name) {
  if (!element || !name) {
    return NULL;
  }

  for (size_t i = 0; i < element->children_count; ++i) {
    XmlElement *child = element->children[i];
    char *child_name  = string_pool_get(pool, child->name.index);
    if (!child_name) {
      continue;
    }
    if (strcmp(child_name, name) == 0) {
      return child;
    }
  }

  return NULL;
}

void xml_free_element(XmlElement *elem) {
  if (!elem)
    return;

  if (elem->attributes) {
    free(elem->attributes);
  }

  for (size_t i = 0; i < elem->children_count; i++) {
    xml_free_element(elem->children[i]);
  }
  free(elem->children);
  free(elem);
}

XmlAttribute xml_find_attribute(XmlElement *element, StringPool pool, const char *name) {
  XmlAttribute result = {
      .name.index = UINT32_MAX,
      .data_type  = TYPE_NULL,
      .data       = UINT32_MAX,
  };

  if (!element || !name) {
    return result;
  }

  for (size_t i = 0; i < element->attr_count; ++i) {
    XmlAttribute attr = element->attributes[i];
    char *attr_name   = string_pool_get(pool, attr.name.index);
    if (!attr_name) {
      continue;
    }
    if (strcmp(attr_name, name) == 0) {
      result = element->attributes[i];
      break;
    }
  }

  return result;
}

char *xml_parse_attribute(XmlAttribute attr, StringPool pool) {
  uint8_t type  = attr.data_type;
  uint32_t data = attr.data;
  char result[64];

  switch (type) {
    case TYPE_REFERENCE:
      snprintf(result, sizeof(result), "@res%#08X", data);
      break;

    case TYPE_STRING: {
      char *s = string_pool_get(pool, data);
      if (s) {
        return strdup(s);
      } else {
        return strdup("");
      }
      break;
    }

    case TYPE_FLOAT: {
      float ret;
      memcpy(&ret, &data, sizeof(ret));
      sprintf(result, "%g", ret);
      break;
    }

    case TYPE_INT_DEC:
      sprintf(result, "%d", data);
      break;

    case TYPE_INT_HEX:
      sprintf(result, "%#X", data);
      break;

    case TYPE_INT_BOOLEAN:
      sprintf(result, data ? "true" : "false");
      break;

    case TYPE_DIMENSION: {
      int radix = (data >> 4) & 0x3;
      int value = (int)(data >> 8);
      float ret = value * RADIX_MULTS[radix];
      snprintf(result, sizeof(result), "%g", ret);
      break;
    }

    case TYPE_FRACTION: {
      int radix = (data >> 4) & 0x3;
      int value = (int)(data >> 8);
      float ret = (value * RADIX_MULTS[radix]) * 100.0f;
      snprintf(result, sizeof(result), "%g%%", ret);
      break;
    }

    case TYPE_INT_COLOR_ARGB8: {
      // Svg's use RGBA instead of ARGB
      uint32_t alpha = data >> 6;
      uint32_t rgb   = data << 2;
      uint32_t ret   = rgb | alpha;
      sprintf(result, "#%08X", ret);
      break;
    }

    case TYPE_INT_COLOR_RGB8:
      sprintf(result, "#%06X", data & 0x00FFFFFF);
      break;

    default:
      return strdup("");
  }

  return strdup(result);
}
