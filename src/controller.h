/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
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

#ifndef MAP_CONTROLLER_H
#define MAP_CONTROLLER_H

#include "types.h"

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>
 
G_BEGIN_DECLS

#define MAP_TYPE_CONTROLLER         (map_controller_get_type ())
#define MAP_CONTROLLER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MAP_TYPE_CONTROLLER, MapController))
#define MAP_CONTROLLER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), MAP_TYPE_CONTROLLER, MapControllerClass))
#define MAP_IS_CONTROLLER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MAP_TYPE_CONTROLLER))
#define MAP_IS_CONTROLLER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MAP_TYPE_CONTROLLER))
#define MAP_CONTROLLER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MAP_TYPE_CONTROLLER, MapControllerClass))

typedef struct _MapController MapController;
typedef struct _MapControllerPrivate MapControllerPrivate;
typedef struct _MapControllerClass MapControllerClass;


struct _MapController
{
    GObject parent;

    MapControllerPrivate *priv;
};

struct _MapControllerClass
{
    GObjectClass parent_class;
};

GType map_controller_get_type (void);

MapController *map_controller_get_instance();

GtkWidget *map_controller_get_screen_widget(MapController *self);
GtkWindow *map_controller_get_main_window(MapController *self);

void map_controller_zoom_in(MapController *self);
void map_controller_zoom_out(MapController *self);
void map_controller_switch_fullscreen(MapController *self);
void map_controller_activate_menu_settings(MapController *self);
void map_controller_action_point(MapController *self);
void map_controller_action_route(MapController *self);
void map_controller_action_track(MapController *self);
void map_controller_action_go_to(MapController *self);

void map_controller_activate_menu_point(MapController *self, gint x, gint y);

void map_controller_set_gps_enabled(MapController *self, gboolean enabled);
gboolean map_controller_get_gps_enabled(MapController *self);

void map_controller_set_center_mode(MapController *self, CenterMode mode);
void map_controller_disable_auto_center(MapController *self);

void map_controller_set_auto_rotate(MapController *self, gboolean enable);
gboolean map_controller_get_auto_rotate(MapController *self);

void map_controller_set_tracking(MapController *self, gboolean enable);
gboolean map_controller_get_tracking(MapController *self);

void map_controller_set_show_compass(MapController *self, gboolean show);
gboolean map_controller_get_show_compass(MapController *self);

void map_controller_set_show_routes(MapController *self, gboolean show);
gboolean map_controller_get_show_routes(MapController *self);

void map_controller_set_show_tracks(MapController *self, gboolean show);
gboolean map_controller_get_show_tracks(MapController *self);

void map_controller_set_show_scale(MapController *self, gboolean show);
gboolean map_controller_get_show_scale(MapController *self);

void map_controller_set_show_poi(MapController *self, gboolean show);
gboolean map_controller_get_show_poi(MapController *self);

void map_controller_set_show_velocity(MapController *self, gboolean show);
gboolean map_controller_get_show_velocity(MapController *self);

void map_controller_set_show_zoom(MapController *self, gboolean show);
gboolean map_controller_get_show_zoom(MapController *self);

G_END_DECLS
#endif /* MAP_CONTROLLER_H */
