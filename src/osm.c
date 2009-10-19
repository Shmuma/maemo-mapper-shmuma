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
#include "osm.h"

#include "controller.h"
#include "defines.h"
#include <clutter-gtk/clutter-gtk.h>

/* seconds to wait before hiding the menu */
#define HIDE_TIMEOUT    5

/* number of buttons per row */
#define N_BUTTONS_ROW   4
#define BUTTON_SIZE_X   72
#define BUTTON_SIZE_Y   72
#define BUTTON_BORDER_OFFSET    8
#define BUTTON_X_POS    ((BUTTON_SIZE_X / 2) + BUTTON_BORDER_OFFSET)

enum
{
    PROP_0,
    PROP_WIDGET,
};

struct _MapOsmPrivate
{
    /* widget used for loading the icons */
    GtkWidget *widget;

    /* the two rows of buttons, accessible in a vector or by name */
    union {
        ClutterActor *v[N_BUTTONS_ROW * 2];
        struct {
            /* this must be kept in sync with the btn_icons array */
            ClutterActor *settings;
            ClutterActor *btn1;
            ClutterActor *btn2;
            ClutterActor *btn3;
            ClutterActor *zoom_in;
            ClutterActor *zoom_out;
            ClutterActor *rotate;
            ClutterActor *fullscreen;
        } n;
    } btn;

    guint id_hide_timeout;

    guint is_disposed : 1;
};

G_DEFINE_TYPE(MapOsm, map_osm, CLUTTER_TYPE_GROUP);

#define MAP_OSM_PRIV(osm) (MAP_OSM(osm)->priv)

static const gchar *btn_icons[N_BUTTONS_ROW * 2] = {
    "maemo-mapper-settings",
    "maemo-mapper-add-point",
    NULL,
    NULL,
    "maemo-mapper-zoom-in",
    "maemo-mapper-zoom-out",
    "maemo-mapper-rotate",
    "maemo-mapper-fullscreen",
};

static gboolean
hide_timeout_cb(MapOsm *self)
{
    MapOsmPrivate *priv = self->priv;

    g_debug("%s called", G_STRFUNC);
    /* TODO: nice animation to hide the menu */
    clutter_actor_hide(CLUTTER_ACTOR(self));
    priv->id_hide_timeout = 0;
    return FALSE;
}

static void
enable_hide_on_timeout(MapOsm *self, gboolean hide_on_timeout)
{
    MapOsmPrivate *priv = self->priv;

    /* delete the existing timeout, if any */
    if (priv->id_hide_timeout != 0)
    {
        g_source_remove(priv->id_hide_timeout);
        priv->id_hide_timeout = 0;
    }

    if (hide_on_timeout)
    {
        priv->id_hide_timeout =
            g_timeout_add_seconds(HIDE_TIMEOUT, (GSourceFunc)hide_timeout_cb,
                                  self);
    }
}

static void
map_osm_constructed(GObject *object)
{
    MapOsmPrivate *priv = MAP_OSM_PRIV(object);
    void (*constructed)(GObject *);
    ClutterContainer *container = CLUTTER_CONTAINER(object);
    MapController *controller;
    ClutterActor *button;
    gint i;

    /* add the buttons to the OSM */
    for (i = 0; i < N_BUTTONS_ROW * 2; i++)
    {
        if (!btn_icons[i]) continue;
        button = gtk_clutter_texture_new_from_icon_name(priv->widget,
                                                        btn_icons[i], -1);
        priv->btn.v[i] = button;
        clutter_actor_set_anchor_point(button,
                                       BUTTON_SIZE_X / 2,
                                       BUTTON_SIZE_Y / 2);
        clutter_actor_set_reactive(button, TRUE);
        clutter_container_add_actor(container, button);
    }

    /* Connect signals */
    controller = map_controller_get_instance();
    g_signal_connect_swapped(priv->btn.n.zoom_in, "button-press-event",
                             G_CALLBACK(map_controller_zoom_in), controller);
    g_signal_connect_swapped(priv->btn.n.zoom_out, "button-press-event",
                             G_CALLBACK(map_controller_zoom_out), controller);
    g_signal_connect_swapped(priv->btn.n.fullscreen, "button-press-event",
                             G_CALLBACK(map_controller_switch_fullscreen),
                             controller);
    g_signal_connect_swapped(priv->btn.n.settings, "button-press-event",
                             G_CALLBACK(map_controller_activate_menu_settings),
                             controller);

    constructed = G_OBJECT_CLASS(map_osm_parent_class)->constructed;
    if (constructed != NULL)
        constructed(object);
}

static void
map_osm_set_property(GObject *object, guint property_id,
                     const GValue *value, GParamSpec *pspec)
{
    MapOsmPrivate *priv = MAP_OSM_PRIV(object);

    switch (property_id)
    {
    case PROP_WIDGET:
        priv->widget = g_value_get_object(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

static void
map_osm_dispose(GObject *object)
{
    MapOsmPrivate *priv = MAP_OSM_PRIV(object);

    if (priv->is_disposed)
        return;

    priv->is_disposed = TRUE;

    if (priv->id_hide_timeout != 0)
    {
        g_source_remove(priv->id_hide_timeout);
        priv->id_hide_timeout = 0;
    }

    G_OBJECT_CLASS(map_osm_parent_class)->dispose(object);
}

static void
map_osm_init(MapOsm *osm)
{
    MapOsmPrivate *priv;

    priv = G_TYPE_INSTANCE_GET_PRIVATE(osm, MAP_TYPE_OSM, MapOsmPrivate);
    osm->priv = priv;
}

static void
map_osm_show(ClutterActor *actor)
{
    CLUTTER_ACTOR_CLASS(map_osm_parent_class)->show(actor);

    /* TODO animation to show the OSM */
    g_debug("%s called", G_STRFUNC);
    enable_hide_on_timeout(MAP_OSM(actor), TRUE);
}

static void
map_osm_class_init(MapOsmClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    ClutterActorClass *actor_class = (ClutterActorClass *)klass;

    g_type_class_add_private(object_class, sizeof(MapOsmPrivate));

    object_class->constructed = map_osm_constructed;
    object_class->set_property = map_osm_set_property;
    object_class->dispose = map_osm_dispose;
    actor_class->show = map_osm_show;

    g_object_class_install_property(object_class, PROP_WIDGET,
        g_param_spec_object("widget", "widget", "widget",
                            GTK_TYPE_WIDGET,
                            G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
                            G_PARAM_STATIC_STRINGS));
}

void
map_osm_set_screen_size(MapOsm *self, gint width, gint height)
{
    MapOsmPrivate *priv;
    ClutterActor *button;
    gint row, i, x, y, dy;

    g_return_if_fail(MAP_IS_OSM(self));
    priv = self->priv;

    /* lay out the buttons according to the new screen size */
    dy = height / N_BUTTONS_ROW;
    x = BUTTON_X_POS;
    for (row = 0; row < 2; row++)
    {
        y = dy / 2;

        if (row == 1)
        {
            x = width - BUTTON_X_POS;
        }

        for (i = 0; i < N_BUTTONS_ROW; i++)
        {
            button = priv->btn.v[row * N_BUTTONS_ROW + i];
            if (!button) continue;

            clutter_actor_set_position(button, x, y);
            y += dy;
        }
    }
}

