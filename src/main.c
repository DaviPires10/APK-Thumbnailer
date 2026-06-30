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
#include "xml.h"

#include <MagickWand/MagickWand.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>

static const struct option long_opts[] = {
    {"help",    no_argument,       0, 'h'},
    {"verbose", no_argument,       0, 'v'},
    {"input",   required_argument, 0, 'i'},
    {"output",  required_argument, 0, 'o'},
    {"size",    required_argument, 0, 's'},
    {0,         0,                 0, 0  }
};

void extract_image(MagickWand **image,
                   char *filename,
                   uint8_t *data,
                   size_t size) {
  MagickWand *icon = NewMagickWand();

  if (!MagickReadImageBlob(icon, data, size)) {
    ExceptionType wand_error;
    char *wand_err_desc = MagickGetException(icon, &wand_error);
    fprintf(stderr, "Failed to decode %s: %s\n", filename, wand_err_desc);
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
  if (za == NULL) {
    zip_error_t error;
    zip_error_init_with_code(&error, err);
    fprintf(stderr, "Failed to open %s: %s\n", in_path,
            zip_error_strerror(&error));
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

  manifest                = xml_parse_document(manifest_data, manifest_size);
  XmlElement *application = xml_find_child(manifest, "application");
  XmlAttribute icon       = xml_find_attribute(application, "icon");
  if (icon.data == UINT32_MAX) {
    fprintf(stderr, "Failed to find icon ID inside AndroidManifest.xml\n");
    goto cleanup;
  }
  icon_id = icon.data;

  xml_free_element(manifest);
  free(manifest_data);
  manifest      = NULL;
  manifest_data = NULL;

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
    fprintf(stderr,
            "Failed to resolve ID 0x%08X to any file paths in resources.arsc\n",
            icon_id);
    goto cleanup;
  }

  free(resources_data);
  resources_data = NULL;

  MagickWandGenesis();
  magick_initialised = true;

  for (size_t i = 0; i < icons.count; ++i) {
    char *path = icons.strings[i];
    if (!path) {
      continue;
    }
    const char *dot = strrchr(path, '.');
    if (dot && strcmp(dot, ".xml") == 0) {
      if (verbose)
        printf("Skipped XML file: %s\n", path);
      continue;
    }

    if (verbose)
      printf("Found Image Asset: %s\n", path);

    size_t icon_size;
    uint8_t *icon_data = apk_extract_file(za, path, &icon_size);
    if (!icon_data) {
      fprintf(stderr, "Failed to extract icon file from ZIP: %s\n", path);
      goto cleanup;
    }

    extract_image(&image, path, icon_data, icon_size);
    free(icon_data);
  }
  string_pool_free(&icons);

  zip_close(za);
  za = NULL;

  if (!image) {
    fprintf(stderr,
            "Failed to load any valid non-XML thumbnail image formats.\n");
    goto cleanup;
  }

  MagickSetFormat(image, "PNG");
  if (size > 0) {
    MagickResizeImage(image, size, size, LanczosFilter);
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
    MagickWandTerminus();

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
