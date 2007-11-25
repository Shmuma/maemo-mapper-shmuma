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

#ifndef MAEMO_MAPPER_DBUS_H
#define MAEMO_MAPPER_DBUS_H

#define DBUS_API_SUBJECT_TO_CHANGE

#define MM_DBUS_SERVICE "com.gnuite.maemo_mapper"
#define MM_DBUS_PATH "/com/gnuite/maemo_mapper"
#define MM_DBUS_INTERFACE "com.gnuite.maemo_mapper"

#define MM_DBUS_METHOD_SET_VIEW_CENTER "set_view_center"
typedef struct _SetViewCenterArgs SetViewCenterArgs;
struct _SetViewCenterArgs
{
    gdouble lat;
    gdouble lon;
    gint zoom; /* Optional - omit to keep zoom the same. */
};

void dbus_ifc_init(void);

#endif /* ifndef MAEMO_MAPPER_GPS_H */
