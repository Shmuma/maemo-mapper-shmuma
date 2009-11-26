/* vi: set et sw=4 ts=8 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 8 -*- */
/*
 * Copyright (C) 2009 Alberto Mardegan.
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

#ifndef MAP_SCREEN_H
#define MAP_SCREEN_H

#include <glib.h>
#include <glib-object.h>
#include <clutter-gtk/clutter-gtk.h>
#include "types.h"

G_BEGIN_DECLS

#define MAP_TYPE_SCREEN         (map_screen_get_type ())
#define MAP_SCREEN(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MAP_TYPE_SCREEN, MapScreen))
#define MAP_SCREEN_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), MAP_TYPE_SCREEN, MapScreenClass))
#define MAP_IS_SCREEN(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MAP_TYPE_SCREEN))
#define MAP_IS_SCREEN_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MAP_TYPE_SCREEN))
#define MAP_SCREEN_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MAP_TYPE_SCREEN, MapScreenClass))

typedef struct _MapScreen MapScreen;
typedef struct _MapScreenPrivate MapScreenPrivate;
typedef struct _MapScreenClass MapScreenClass;


struct _MapScreen
{
    GtkClutterEmbed parent;

    MapScreenPrivate *priv;
};

struct _MapScreenClass
{
    GtkClutterEmbedClass parent_class;
};

GType map_screen_get_type (void);

void map_screen_set_center(MapScreen *screen, gint x, gint y, gint zoom);
void map_screen_set_rotation(MapScreen *screen, gint angle);
void map_screen_zoom_in(MapScreen *screen);
void map_screen_zoom_out(MapScreen *screen);

void map_screen_show_compass(MapScreen *screen, gboolean show);
void map_screen_show_scale(MapScreen *screen, gboolean show);
void map_screen_show_zoom_box(MapScreen *screen, gboolean show);
void map_screen_show_message(MapScreen *screen, const gchar *text);

void map_screen_update_mark(MapScreen *screen);

void map_screen_action_point(MapScreen *screen);

void map_screen_clear_pois(MapScreen *self);
void map_screen_show_poi(MapScreen *self, gint x, gint y, GdkPixbuf *pixbuf);

void map_screen_get_tap_area_from_units(MapScreen *self, gint ux, gint uy,
                                        MapArea *area);

void map_screen_redraw_overlays(MapScreen *self);

G_END_DECLS
#endif /* MAP_SCREEN_H */
