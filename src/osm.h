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

#ifndef MAP_OSM_H
#define MAP_OSM_H

#include <glib.h>
#include <glib-object.h>
#include <clutter/clutter.h>
 
G_BEGIN_DECLS

#define MAP_TYPE_OSM         (map_osm_get_type ())
#define MAP_OSM(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MAP_TYPE_OSM, MapOsm))
#define MAP_OSM_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), MAP_TYPE_OSM, MapOsmClass))
#define MAP_IS_OSM(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MAP_TYPE_OSM))
#define MAP_IS_OSM_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MAP_TYPE_OSM))
#define MAP_OSM_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MAP_TYPE_OSM, MapOsmClass))

typedef struct _MapOsm MapOsm;
typedef struct _MapOsmPrivate MapOsmPrivate;
typedef struct _MapOsmClass MapOsmClass;


struct _MapOsm
{
    ClutterGroup parent;

    MapOsmPrivate *priv;
};

struct _MapOsmClass
{
    ClutterGroupClass parent_class;
};

GType map_osm_get_type (void);

void map_osm_set_screen_size(MapOsm *self, gint width, gint height);

G_END_DECLS
#endif /* MAP_OSM_H */
