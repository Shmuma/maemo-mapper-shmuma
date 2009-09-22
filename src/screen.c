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
#include "tile.h"
#include "types.h"

#include <cairo/cairo.h>

struct _MapScreenPrivate
{
    ClutterActor *map;
    ClutterActor *compass;
    ClutterActor *compass_north;

    gint zoom;

    guint show_compass : 1;

    guint is_disposed : 1;
};

G_DEFINE_TYPE(MapScreen, map_screen, GTK_CLUTTER_TYPE_EMBED);

#define MAP_SCREEN_PRIV(screen) (MAP_SCREEN(screen)->priv)

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
            tile = map_tile_load(repo, zoom, tx, ty);
            clutter_container_add_actor(map, tile);
        }
    }
}

static void
create_compass(MapScreen *screen)
{
    MapScreenPrivate *priv = screen->priv;
    gint height, width;
    cairo_surface_t *surface;
    cairo_text_extents_t te;
    cairo_t *cr;
    GError *error = NULL;

    width = 40, height = 75;
    surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    cr = cairo_create(surface);

    cairo_move_to(cr, 20, 57);
    cairo_line_to(cr, 40, 75);
    cairo_line_to(cr, 20, 0);
    cairo_line_to(cr, 0, 75);
    cairo_close_path(cr);
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_set_line_width(cr, 2);
    cairo_stroke(cr);
    cairo_destroy(cr);

    g_debug ("Creating texture");
    priv->compass = clutter_texture_new();
    g_assert(priv->compass != NULL);
    clutter_texture_set_from_rgb_data(CLUTTER_TEXTURE(priv->compass),
                                      cairo_image_surface_get_data(surface),
                                      TRUE,
                                      width, height,
                                      cairo_image_surface_get_stride(surface),
                                      4,
                                      CLUTTER_TEXTURE_NONE,
                                      &error);
    cairo_surface_destroy(surface);
    if (G_UNLIKELY(error))
    {
        g_warning("Creation of texture failed: %s", error->message);
        g_error_free(error);
    }
    clutter_actor_set_anchor_point(priv->compass, 20, 45);

    /* create the "N" letter */
    width = 16, height = 16;
    surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    cr = cairo_create(surface);
    cairo_set_source_rgb (cr, 1, 1, 1);
    cairo_select_font_face (cr, "Sans Serif",
                            CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size (cr, 20);
    cairo_text_extents (cr, "N", &te);
    cairo_move_to (cr, 8 - te.width / 2 - te.x_bearing,
                       8 - te.height / 2 - te.y_bearing);
    cairo_show_text (cr, "N");
    cairo_destroy(cr);

    priv->compass_north = clutter_texture_new();
    g_assert(priv->compass_north != NULL);
    clutter_texture_set_from_rgb_data(CLUTTER_TEXTURE(priv->compass_north),
                                      cairo_image_surface_get_data(surface),
                                      TRUE,
                                      width, height,
                                      cairo_image_surface_get_stride(surface),
                                      4,
                                      CLUTTER_TEXTURE_NONE,
                                      &error);
    cairo_surface_destroy(surface);
    if (G_UNLIKELY(error))
    {
        g_warning("Creation of texture failed: %s", error->message);
        g_error_free(error);
    }
    clutter_actor_set_anchor_point(priv->compass_north, 8, 8);
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
    if (priv->compass)
    {
        gint x, y;

        x = allocation->width - 42;
        y = allocation->height - 42;
        clutter_actor_set_position(priv->compass, x, y);
        clutter_actor_set_position(priv->compass_north, x, y);
    }
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

    create_compass(screen);
    clutter_container_add_actor(CLUTTER_CONTAINER(stage), priv->compass);
    clutter_container_add_actor(CLUTTER_CONTAINER(stage), priv->compass_north);
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
    MapScreenPrivate *priv;

    g_return_if_fail(MAP_IS_SCREEN(screen));
    priv = screen->priv;
    clutter_actor_set_rotation(priv->map,
                               CLUTTER_Z_AXIS, -angle, 0, 0, 0);

    if (priv->compass)
    {
        clutter_actor_set_rotation(priv->compass,
                                   CLUTTER_Z_AXIS, -angle, 0, 0, 0);
    }
}

/**
 * map_screen_show_compass:
 * @screen: the #MapScreen.
 * @show: %TRUE if the compass should be shown.
 */
void
map_screen_show_compass(MapScreen *screen, gboolean show)
{
    MapScreenPrivate *priv;

    g_return_if_fail(MAP_IS_SCREEN(screen));
    priv = screen->priv;

    priv->show_compass = show;
    if (show)
    {
        clutter_actor_show(priv->compass);
        clutter_actor_show(priv->compass_north);
    }
    else
    {
        clutter_actor_hide(priv->compass);
        clutter_actor_hide(priv->compass_north);
    }
}

