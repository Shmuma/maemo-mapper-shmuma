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

#ifndef MAEMO_MAPPER_UTIL_H
#define MAEMO_MAPPER_UTIL_H

#include <hildon-widgets/hildon-controlbar.h>

void popup_error(GtkWidget *window, const gchar *error);

Point locate_address(GtkWidget *parent, const gchar *address);

gfloat calculate_distance(gfloat lat1, gfloat lon1, gfloat lat2, gfloat lon2);
gfloat calculate_bearing(gfloat lat1, gfloat lon1, gfloat lat2, gfloat lon2);

void force_min_visible_bars(HildonControlbar *control_bar, gint num_bars);

gboolean banner_reset();

void deg_format(gfloat coor, gchar *scoor, gchar neg_char, gchar pos_char);

gint strdmstod_1(gdouble *d, gchar *nptr, gchar **endptr, gchar *sep,
        gint utf8_deg);
gdouble strdmstod_2(gchar *nptr, gchar **endptr);
gdouble strdmstod(const gchar *nptr, gchar **endptr);

#endif /* ifndef MAEMO_MAPPER_UTIL_H */
