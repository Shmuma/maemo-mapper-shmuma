/*
 * Copyright (C) 2006, 2007 John Costigan.
 *
 * POI and GPS-Info code originally written by Cezary Jackiewicz.
 *
 * Default map data provided by http://www.openstreetmap.org/
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

#ifndef MAEMO_MAPPER_PATH_H
#define MAEMO_MAPPER_PATH_H

void path_resize(Path *path, gint size);
void path_wresize(Path *path, gint wsize);

void path_save_route_to_db();

void route_find_nearest_point();
gboolean route_show_distance_to(Point *point);
void route_show_distance_to_next();
void route_show_distance_to_last();

void track_show_distance_from_last();
void track_show_distance_from_first();

gboolean track_add(time_t time, gboolean newly_fixed);
void track_clear();
void track_insert_break(gboolean temporary);

void path_reset_route();

void cancel_autoroute();

WayPoint * find_nearest_waypoint(gint unitx, gint unity);

gboolean route_download(gchar *to);
void route_add_way_dialog(gint unitx, gint unity);

WayPoint* path_get_next_way();

void path_init();
void path_destroy();

#endif /* ifndef MAEMO_MAPPER_PATH_H */
