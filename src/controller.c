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
#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif
#include "controller.h"

#include "cmenu.h"
#include "data.h"
#include "defines.h"
#include "display.h"
#include "gps.h"
#include "menu.h"
#include "screen.h"

#include <hildon/hildon-banner.h>

struct _MapControllerPrivate
{
    MapScreen *screen;

    guint is_disposed : 1;
};

G_DEFINE_TYPE(MapController, map_controller, G_TYPE_OBJECT);

#define MAP_CONTROLLER_PRIV(controller) (MAP_CONTROLLER(controller)->priv)

static MapController *instance = NULL;

static void
map_controller_dispose(GObject *object)
{
    MapControllerPrivate *priv = MAP_CONTROLLER_PRIV(object);

    if (priv->is_disposed)
        return;

    priv->is_disposed = TRUE;

    G_OBJECT_CLASS(map_controller_parent_class)->dispose(object);
}

static void
map_controller_init(MapController *controller)
{
    MapControllerPrivate *priv;

    priv = G_TYPE_INSTANCE_GET_PRIVATE(controller, MAP_TYPE_CONTROLLER,
                                       MapControllerPrivate);
    controller->priv = priv;

    g_assert(instance == NULL);
    instance = controller;

    priv->screen = g_object_new(MAP_TYPE_SCREEN, NULL);
    map_screen_show_compass(priv->screen, _show_comprose);
    map_screen_show_scale(priv->screen, _show_scale);
    map_screen_show_zoom_box(priv->screen, _show_zoomlevel);
}

static void
map_controller_class_init(MapControllerClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(object_class, sizeof(MapControllerPrivate));

    object_class->dispose = map_controller_dispose;
}

MapController *
map_controller_get_instance()
{
    return instance;
}

GtkWidget *
map_controller_get_screen_widget(MapController *self)
{
    g_return_val_if_fail(MAP_IS_CONTROLLER(self), NULL);
    return (GtkWidget *)self->priv->screen;
}

GtkWindow *
map_controller_get_main_window(MapController *self)
{
    GtkWidget *screen;

    screen = map_controller_get_screen_widget(self);
    return GTK_WINDOW(gtk_widget_get_toplevel(screen));
}

void
map_controller_zoom_in(MapController *self)
{
    g_return_if_fail(MAP_IS_CONTROLLER(self));
    map_screen_zoom_in(self->priv->screen);
}

void
map_controller_zoom_out(MapController *self)
{
    g_return_if_fail(MAP_IS_CONTROLLER(self));
    map_screen_zoom_out(self->priv->screen);
}

void
map_controller_switch_fullscreen(MapController *self)
{
    g_return_if_fail(MAP_IS_CONTROLLER(self));
    gtk_check_menu_item_set_active
        (GTK_CHECK_MENU_ITEM(_menu_view_fullscreen_item), !_fullscreen);
}

void
map_controller_activate_menu_settings(MapController *self)
{
    g_return_if_fail(MAP_IS_CONTROLLER(self));
    gtk_menu_item_activate(GTK_MENU_ITEM(_menu_settings_item));
}

void
map_controller_action_point(MapController *self)
{
    g_return_if_fail(MAP_IS_CONTROLLER(self));
    map_screen_action_point(self->priv->screen);
}

void
map_controller_action_route(MapController *self)
{
    g_return_if_fail(MAP_IS_CONTROLLER(self));
    map_menu_route();
}

void
map_controller_action_track(MapController *self)
{
    g_return_if_fail(MAP_IS_CONTROLLER(self));
    map_menu_track();
}

void
map_controller_action_go_to(MapController *self)
{
    g_return_if_fail(MAP_IS_CONTROLLER(self));
    map_menu_go_to();
}

void
map_controller_activate_menu_point(MapController *self, gint x, gint y)
{
    Point p;
    MapArea area;

    g_return_if_fail(MAP_IS_CONTROLLER(self));
    p.unitx = x;
    p.unity = y;
    map_screen_get_tap_area_from_units(self->priv->screen, x, y, &area);
    map_menu_point(&p, &area);
}

void
map_controller_set_gps_enabled(MapController *self, gboolean enabled)
{
    _enable_gps = enabled;

    if (enabled)
        rcvr_connect();
    else
        rcvr_disconnect();

    map_move_mark();
    gps_show_info();
}

gboolean
map_controller_get_gps_enabled(MapController *self)
{
    return _enable_gps;
}

void
map_controller_set_center_mode(MapController *self, CenterMode mode)
{
    g_return_if_fail(MAP_IS_CONTROLLER(self));

    _center_mode = mode;
    if (mode > 0)
        map_refresh_mark(TRUE);
}

void
map_controller_disable_auto_center(MapController *self)
{
    if (_center_mode > 0)
    {
        map_controller_set_center_mode(self, -_center_mode);
        MACRO_BANNER_SHOW_INFO(_window, _("Auto-Center Off"));
    }
}

