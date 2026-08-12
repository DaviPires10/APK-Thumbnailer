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

#ifndef IMAGE_H
#define IMAGE_H

#include "arsc.h"
#include "cairo.h"
#include "xml.h"

#include <cairo.h>
#include <stddef.h>
#include <zip.h>

cairo_surface_t *image_load_vector(zip_t *apk,
                                   ArscTable table,
                                   XmlElement *vector,
                                   StringPool pool,
                                   int target_size);

cairo_surface_t *image_load_adaptive_icon(zip_t *apk,
                                          ArscTable table,
                                          XmlElement *icon,
                                          StringPool pool,
                                          int target_size);

cairo_surface_t *image_load_from_data(const uint8_t *data,
                                      size_t size,
                                      zip_t *apk,
                                      ArscTable table,
                                      int target_size);

cairo_surface_t *image_scale_surface(cairo_surface_t *surf, int target_size);

cairo_surface_t *image_composite_surfaces(cairo_surface_t *bottom, cairo_surface_t *top);

#endif
