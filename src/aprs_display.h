
/*
 * Created by Rob Williams - 10 Aug 2008
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
 */


#ifdef HAVE_CONFIG_H
#    include "config.h"
#endif

#ifdef INCLUDE_APRS

#ifndef MAEMO_MAPPER_APRS_DISPLAY_H
#define MAEMO_MAPPER_APRS_DISPLAY_H


#include "types.h"

void ShowAprsStationPopup(AprsDataRow *p_station);
void list_stations();
void list_messages();

#endif

#endif //INCLUDE_APRS
