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

#include "apk.h"
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

void extract_image(MagickWand **image, char *file_name, uint8_t *data, size_t size) {
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
  } else {
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
        print_usage(argv[0]);
        exit(EXIT_FAILURE);
        break;
      default:
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
  StringPool icons         = {0};
  MagickWand *image        = NULL;
  ExceptionType wand_error = 0;
  char *wand_err_desc      = NULL;
  bool magick_initialised  = false;

  za = zip_open(in_path, ZIP_RDONLY, &err);
  if (!za) {
    zip_error_t error;
    zip_error_init_with_code(&error, err);
    fprintf(stderr, "Failed to open %s: %s\n", in_path, zip_error_strerror(&error));
    zip_error_fini(&error);
    goto cleanup;
  }

  size_t manifest_size;
  manifest_data = apk_extract_file(za, "AndroidManifest.xml", &manifest_size);
  if (!manifest_data) {
    fprintf(stderr, "Failed to read AndroidManifest.xml\n");
    goto cleanup;
  }

  // get icon_id from AndroidManifest.xml
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

  size_t resources_size;
  resources_data = apk_extract_file(za, "resources.arsc", &resources_size);
  if (!resources_data) {
    fprintf(stderr, "Failed to read resources.arsc\n");
    goto cleanup;
  }

  // get icon_paths from resources.arsc
  icons = get_resource(resources_data, resources_size, icon_id);
  if (!icons.strings) {
    fprintf(stderr, "Failed to resolve ID 0x%08X to any file paths in resources.arsc\n", icon_id);
    goto cleanup;
  }

  InitializeMagick(NULL);
  magick_initialised = true;

  for (size_t i = 0; i < icons.count; ++i) {
    char *path = icons.strings[i];
    if (!path) {
      continue;
    }

    const char *dot = strrchr(path, '.');
    if (dot && strcmp(dot, ".xml") == 0) {
      StringPool ic_laucher_pool = {0};
      size_t ic_laucher_size     = 0;
      uint8_t *ic_laucher_data   = apk_extract_file(za, path, &ic_laucher_size);
      XmlElement *ic_launcher =
          xml_parse_document(ic_laucher_data, ic_laucher_size, &ic_laucher_pool);
      XmlElement *fg         = xml_find_child(ic_launcher, ic_laucher_pool, "foreground");
      XmlAttribute *drawable = xml_find_attribute(fg, ic_laucher_pool, "drawable");
      if (!drawable) {
        continue;
      }
      char *vector_path =
          get_resource(resources_data, resources_size, drawable->value.raw).strings[0];

      const char *dot = strrchr(vector_path, '.');
      if (dot && strcmp(dot, ".xml") != 0) {
        if (i > 0) {
          path = icons.strings[i - 1];
        } else {
          path = vector_path;
        }
        goto image_proccessing;
      }

      StringPool vector_pool = {0};
      size_t vector_size     = 0;
      uint8_t *vector_data   = apk_extract_file(za, vector_path, &vector_size);
      XmlElement *vector     = xml_parse_document(vector_data, vector_size, &vector_pool);
      SvgDocument svg        = svg_parse_xml(vector, vector_pool);

      for (size_t i = 0; i < svg.def_count; ++i) {
        size_t def_size     = 0;
        StringPool def_pool = {0};
        char *def_path    = get_resource(resources_data, resources_size, svg.defs[i].id).strings[0];
        uint8_t *def_data = apk_extract_file(za, def_path, &def_size);
        XmlElement *xml_def = xml_parse_document(def_data, def_size, &def_pool);
        SvgElement def      = svg_parse_def(xml_def, def_pool, svg.defs[i].id);
        svg_document_add_def(&svg, def);
      }

      FILE *fp = fopen(out_path, "w");
      if (!fp) {
        return EXIT_FAILURE;
      }
      if (size > svg.width) {
        svg.width  = size;
        svg.height = size;
      }

      svg_write_document(fp, &svg);

      fclose(fp);
      goto cleanup;
    } else
    image_proccessing: {
      size_t icon_size;
      uint8_t *icon_data = apk_extract_file(za, path, &icon_size);
      if (!icon_data) {
        fprintf(stderr, "Failed to extract icon file from ZIP: %s\n", path);
        goto cleanup;
      }

      extract_image(&image, path, icon_data, icon_size);
      free(icon_data);
    }
  }
  string_pool_free(&icons);

  zip_close(za);
  za = NULL;

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

  if (verbose)
    printf("Thumbnail successfully written to %s\n", out_path);

cleanup:
  if (image)
    DestroyMagickWand(image);
  if (magick_initialised)
    DestroyMagick();

  if (icons.strings)
    string_pool_free(&icons);

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
