/*
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
 * 
 * 
 * Parts of this code have been ported from Xastir by Rob Williams (Aug 2008):
 * 
 *  * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2007  The Xastir Group
 * 
 */

#ifdef HAVE_CONFIG_H
#    include "config.h"
#endif

#ifdef INCLUDE_APRS

#ifndef MAEMO_MAPPER_APRS_DECODE_H
#define MAEMO_MAPPER_APRS_DECODE_H

#define xastir_snprintf snprintf

#include "types.h"



void substr(char *dest, char *src, int size);
gint decode_ax25_line(gchar *line, TAprsPort port);


int extract_position(AprsDataRow *p_station, char **info, int type);
time_t sec_now(void);
void insert_name(AprsDataRow *p_new, AprsDataRow *p_name);
double calc_distance_haversine_radian(double lat1, double lon1, double lat2, double lon2);
void init_station_data(void);
void init_station(AprsDataRow *p_station);
char *get_tactical_from_hash(char *callsign);

double convert_lat_l2d(long lat);
double convert_lon_l2d(long lon);

#endif

#endif //INCLUDE_APRS

