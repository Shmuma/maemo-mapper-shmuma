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

#ifndef MAEMO_MAPPER_GPX_H
#define MAEMO_MAPPER_GPX_H

gboolean gpx_path_parse(Path *to_replace, gchar *buffer, gint size,
        gint policy_old);
gboolean gpx_path_write(Path *path, GnomeVFSHandle *handle);

gint gpx_poi_parse(gchar *buffer, gint size, GList **list);
gint gpx_poi_write(GtkTreeModel *model, GnomeVFSHandle *handle);

#endif /* #ifndef MAEMO_MAPPER_GPX_H */
