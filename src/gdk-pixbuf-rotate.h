/* GdkPixbuf library - image transformation using 2x2 arbitrary matrix
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Authors: Oleg Klimov <quif@land.ru>, John Costigan
 *
 * This file is part of Maemo Mapper.
 *
 * Maemo Mapper is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Maemo Mapper is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Maemo Mapper.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GDK_PIXBUF_ROTATE_H
#define GDK_PIXBUF_ROTATE_H

#include "gdk-pixbuf/gdk-pixbuf.h"

G_BEGIN_DECLS


gfloat* gdk_pixbuf_rotate_matrix_new            ();
void gdk_pixbuf_rotate_matrix_fill_for_rotation (gfloat*       matrix,
                                                 gfloat        angle);
void gdk_pixbuf_rotate_matrix_mult_number       (gfloat*       matrix,
                                                 gfloat        mult);
void gdk_pixbuf_rotate_matrix_mult_matrix       (gfloat*       dst_matrix,
                                                 const gfloat* src1,
                                                 const gfloat* src2);
gboolean gdk_pixbuf_rotate_matrix_reverse       (gfloat*       dest,
                                                 const gfloat* src);
void gdk_pixbuf_rotate_matrix_transpose         (gfloat*       dest,
                                                 const gfloat* src);
gfloat gdk_pixbuf_rotate_matrix_determinant     (const gfloat* matrix);


void gdk_pixbuf_rotate          (GdkPixbuf*    dst,
                                 gfloat        dst_x,
                                 gfloat        dst_y,
                                 gfloat*       matrix,
                                 GdkPixbuf*    src,
                                 gfloat        src_center_x,
                                 gfloat        src_center_y,
                                 gfloat        src_width,
                                 gfloat        src_height,
                                 gint* result_rect_x,
                                 gint* result_rect_y,
                                 gint* result_rect_width,
                                 gint* result_rect_height);
void gdk_pixbuf_rotate_vector   (gfloat*        dst_x,
                                 gfloat*        dst_y,
                                 const gfloat*  matrix,
                                 gfloat         src_vector_x,
                                 gfloat         src_vector_y);

G_END_DECLS

#endif /* GDK_PIXBUF_ROTATE_H */

