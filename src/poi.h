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

#ifndef MAEMO_MAPPER_POI_H
#define MAEMO_MAPPER_POI_H

void poi_db_connect();

gboolean get_nearest_poi(gint unitx, gint unity, PoiInfo *poi);

gboolean select_poi(gint unitx, gint unity, PoiInfo *poi, gboolean quick);

gboolean category_list_dialog(GtkWidget *parent);

gboolean poi_add_dialog(GtkWidget *parent, gint unitx, gint unity);
gboolean poi_view_dialog(GtkWidget *parent, PoiInfo *poi);
gboolean poi_import_dialog(gint unitx, gint unity);
gboolean poi_download_dialog(gint unitx, gint unity);
gboolean poi_browse_dialog(gint unitx, gint unity);

void map_render_poi();

void poi_destroy();

#endif /* ifndef MAEMO_MAPPER_POI_H */
