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

#ifdef HAVE_CONFIG_H
#    include "config.h"
#endif

#ifndef MAEMO_MAPPER_UTIL_H
#define MAEMO_MAPPER_UTIL_H

#ifndef LEGACY
#    include <hildon/hildon-controlbar.h>
#else
#    include <hildon-widgets/hildon-controlbar.h>
#endif

void popup_error(GtkWidget *window, const gchar *error);

Point locate_address(GtkWidget *parent, const gchar *address);

gdouble calculate_distance(gdouble lat1, gdouble lon1,
        gdouble lat2, gdouble lon2);
gdouble calculate_bearing(gdouble lat1, gdouble lon1,
        gdouble lat2, gdouble lon2);

void force_min_visible_bars(HildonControlbar *control_bar, gint num_bars);

gboolean banner_reset(void);

void deg_format(gdouble coor, gchar *scoor, gchar neg_char, gchar pos_char);

gdouble strdmstod(const gchar *nptr, gchar **endptr);

gboolean parse_coords(const gchar* txt_lat, const gchar* txt_lon, gdouble* lat, gdouble* lon);
void format_lat_lon(gdouble d_lat, gdouble d_lon, gchar* lat, gchar* lon);

gboolean coord_system_check_lat_lon (gdouble lat, gdouble lon, gint *fallback_deg_format);

gint64 g_ascii_strtoll(const gchar *nptr, gchar **endptr, guint base);
gint convert_str_to_int(const gchar *str);

gint gint_sqrt(gint num);

#endif /* ifndef MAEMO_MAPPER_UTIL_H */
