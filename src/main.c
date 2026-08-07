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

#include "apk.h"
#include "arsc.h"
#include "resource_value.h"
#include "string_pool.h"
#include "svg.h"
#include "svg_writer.h"
#include "xml.h"

#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <wand/magick_wand.h>

static const struct option long_opts[] = {
    {"help",    no_argument,       0, 'h'},
    {"verbose", no_argument,       0, 'v'},
    {"input",   required_argument, 0, 'i'},
    {"output",  required_argument, 0, 'o'},
    {"size",    required_argument, 0, 's'},
    {0,         0,                 0, 0  }
};

void extract_image(MagickWand **image, const char *file_name, uint8_t *data, size_t size) {
  MagickWand *icon = NewMagickWand();

  if (!MagickReadImageBlob(icon, data, size)) {
    ExceptionType wand_error;
    char *wand_err_desc = MagickGetException(icon, &wand_error);
    fprintf(stderr, "Failed to decode %s: %s\n", file_name, wand_err_desc);
    MagickRelinquishMemory(wand_err_desc);
    DestroyMagickWand(icon);
    return;
  }

  size_t icon_w = MagickGetImageWidth(icon);
  size_t icon_h = MagickGetImageHeight(icon);

  size_t image_w = *image ? MagickGetImageWidth(*image) : 0;
  size_t image_h = *image ? MagickGetImageHeight(*image) : 0;

  if (!*image || icon_w * icon_h > image_w * image_h) {
    if (*image)
      DestroyMagickWand(*image);
    *image = icon;
  }
  else {
    DestroyMagickWand(icon);
  }
}

void print_usage(const char *progname) {
  fprintf(stderr,
          "Usage: %s -i input -o output -s size [-v] [--help]\n"
          "  -i, --input input.apk   : path to APK file\n"
          "  -o, --output output.png : output image path\n"
          "  -s, --size size         : requested thumbnail size (square)\n"
          "  -v, --verbose           : verbose output\n"
          "  -h, --help              : show this help\n",
          progname);
}

int main(int argc, char **argv) {
  int opt;
  int err;

  bool verbose   = false;
  int size       = -1;
  char *in_path  = NULL;
  char *out_path = NULL;

  while ((opt = getopt_long(argc, argv, "vi:o:s:h", long_opts, NULL)) != -1) {
    switch (opt) {
      case 'v':
        verbose = true;
        break;
      case 'i':
        in_path = optarg;
        break;
      case 'o':
        out_path = optarg;
        break;
      case 's':
        size = atoi(optarg);
        break;
      case 'h':
        print_usage(argv[0]);
        exit(EXIT_SUCCESS);
        break;
      case '?':
      default:
        print_usage(argv[0]);
        exit(EXIT_FAILURE);
        break;
    }
  }

  if (!in_path || !out_path) {
    print_usage(argv[0]);
    exit(EXIT_FAILURE);
  }

  zip_t *za                = NULL;
  XmlElement *manifest     = NULL;
  uint8_t *manifest_data   = NULL;
  uint8_t *resources_data  = NULL;
  MagickWand *image        = NULL;
  ExceptionType wand_error = 0;
  char *wand_err_desc      = NULL;
  bool magick_initialised  = false;
  char *path               = NULL;

  za = zip_open(in_path, ZIP_RDONLY, &err);
  if (!za) {
    zip_error_t error;
    zip_error_init_with_code(&error, err);
    fprintf(stderr, "Failed to open %s: %s\n", in_path, zip_error_strerror(&error));
    zip_error_fini(&error);
    goto cleanup;
  }

  size_t manifest_size = 0;
  manifest_data        = apk_extract_file(za, "AndroidManifest.xml", &manifest_size);
  if (!manifest_data) {
    fprintf(stderr, "Failed to read AndroidManifest.xml\n");
    goto cleanup;
  }

  uint32_t icon_id = UINT32_MAX;
  {
    StringPool manifest_pool = {0};
    manifest                 = xml_parse_document(manifest_data, manifest_size, &manifest_pool);
    XmlElement *application  = xml_find_child(manifest, manifest_pool, "application");
    XmlAttribute *icon       = xml_find_attribute(application, manifest_pool, "icon");
    if (!icon) {
      fprintf(stderr, "Failed to find icon ID inside AndroidManifest.xml\n");
      goto cleanup;
    }
    icon_id = icon->value.raw;

    string_pool_free(&manifest_pool);
    xml_free_element(manifest);
    free(manifest_data);
    manifest      = NULL;
    manifest_data = NULL;
  }

  if (verbose) {
    printf("Found target Icon Reference ID: %#X\n", icon_id);
  }

  size_t resources_size = 0;
  resources_data        = apk_extract_file(za, "resources.arsc", &resources_size);
  if (!resources_data) {
    fprintf(stderr, "Failed to read resources.arsc\n");
    goto cleanup;
  }
  ArscTable resources = parse_arsc_table(resources_data, resources_size);

  ResourceValue resolved_icon = arsc_table_resolve(resources, icon_id, 1);
  char *icon_path             = resolved_icon.data.string;
  if (!icon_path) {
    fprintf(stderr, "Failed to resolve icon path from resources.arsc\n");
    goto cleanup;
  }

  if (verbose) {
    printf("Resolved icon path: %s\n", icon_path);
  }

  InitializeMagick(NULL);
  magick_initialised = true;

  const char *dot = strrchr(icon_path, '.');
  if (dot && strcmp(dot, ".xml") == 0) {
    size_t ic_laucher          = 0;
    StringPool ic_laucher_pool = {0};
    uint8_t *ic_launcher_data  = apk_extract_file(za, icon_path, &ic_laucher);
    if (!ic_launcher_data) {
      fprintf(stderr, "Failed to extract adaptive icon XML: %s\n", icon_path);
      goto cleanup;
    }

    XmlElement *ic_launcher = xml_parse_document(ic_launcher_data, ic_laucher, &ic_laucher_pool);
    XmlElement *fg          = xml_find_child(ic_launcher, ic_laucher_pool, "foreground");
    XmlAttribute *drawable  = NULL;
    if (fg->child_count > 0) {
      XmlElement *inset = fg->children[0];
      drawable          = xml_find_attribute(inset, ic_laucher_pool, "drawable");
      if (!drawable) {
        drawable = xml_find_attribute(inset, ic_laucher_pool, "src");
      }
    }
    else {
      drawable = xml_find_attribute(fg, ic_laucher_pool, "drawable");
    }

    if (drawable) {
      if (verbose) {
        printf("Found foreground Reference ID: %#X\n", drawable->value.raw);
      }

      ResourceValue resolved_fg = arsc_table_resolve(resources, drawable->value.raw, 1);
      char *vector_path         = resolved_fg.data.string;

      if (vector_path) {
        if (verbose) {
          printf("Resolved foreground path: %s\n", vector_path);
        }
        const char *v_dot = strrchr(vector_path, '.');
        if (v_dot && strcmp(v_dot, ".xml") == 0) {
          size_t vector_size      = 0;
          StringPool vector_pool  = {0};
          uint8_t *vector_data    = apk_extract_file(za, vector_path, &vector_size);
          XmlElement *vector_elem = xml_parse_document(vector_data, vector_size, &vector_pool);
          SvgDocument svg         = svg_parse_xml(vector_elem, vector_pool);

          for (size_t i = 0; i < svg.def_count; ++i) {
            size_t def_size            = 0;
            StringPool def_pool        = {0};
            ResourceValue resolved_def = arsc_table_resolve(resources, svg.defs[i].id, 1);
            char *def_path             = resolved_def.data.string;
            if (def_path) {
              uint8_t *def_data   = apk_extract_file(za, def_path, &def_size);
              XmlElement *xml_def = xml_parse_document(def_data, def_size, &def_pool);
              SvgElement def      = svg_parse_def(xml_def, def_pool, svg.defs[i].id);
              svg_document_add_def(&svg, def);
              free(def_data);
            }
          }

          FILE *fp = fopen(out_path, "w");
          if (fp) {
            if (size > svg.width) {
              svg.width  = size;
              svg.height = size;
            }
            svg_write_document(fp, &svg);
            fclose(fp);
          }

          if (verbose) {
            printf("Thumbnail successfully written to %s\n", out_path);
          }

          string_pool_free(&vector_pool);
          xml_free_element(vector_elem);
          free(vector_data);
          string_pool_free(&ic_laucher_pool);
          xml_free_element(ic_launcher);
          free(ic_launcher_data);

          goto cleanup;
        }
        else {
          path = vector_path;

          string_pool_free(&ic_laucher_pool);
          xml_free_element(ic_launcher);
          free(ic_launcher_data);

          goto image_processing;
        }
      }
    }
    string_pool_free(&ic_laucher_pool);
    xml_free_element(ic_launcher);
    free(ic_launcher_data);
  }
  else {
    path = icon_path;
    goto image_processing;
  }

image_processing: {
  if (!path) {
    fprintf(stderr, "No valid icon path specified for image processing.\n");
    goto cleanup;
  }

  size_t icon_size   = 0;
  uint8_t *icon_data = apk_extract_file(za, path, &icon_size);
  if (!icon_data) {
    fprintf(stderr, "Failed to extract icon file from ZIP: %s\n", path);
    goto cleanup;
  }
  extract_image(&image, path, icon_data, icon_size);
  free(icon_data);
}

  if (!image) {
    fprintf(stderr, "Failed to load any valid non-XML thumbnail image formats.\n");
    goto cleanup;
  }

  MagickSetFormat(image, "PNG");
  if (size > 0) {
    MagickResizeImage(image, size, size, LanczosFilter, 1.0f);
    PixelWand *p_wand = NewPixelWand();
    PixelSetColor(p_wand, "none");

    MagickSetImageBackgroundColor(image, p_wand);
    MagickExtentImage(image, size, size, 0, 0);

    DestroyPixelWand(p_wand);
  }

  if (!MagickWriteImage(image, out_path)) {
    wand_err_desc = MagickGetException(image, &wand_error);
    fprintf(stderr, "Failed to write %s: %s\n", out_path, wand_err_desc);
    MagickRelinquishMemory(wand_err_desc);
    goto cleanup;
  }

  if (verbose) {
    printf("Thumbnail successfully written to %s\n", out_path);
  }

cleanup:
  arsc_table_free(&resources);
  if (image)
    DestroyMagickWand(image);
  if (magick_initialised)
    DestroyMagick();
  if (resources_data)
    free(resources_data);
  if (manifest_data)
    free(manifest_data);
  if (manifest)
    xml_free_element(manifest);
  if (za)
    zip_close(za);

  return (image && !wand_error) ? 0 : 1;
}
