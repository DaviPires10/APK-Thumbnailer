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
#include "image.h"
#include "resource_value.h"
#include "string_pool.h"
#include "xml.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const struct option long_opts[] = {
    {"help",    no_argument,       0, 'h'},
    {"verbose", no_argument,       0, 'v'},
    {"input",   required_argument, 0, 'i'},
    {"output",  required_argument, 0, 'o'},
    {"size",    required_argument, 0, 's'},
    {0,         0,                 0, 0  }
};

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
  int size       = 256;
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
        if (size <= 0)
          size = 256;
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

  zip_t *apk              = NULL;
  XmlElement *manifest    = NULL;
  uint8_t *manifest_data  = NULL;
  uint8_t *resources_data = NULL;
  uint8_t *icon_data      = NULL;
  cairo_surface_t *icon   = NULL;
  char *path              = NULL;
  ArscTable resources     = {0};

  apk = zip_open(in_path, ZIP_RDONLY, &err);
  if (!apk) {
    zip_error_t error;
    zip_error_init_with_code(&error, err);
    fprintf(stderr, "Failed to open %s: %s\n", in_path, zip_error_strerror(&error));
    zip_error_fini(&error);
    goto cleanup;
  }

  size_t manifest_size = 0;
  manifest_data        = apk_extract_file(apk, "AndroidManifest.xml", &manifest_size);
  if (!manifest_data) {
    fprintf(stderr, "Failed to read AndroidManifest.xml\n");
    goto cleanup;
  }

  uint32_t icon_id = UINT32_MAX;
  {
    StringPool manifest_pool = {0};
    manifest                 = xml_parse_document(manifest_data, manifest_size, &manifest_pool);
    XmlElement *application  = xml_find_child(manifest, manifest_pool, "application");
    XmlAttribute *icon_attr  = xml_find_attribute(application, manifest_pool, "icon");
    if (!icon_attr) {
      fprintf(stderr, "Failed to find icon ID inside AndroidManifest.xml\n");
      string_pool_free(&manifest_pool);
      goto cleanup;
    }
    icon_id = icon_attr->value.raw;

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
  resources_data        = apk_extract_file(apk, "resources.arsc", &resources_size);
  if (!resources_data) {
    fprintf(stderr, "Failed to read resources.arsc\n");
    goto cleanup;
  }

  resources = parse_arsc_table(resources_data, resources_size);

  ResourceValue resolved_icon = arsc_table_resolve(resources, icon_id, 1);
  if (resolved_icon.type != TYPE_STRING || !resolved_icon.data.string) {
    fprintf(stderr, "Failed to resolve icon path from resources.arsc\n");
    goto cleanup;
  }
  path = resolved_icon.data.string;

  if (verbose) {
    printf("Resolved icon path: %s\n", path);
  }

  size_t icon_size = 0;
  icon_data        = apk_extract_file(apk, path, &icon_size);
  icon             = image_load_from_data(icon_data, icon_size, apk, resources, size);

  if (!icon) {
    fprintf(stderr, "Error loading data\n");
    goto cleanup;
  }

  cairo_status_t status = cairo_surface_write_to_png(icon, out_path);

  if (status != CAIRO_STATUS_SUCCESS) {
    fprintf(stderr, "Error saving PNG: %s\n", cairo_status_to_string(status));
    goto cleanup;
  }

  if (verbose) {
    printf("Thumbnail successfully written to %s\n", out_path);
  }

cleanup:
  arsc_table_free(&resources);
  if (resources_data)
    free(resources_data);
  if (manifest_data)
    free(manifest_data);
  if (icon_data)
    free(icon_data);
  if (icon)
    cairo_surface_destroy(icon);
  if (manifest)
    xml_free_element(manifest);
  if (apk)
    zip_close(apk);

  return 0;
}
