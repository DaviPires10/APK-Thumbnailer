/* SPDX-License-Identifier: GPL-3.0-or-later
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

#include "image.h"

#include "apk.h"
#include "svg_writer.h"

#include <cairo.h>
#include <librsvg/rsvg.h>
#include <magic.h>
#include <stdlib.h>
#include <string.h>
#include <webp/decode.h>

static cairo_status_t png_read_callback(void *closure, unsigned char *data, unsigned int length) {
  BinaryReader *reader = (BinaryReader *)closure;

  if (length > reader->size - reader->pos) {
    return CAIRO_STATUS_READ_ERROR;
  }

  if (read_raw(reader, data, length) != length) {
    return CAIRO_STATUS_READ_ERROR;
  }

  return CAIRO_STATUS_SUCCESS;
}

static cairo_surface_t *load_png(const uint8_t *data, size_t size) {
  if (!data || size == 0) {
    return NULL;
  }

  BinaryReader reader   = set_buffer(data, size);
  cairo_surface_t *surf = cairo_image_surface_create_from_png_stream(png_read_callback, &reader);
  if (cairo_surface_status(surf) != CAIRO_STATUS_SUCCESS) {
    cairo_surface_destroy(surf);
    return NULL;
  }

  return surf;
}

static cairo_surface_t *load_webp(const uint8_t *data, size_t size) {
  if (!data || size == 0) {
    return NULL;
  }

  int width  = 0;
  int height = 0;

  uint8_t *rgba = WebPDecodeRGBA(data, size, &width, &height);
  if (!rgba) {
    return NULL;
  }

  cairo_surface_t *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
  if (cairo_surface_status(surf) != CAIRO_STATUS_SUCCESS) {
    WebPFree(rgba);
    cairo_surface_destroy(surf);
    return NULL;
  }

  uint8_t *cairo_data = cairo_image_surface_get_data(surf);
  int stride          = cairo_image_surface_get_stride(surf);

  for (int y = 0; y < height; ++y) {
    uint8_t *row_src   = rgba + y * width * 4;
    uint32_t *row_dest = (uint32_t *)(cairo_data + y * stride);

    for (int x = 0; x < width; ++x) {
      uint8_t r = row_src[x * 4 + 0];
      uint8_t g = row_src[x * 4 + 1];
      uint8_t b = row_src[x * 4 + 2];
      uint8_t a = row_src[x * 4 + 3];

      if (a == 0) {
        row_dest[x] = 0;
      }
      else {
        uint32_t pr = (r * a + 0x7F) / 0xFF;
        uint32_t pg = (g * a + 0x7F) / 0xFF;
        uint32_t pb = (b * a + 0x7F) / 0xFF;

        row_dest[x] = (a << 24) | (pr << 16) | (pg << 8) | pb;
      }
    }
  }

  WebPFree(rgba);
  cairo_surface_mark_dirty(surf);

  return surf;
}

static cairo_surface_t *load_svg_text(const char *data, size_t size, int target_size) {
  if (!data || size == 0 || target_size <= 0) {
    return NULL;
  }

  GError *error        = NULL;
  RsvgHandle *handle   = NULL;
  GInputStream *stream = g_memory_input_stream_new_from_data(data, size, NULL);
  if (!stream) {
    return NULL;
  }

  handle = rsvg_handle_new_from_stream_sync(stream, NULL, RSVG_HANDLE_FLAGS_NONE, NULL, &error);
  g_object_unref(stream);
  if (!handle) {
    if (error) {
      g_error_free(error);
    }
    return NULL;
  }

  cairo_surface_t *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, target_size, target_size);
  if (cairo_surface_status(surf) != CAIRO_STATUS_SUCCESS) {
    g_object_unref(handle);
    return NULL;
  }

  cairo_t *cr            = cairo_create(surf);
  RsvgRectangle viewport = {
      0,
      0,
      target_size,
      target_size,
  };
  rsvg_handle_render_document(handle, cr, &viewport, &error);
  g_object_unref(handle);
  cairo_destroy(cr);

  if (error) {
    g_error_free(error);
    cairo_surface_destroy(surf);
    return NULL;
  }

  return surf;
}

static cairo_surface_t *load_color(ResourceValue color, int target_size) {
  uint32_t c = color.data.integer;
  double r   = 0.0f;
  double g   = 0.0f;
  double b   = 0.0f;
  double a   = 1.0f;

  switch (color.type) {
    case TYPE_INT_COLOR_ARGB8:
      r = ((c >> 24) & 0xFF) / 255.0f;
      g = ((c >> 16) & 0xFF) / 255.0f;
      b = ((c >> 8) & 0xFF) / 255.0f;
      a = (c & 0xFF) / 255.0f;
      break;

    case TYPE_INT_COLOR_RGB8:
      r = ((c >> 16) & 0xFF) / 255.0f;
      g = ((c >> 8) & 0xFF) / 255.0f;
      b = (c & 0xFF) / 255.0f;
      a = 1.0f;
      break;

    case TYPE_INT_COLOR_ARGB4:
      r = ((c >> 12) & 0xF) / 15.0f;
      g = ((c >> 8) & 0xF) / 15.0f;
      b = ((c >> 4) & 0xF) / 15.0f;
      a = (c & 0xF) / 15.0f;
      break;

    case TYPE_INT_COLOR_RGB4:
      r = ((c >> 8) & 0xF) / 15.0f;
      g = ((c >> 4) & 0xF) / 15.0f;
      b = (c & 0xF) / 15.0f;
      a = 1.0f;
      break;
  }

  cairo_surface_t *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, target_size, target_size);

  if (cairo_surface_status(surf) != CAIRO_STATUS_SUCCESS) {
    cairo_surface_destroy(surf);
    return NULL;
  }

  cairo_t *cr = cairo_create(surf);
  cairo_set_source_rgba(cr, r, g, b, a);
  cairo_paint(cr);
  cairo_destroy(cr);

  return surf;
}

static char *svg_document_to_string(SvgDocument svg, size_t *out_len) {
  size_t len   = 0;
  char *buffer = NULL;
  FILE *fp     = open_memstream(&buffer, &len);
  if (!fp) {
    return NULL;
  }

  svg_write_document(fp, svg);
  fclose(fp);

  if (out_len) {
    *out_len = len;
  }

  return buffer;
}

static uint32_t get_id(XmlElement *elem, StringPool pool) {
  if (!elem) {
    return UINT32_MAX;
  }

  XmlAttribute *drawable = NULL;

  if (elem->child_count == 0) {
    drawable = xml_find_attribute(elem, pool, "drawable");
  }
  else {
    for (size_t i = 0; i < elem->child_count; ++i) {
      XmlElement *child = elem->children[i];

      if (xml_element_has_name(child, pool, "inset")) {
        drawable = xml_find_attribute(child, pool, "drawable");
      }
      else if (xml_element_has_name(child, pool, "bitmap")) {
        drawable = xml_find_attribute(child, pool, "src");
      }
    }
  }

  if (!drawable) {
    return UINT32_MAX;
  }

  return drawable->value.raw;
}

cairo_surface_t *image_load_vector(zip_t *apk,
                                   ArscTable table,
                                   XmlElement *vector,
                                   StringPool pool,
                                   int target_size) {
  if (!apk || !vector) {
    return NULL;
  }

  SvgDocument svg = svg_parse_xml(vector, pool);

  // Resolve <defs> that reference other resources
  for (size_t i = 0; i < svg.def_count; ++i) {
    SvgElement def         = {0};
    ResourceValue resolved = arsc_table_resolve(table, svg.defs[i].id, 0);

    if (resolved.type == TYPE_STRING && resolved.data.string) {
      size_t def_size   = 0;
      uint8_t *def_data = apk_extract_file(apk, resolved.data.string, &def_size);

      if (def_data) {
        StringPool def_pool = {0};
        XmlElement *def_xml = xml_parse_document(def_data, def_size, &def_pool);
        if (def_xml) {
          def = svg_parse_def(def_xml, def_pool, svg.defs[i].id);
          svg_document_add_def(&svg, def);
          xml_free_element(def_xml);
        }
        string_pool_free(&def_pool);
        free(def_data);
      }
    }
    else if (resolved.type == TYPE_INT_COLOR_ARGB8 || resolved.type == TYPE_INT_COLOR_RGB8 ||
             resolved.type == TYPE_INT_COLOR_ARGB4 || resolved.type == TYPE_INT_COLOR_RGB4) {
      def = svg_parse_color(resolved, svg.defs[i].id);
      svg_document_add_def(&svg, def);
    }
  }

  size_t svg_len        = 0;
  char *svg_str         = svg_document_to_string(svg, &svg_len);
  cairo_surface_t *surf = NULL;
  if (svg_str) {
    surf = load_svg_text(svg_str, svg_len, target_size);
    free(svg_str);
  }

  svg_free_document(&svg);

  return surf;
}

cairo_surface_t *image_load_adaptive_icon(zip_t *apk,
                                          ArscTable table,
                                          XmlElement *icon,
                                          StringPool pool,
                                          int target_size) {
  if (!apk || !icon) {
    return NULL;
  }

  cairo_surface_t *bg_surf = NULL;
  cairo_surface_t *fg_surf = NULL;

  XmlElement *bg = xml_find_child(icon, pool, "background");
  XmlElement *fg = xml_find_child(icon, pool, "foreground");

  uint32_t bg_id = get_id(bg, pool);
  uint32_t fg_id = get_id(fg, pool);

  if (bg_id == fg_id) {
    bg_id = UINT32_MAX;
  }

  if (bg && bg_id != UINT32_MAX) {
    ResourceValue bg_value = arsc_table_resolve(table, bg_id, 0);

    if (bg_value.type == TYPE_STRING && bg_value.data.string) {
      size_t bg_size   = 0;
      uint8_t *bg_data = apk_extract_file(apk, bg_value.data.string, &bg_size);
      if (bg_data) {
        bg_surf = image_load_from_data(bg_data, bg_size, apk, table, target_size);
        free(bg_data);
      }
    }
    else if (bg_value.type == TYPE_INT_COLOR_ARGB8 || bg_value.type == TYPE_INT_COLOR_RGB8 ||
             bg_value.type == TYPE_INT_COLOR_ARGB4 || bg_value.type == TYPE_INT_COLOR_RGB4) {
      bg_surf = load_color(bg_value, target_size);
    }
  }

  if (fg && fg_id != UINT32_MAX) {
    ResourceValue fg_value = arsc_table_resolve(table, fg_id, 0);

    if (fg_value.type == TYPE_STRING && fg_value.data.string) {
      size_t fg_size   = 0;
      uint8_t *fg_data = apk_extract_file(apk, fg_value.data.string, &fg_size);
      if (fg_data) {
        fg_surf = image_load_from_data(fg_data, fg_size, apk, table, target_size);
        free(fg_data);
      }
    }
  }
  else {
    if (bg_surf) {
      cairo_surface_destroy(bg_surf);
    }
    return NULL;
  }

  if (bg_surf && fg_surf) {
    cairo_surface_t *result = image_composite_surfaces(bg_surf, fg_surf);

    cairo_surface_destroy(bg_surf);
    cairo_surface_destroy(fg_surf);

    return result;
  }
  else if (fg_surf) {
    return fg_surf;
  }

  return NULL;
}

cairo_surface_t *image_load_from_data(const uint8_t *data,
                                      size_t size,
                                      zip_t *apk,
                                      ArscTable table,
                                      int target_size) {
  cairo_surface_t *result = NULL;

  if (!apk || !data || size == 0) {
    return NULL;
  }

  magic_t magic = magic_open(MAGIC_MIME_TYPE);
  if (!magic) {
    return NULL;
  }
  if (magic_load(magic, NULL) != 0) {
    magic_close(magic);
    return NULL;
  }
  const char *mime = magic_buffer(magic, data, size);

  if (mime) {
    if (strcmp(mime, "image/png") == 0) {
      cairo_surface_t *png = load_png(data, size);
      result               = image_scale_surface(png, target_size);
      if (png)
        cairo_surface_destroy(png);
    }
    else if (strcmp(mime, "image/webp") == 0) {
      cairo_surface_t *webp = load_webp(data, size);
      result                = image_scale_surface(webp, target_size);
      if (webp)
        cairo_surface_destroy(webp);
    }
    else if (strcmp(mime, "application/octet-stream") == 0) {
      StringPool pool = {0};
      XmlElement *doc = xml_parse_document(data, size, &pool);

      if (doc) {
        if (xml_element_has_name(doc, pool, "adaptive-icon")) {
          result = image_load_adaptive_icon(apk, table, doc, pool, target_size);
        }
        else if (xml_element_has_name(doc, pool, "vector")) {
          result = image_load_vector(apk, table, doc, pool, target_size);
        }
        else if (xml_element_has_name(doc, pool, "color")) {
          ResourceValue color = xml_find_attribute(doc, pool, "color")->value;
          result              = load_color(color, target_size);
        }
        xml_free_element(doc);
      }
      string_pool_free(&pool);
    }
  }

  magic_close(magic);
  return result;
}

cairo_surface_t *image_scale_surface(cairo_surface_t *surf, int target_size) {
  if (!surf || target_size <= 0) {
    return NULL;
  }
  int src_size = cairo_image_surface_get_width(surf);
  if (src_size <= 0) {
    return NULL;
  }

  cairo_surface_t *dest = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, target_size, target_size);
  if (cairo_surface_status(dest) != CAIRO_STATUS_SUCCESS) {
    cairo_surface_destroy(dest);
    return NULL;
  }

  cairo_t *cr = cairo_create(dest);
  cairo_set_source_rgba(cr, 0, 0, 0, 0);
  cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
  cairo_paint(cr);

  double scale = (double)target_size / src_size;
  cairo_scale(cr, scale, scale);
  cairo_set_source_surface(cr, surf, 0, 0);
  cairo_paint(cr);
  cairo_destroy(cr);

  return dest;
}

cairo_surface_t *image_composite_surfaces(cairo_surface_t *bottom, cairo_surface_t *top) {
  if (!bottom || !top) {
    return NULL;
  }

  int size = cairo_image_surface_get_width(bottom);
  if (size <= 0) {
    return NULL;
  }

  cairo_surface_t *dest = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, size, size);
  if (cairo_surface_status(dest) != CAIRO_STATUS_SUCCESS) {
    cairo_surface_destroy(dest);
    return NULL;
  }

  cairo_t *cr = cairo_create(dest);

  cairo_set_source_surface(cr, bottom, 0, 0);
  cairo_paint(cr);

  cairo_set_source_surface(cr, top, 0, 0);
  cairo_paint(cr);

  cairo_destroy(cr);

  return dest;
}
