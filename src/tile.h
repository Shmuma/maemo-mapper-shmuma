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

#ifndef MAP_TILE_H
#define MAP_TILE_H

#include <glib.h>
#include <glib-object.h>
#include <clutter/clutter.h>
#include "types.h"
 
G_BEGIN_DECLS

#define MAP_TYPE_TILE         (map_tile_get_type ())
#define MAP_TILE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MAP_TYPE_TILE, MapTile))
#define MAP_TILE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), MAP_TYPE_TILE, MapTileClass))
#define MAP_IS_TILE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MAP_TYPE_TILE))
#define MAP_IS_TILE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MAP_TYPE_TILE))
#define MAP_TILE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MAP_TYPE_TILE, MapTileClass))

typedef struct _MapTile MapTile;
typedef struct _MapTilePrivate MapTilePrivate;
typedef struct _MapTileClass MapTileClass;


struct _MapTile
{
    ClutterTexture parent;

    gint zoom;
    gint x;
    gint y;

    MapTilePrivate *priv;
};

struct _MapTileClass
{
    ClutterTextureClass parent_class;
};

GType map_tile_get_type (void);

ClutterActor *map_tile_load(RepoData *repo, gint zoom, gint x, gint y);

G_END_DECLS
#endif /* MAP_TILE_H */
