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
#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif
#include "screen.h"

struct _MapScreenPrivate
{
    ClutterActor *map;

    guint is_disposed : 1;
};

G_DEFINE_TYPE(MapScreen, map_screen, GTK_CLUTTER_TYPE_EMBED);

#define MAP_SCREEN_PRIV(screen) (MAP_SCREEN(screen)->priv)

static void
map_screen_dispose(GObject *object)
{
    MapScreenPrivate *priv = MAP_SCREEN_PRIV(object);

    if (priv->is_disposed)
	return;

    priv->is_disposed = TRUE;

    G_OBJECT_CLASS(map_screen_parent_class)->dispose(object);
}

static void
map_screen_init(MapScreen *screen)
{
    MapScreenPrivate *priv;
    ClutterActor *stage;

    priv = G_TYPE_INSTANCE_GET_PRIVATE(screen, MAP_TYPE_SCREEN,
                                       MapScreenPrivate);
    screen->priv = priv;

    stage = gtk_clutter_embed_get_stage(GTK_CLUTTER_EMBED(screen));
    g_return_if_fail(stage != NULL);

    priv->map = clutter_group_new();
    g_return_if_fail(priv->map != NULL);
    clutter_container_add_actor(CLUTTER_CONTAINER(stage), priv->map);
    clutter_actor_show(priv->map);
}

static void
map_screen_class_init(MapScreenClass * klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (object_class, sizeof (MapScreenPrivate));

    object_class->dispose = map_screen_dispose;
}

