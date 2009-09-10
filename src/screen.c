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

#include "data.h"
#include "defines.h"
#include "maps.h"
#include "types.h"

struct _MapScreenPrivate
{
    ClutterActor *map;

    gint zoom;

    guint is_disposed : 1;
};

G_DEFINE_TYPE(MapScreen, map_screen, GTK_CLUTTER_TYPE_EMBED);

#define MAP_SCREEN_PRIV(screen) (MAP_SCREEN(screen)->priv)

static ClutterActor *
create_tile(RepoData *repo, gint zoom, gint x, gint y)
{
    ClutterActor *tile;
    GdkPixbuf *pixbuf;
    gint px, py;

    /* TODO: handle layers */
    pixbuf = mapdb_get(repo, zoom, x, y);
    if (pixbuf)
    {
        tile = gtk_clutter_texture_new_from_pixbuf(pixbuf);
        g_object_unref(pixbuf);
    }
    else
        /* TODO: start download process */
        tile = clutter_texture_new();
    px = x * TILE_SIZE_PIXELS;
    py = y * TILE_SIZE_PIXELS;
    clutter_actor_set_position(tile, px, py);
    clutter_actor_show(tile);
    return tile;
}
static void
load_tiles_into_map(MapScreen *screen, RepoData *repo, gint zoom,
                    gint tx1, gint ty1, gint tx2, gint ty2)
{
    ClutterContainer *map;
    ClutterActor *tile;
    gint tx, ty;

    map = CLUTTER_CONTAINER(screen->priv->map);

    for (tx = tx1; tx <= tx2; tx++)
    {
        for (ty = ty1; ty <= ty2; ty++)
        {
            tile = create_tile(repo, zoom, tx, ty);
            clutter_container_add_actor(map, tile);
        }
    }
}

static void
map_screen_size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
    MapScreenPrivate *priv = MAP_SCREEN_PRIV(widget);

    GTK_WIDGET_CLASS(map_screen_parent_class)->size_allocate
        (widget, allocation);

    /* Position the map at the center of the widget */
    clutter_actor_set_position(priv->map,
                               allocation->width / 2,
                               allocation->height / 2);
}

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
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    g_type_class_add_private (object_class, sizeof (MapScreenPrivate));

    object_class->dispose = map_screen_dispose;

    widget_class->size_allocate = map_screen_size_allocate;
}

/**
 * map_screen_set_center:
 * @screen: the #MapScreen.
 * @x: the x coordinate of the new center.
 * @y: the y coordinate of the new center.
 * @zoom: the new zoom level, or -1 to keep the current one.
 */
void
map_screen_set_center(MapScreen *screen, gint x, gint y, gint zoom)
{
    MapScreenPrivate *priv;
    GtkAllocation *allocation;
    gint diag_halflength_units;
    gint start_tilex, start_tiley, stop_tilex, stop_tiley;
    gint px, py;
    gint cache_amount;
    gint new_zoom;
    RepoData *repo = _curr_repo;

    g_return_if_fail(MAP_IS_SCREEN(screen));
    priv = screen->priv;

    new_zoom = (zoom > 0) ? zoom : priv->zoom;

    /* Destroying all the existing tiles.
     * TODO: implement some caching, reuse tiles when possible. */
    clutter_group_remove_all(CLUTTER_GROUP(priv->map));

    /* Calculate cache amount */
    if(repo->type != REPOTYPE_NONE && MAPDB_EXISTS(repo))
        cache_amount = _auto_download_precache;
    else
        cache_amount = 1; /* No cache. */

    allocation = &(GTK_WIDGET(screen)->allocation);
    diag_halflength_units =
        pixel2zunit(TILE_HALFDIAG_PIXELS +
                    MAX(allocation->width, allocation->height) / 2,
                    new_zoom);

    start_tilex = unit2ztile(x - diag_halflength_units + _map_correction_unitx,
                             new_zoom);
    start_tilex = MAX(start_tilex - (cache_amount - 1), 0);
    start_tiley = unit2ztile(y - diag_halflength_units + _map_correction_unity,
                             new_zoom);
    start_tiley = MAX(start_tiley - (cache_amount - 1), 0);
    stop_tilex = unit2ztile(x + diag_halflength_units + _map_correction_unitx,
                            new_zoom);
    stop_tilex = MIN(stop_tilex + (cache_amount - 1),
                     unit2ztile(WORLD_SIZE_UNITS, new_zoom));
    stop_tiley = unit2ztile(y + diag_halflength_units + _map_correction_unity,
                            new_zoom);
    stop_tiley = MIN(stop_tiley + (cache_amount - 1),
                     unit2ztile(WORLD_SIZE_UNITS, new_zoom));

    /* TODO check what tiles are already on the map */

    /* create the tiles */
    load_tiles_into_map(screen, repo, new_zoom,
                        start_tilex, start_tiley, stop_tilex, stop_tiley);

    /* Move the anchor point to the new center */
    px = unit2zpixel(x, new_zoom);
    py = unit2zpixel(y, new_zoom);
    clutter_actor_set_anchor_point(priv->map, px, py);
}

/**
 * map_screen_set_rotation:
 * @screen: the #MapScreen.
 * @angle: the new rotation angle.
 */
void
map_screen_set_rotation(MapScreen *screen, gint angle)
{
    GtkAllocation *allocation;
    g_return_if_fail(MAP_IS_SCREEN(screen));
    allocation = &(GTK_WIDGET(screen)->allocation);
    clutter_actor_set_rotation(screen->priv->map,
                               CLUTTER_Z_AXIS, -angle, 0, 0, 0);
}

