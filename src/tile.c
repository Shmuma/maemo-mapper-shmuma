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
#include "tile.h"

#include "defines.h"
#include "maps.h"

#include <clutter-gtk/clutter-gtk.h>

struct _MapTilePrivate
{
    guint is_disposed : 1;
};

G_DEFINE_TYPE(MapTile, map_tile, CLUTTER_TYPE_TEXTURE);

#define MAP_TILE_PRIV(tile) (MAP_TILE(tile)->priv)

static void
map_tile_dispose(GObject *object)
{
    MapTilePrivate *priv = MAP_TILE_PRIV(object);

    if (priv->is_disposed)
	return;

    priv->is_disposed = TRUE;

    G_OBJECT_CLASS(map_tile_parent_class)->dispose(object);
}

static void
map_tile_init(MapTile *tile)
{
    MapTilePrivate *priv;

    priv = G_TYPE_INSTANCE_GET_PRIVATE(tile, MAP_TYPE_TILE, MapTilePrivate);
    tile->priv = priv;
}

static void
map_tile_class_init(MapTileClass * klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (object_class, sizeof (MapTilePrivate));

    object_class->dispose = map_tile_dispose;
}

/**
 * map_tile_load:
 * @zoom: the zoom level.
 * @x: x coordinate of the tile, in units.
 * @y: y coordinate of the tile, in units.
 *
 * Returns: a #ClutterActor representing the tile.
 */
ClutterActor *
map_tile_load(RepoData *repo, gint zoom, gint x, gint y)
{
    ClutterActor *tile;
    GdkPixbuf *pixbuf, *area;
    gint px, py, zoff;

    tile = g_object_new (MAP_TYPE_TILE, NULL);

    /* TODO: handle layers */
    for (zoff = 0; zoff + zoom <= MAX_ZOOM && zoff < 4; zoff++)
    {
        pixbuf = mapdb_get(repo, zoom + zoff, x >> zoff, y >> zoff);
        if (pixbuf)
        {
            if (zoff != 0)
            {
                gint area_size, modulo, area_x, area_y;

                area_size = TILE_SIZE_PIXELS >> zoff;
                modulo = 1 << zoff;
                area_x = (x % modulo) * area_size;
                area_y = (y % modulo) * area_size;
                area = gdk_pixbuf_new_subpixbuf (pixbuf, area_x, area_y,
                                                 area_size, area_size);
                g_object_unref (pixbuf);
                pixbuf = gdk_pixbuf_scale_simple (area,
                                                  TILE_SIZE_PIXELS,
                                                  TILE_SIZE_PIXELS,
                                                  GDK_INTERP_NEAREST);
                g_object_unref (area);

            }
            gtk_clutter_texture_set_from_pixbuf(CLUTTER_TEXTURE(tile),
                                                pixbuf, NULL);
            g_object_unref(pixbuf);
            break;
        }
    }

    if (zoff != 0)
    {
        /* TODO: start download process */
    }
    px = x * TILE_SIZE_PIXELS;
    py = y * TILE_SIZE_PIXELS;
    clutter_actor_set_position(tile, px, py);
    clutter_actor_show(tile);
    return tile;
}

