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

/**
 * Sets the view center (including zoom and viewing angle).  All of the
 * fields are optional, although (due to the requirements of DBUS), if you
 * specify one value, you must specify all the values preceding it.  I.e. if
 * you specify new_viewing_angle, you must specify all the fields.
 *
 * A screen refresh is performed regardless of whether or not any of the
 * fields are specified or different from previous values.  Thus, you can
 * force a refresh of the screen simply by calling this method with no
 * arguments.
 *
 * This method has no effect if the user is currently tap-and-holding or
 * dragging on the screen.
 *
 * Note that, if this method causes the refresh of the screen, then
 * Auto-Center will always become disabled.
 */
#define MM_DBUS_METHOD_SET_VIEW_POSITION "set_view_center"
typedef struct _SetViewPositionArgs SetViewPositionArgs;
struct _SetViewPositionArgs
{
    gdouble new_lat;
    gdouble new_lon;
    gint new_zoom;
    gdouble new_viewing_angle; /* i.e. rotation */
};

/**
 * Occurs whenever the view center (including zoom and viewing angle)
 * changes.  This signal is not necessarily fired when the screen size is
 * changed.  Not all of the given information may necessarily have changed,
 * but at least one is guaranteed to have changed.
 */
#define MM_DBUS_SIGNAL_VIEW_POSITION_CHANGED "view_position_changed"
typedef struct _ViewPositionChangedArgs ViewPositionChangedArgs;
struct _ViewPositionChangedArgs
{
    gdouble new_lat;
    gdouble new_lon;
    gint new_zoom;
    gdouble new_viewing_angle; /* i.e. rotation */
};
void dbus_ifc_fire_view_position_changed(
        Point new_center, gint new_zoom, gdouble new_viewing_angle);

/**
 * Occurs whenever the dimensions (width and height) of the map change.  The
 * dimensions do not include any sidebars (like the GPS info panel).  If the
 * GPS info panel is hidden or unhidden, this event will be fired.
 */
#define MM_DBUS_SIGNAL_VIEW_DIMENSIONS_CHANGED "view_dimensions_changed"
typedef struct _ViewDimensionsChangedArgs ViewDimensionsChangedArgs;
struct _ViewDimensionsChangedArgs
{
    gint new_view_width_pixels;
    gint new_view_height_pixels;
};
void dbus_ifc_fire_view_dimensions_changed(
        gint new_view_width_pixels, gint new_view_height_pixels);


void dbus_ifc_init(void);

#endif /* ifndef MAEMO_MAPPER_GPS_H */
