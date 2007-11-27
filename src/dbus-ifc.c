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

#define _GNU_SOURCE

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <libosso.h>
#include <glib.h>
#include "dbus/dbus.h"

#define DBUS_API_SUBJECT_TO_CHANGE

#include "types.h"
#include "data.h"
#include "defines.h"

#include "display.h"
#include "dbus-ifc.h"

/***********************
 * BELOW: DBUS METHODS *
 ***********************/

static gint
dbus_ifc_cb_default(const gchar *interface, const gchar *method,
        GArray *args, gpointer data, osso_rpc_t *retval)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(!strcmp(method, "top_application"))
    {
        g_idle_add((GSourceFunc)window_present, NULL);
    }

    retval->type = DBUS_TYPE_INVALID;

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return OSSO_OK;
}

static gboolean
dbus_ifc_set_view_position_idle(SetViewPositionArgs *args)
{
    Point center;
    printf("%s(%f, %f, %d, %f)\n", __PRETTY_FUNCTION__, args->new_lat,
            args->new_lon, args->new_zoom, args->new_viewing_angle);

    if(!_mouse_is_down)
    {
        if(_center_mode > 0)
            gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(
                        _menu_view_ac_none_item), TRUE);

        latlon2unit(args->new_lat, args->new_lon, center.unitx, center.unity);

        map_center_unit_full(center, args->new_zoom, args->new_viewing_angle);
    }

    g_free(args);

    vprintf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
    return FALSE;
}

static gint
dbus_ifc_handle_set_view_center(GArray *args, osso_rpc_t *retval)
{
    printf("%s()\n", __PRETTY_FUNCTION__);
    SetViewPositionArgs *svca = g_new(SetViewPositionArgs, 1);

    /* Argument 3: int: zoom.  Get this first because we might need it to
     * calculate the next latitude and/or longitude. */
    if(args->len >= 3
            && g_array_index(args, osso_rpc_t, 2).type == DBUS_TYPE_INT32)
    {
        svca->new_zoom = CLAMP(g_array_index(args, osso_rpc_t, 2).value.i,
                MIN_ZOOM, MAX_ZOOM);
    }
    else
        svca->new_zoom = _next_zoom;

    if(args->len < 2)
    {
        /* Latitude and/or Longitude are not defined.  Calculate next. */
        Point new_center = map_calc_new_center(svca->new_zoom);
        unit2latlon(new_center.unitx, new_center.unity, svca->new_lat, svca->new_lon);
    }

    /* Argument 1: double: latitude. */
    if(args->len >= 1
        && g_array_index(args, osso_rpc_t, 0).type == DBUS_TYPE_DOUBLE)
    {
        svca->new_lat = CLAMP(g_array_index(args, osso_rpc_t, 0).value.d,
                -90.0, 90.0);
    }

    /* Argument 2: double: longitude. */
    if(args->len >= 2
            && g_array_index(args, osso_rpc_t, 1).type == DBUS_TYPE_DOUBLE)
    {
        svca->new_lon = CLAMP(g_array_index(args, osso_rpc_t, 1).value.d,
                -180.0, 180.0);
    }

    /* Argument 4: double: viewing angle. */
    if(args->len >= 4
            && g_array_index(args, osso_rpc_t, 3).type == DBUS_TYPE_DOUBLE)
    {
        svca->new_viewing_angle
            = (gint)(g_array_index(args, osso_rpc_t, 2).value.d + 0.5) % 360;
    }
    else
        svca->new_viewing_angle = _center_mode > 0 && _center_rotate
            ? _gps.heading : _next_map_rotate_angle;

    g_idle_add((GSourceFunc)dbus_ifc_set_view_position_idle, svca);
    retval->type = DBUS_TYPE_INVALID;

    vprintf("%s(): return OSSO_OK\n", __PRETTY_FUNCTION__);
    return OSSO_OK;
}

static gint
dbus_ifc_controller(const gchar *interface, const gchar *method,
        GArray *args, gpointer data, osso_rpc_t *retval)
{
    printf("%s(%s)\n", __PRETTY_FUNCTION__, method);

    /* Method: set_view_center */
    if(!strcmp(method, MM_DBUS_METHOD_SET_VIEW_POSITION))
        return dbus_ifc_handle_set_view_center(args, retval);

    vprintf("%s(): return OSSO_ERROR\n", __PRETTY_FUNCTION__);
    return OSSO_ERROR;
}

/***********************
 * ABOVE: DBUS METHODS *
 ***********************/


/***********************
 * BELOW: DBUS SIGNALS *
 ***********************/

static gboolean
dbus_ifc_view_position_changed_idle(ViewPositionChangedArgs *args)
{
    DBusConnection *bus = NULL;
    DBusError error;
    DBusMessage *message = NULL;
    printf("%s(%f, %f, %d, %f)\n", __PRETTY_FUNCTION__, args->new_lat,
            args->new_lon, args->new_zoom, args->new_viewing_angle);

    dbus_error_init(&error);

    if(NULL == (bus = dbus_bus_get(DBUS_BUS_SESSION, &error))
            || NULL == (message = dbus_message_new_signal(MM_DBUS_PATH,
                    MM_DBUS_INTERFACE, MM_DBUS_SIGNAL_VIEW_POSITION_CHANGED))
            || !dbus_message_append_args(message,
                DBUS_TYPE_DOUBLE, &args->new_lat,
                DBUS_TYPE_DOUBLE, &args->new_lon,
                DBUS_TYPE_INT32, &args->new_zoom,
                DBUS_TYPE_DOUBLE, &args->new_viewing_angle,
                DBUS_TYPE_INVALID)
            || !dbus_connection_send(bus, message, NULL))
    {
        g_printerr("Error sending view_position_changed signal: %s: %s\n",
                (dbus_error_is_set(&error)
                 ? error.name : "<no error message>"),
                (dbus_error_is_set(&error)
                 ? error.message : ""));
    }

    if(message)
        dbus_message_unref(message);
    if(bus)
        dbus_connection_unref(bus);

    vprintf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
    return FALSE;
}

void
dbus_ifc_fire_view_position_changed(
        Point new_center, gint new_zoom, gdouble new_viewing_angle)
{
    ViewPositionChangedArgs *args;
    printf("%s(%d, %d, %d, %f)\n", __PRETTY_FUNCTION__, new_center.unitx,
            new_center.unity, new_zoom, new_viewing_angle);

    args = g_new(ViewPositionChangedArgs, 1);
    unit2latlon(new_center.unitx, new_center.unity,
            args->new_lat, args->new_lon);
    args->new_zoom = new_zoom;
    args->new_viewing_angle = new_viewing_angle;

    g_idle_add((GSourceFunc)dbus_ifc_view_position_changed_idle, args);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

static gboolean
dbus_ifc_view_dimensions_changed_idle(ViewDimensionsChangedArgs *args)
{
    DBusConnection *bus = NULL;
    DBusError error;
    DBusMessage *message = NULL;
    printf("%s(%d, %d)\n", __PRETTY_FUNCTION__,
            args->new_view_width_pixels, args->new_view_height_pixels);

    dbus_error_init(&error);

    if(NULL == (bus = dbus_bus_get(DBUS_BUS_SESSION, &error))
            || NULL == (message = dbus_message_new_signal(MM_DBUS_PATH,
                    MM_DBUS_INTERFACE, MM_DBUS_SIGNAL_VIEW_DIMENSIONS_CHANGED))
            || !dbus_message_append_args(message,
                DBUS_TYPE_INT32, &args->new_view_width_pixels,
                DBUS_TYPE_INT32, &args->new_view_height_pixels,
                DBUS_TYPE_INVALID)
            || !dbus_connection_send(bus, message, NULL))
    {
        g_printerr("Error sending view_dimensions_changed signal: %s: %s\n",
                (dbus_error_is_set(&error)
                 ? error.name : "<no error message>"),
                (dbus_error_is_set(&error)
                 ? error.message : ""));
    }

    if(message)
        dbus_message_unref(message);
    if(bus)
        dbus_connection_unref(bus);

    vprintf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
    return FALSE;
}

void
dbus_ifc_fire_view_dimensions_changed(
        gint new_view_width_pixels, gint new_view_height_pixels)
{
    ViewDimensionsChangedArgs *args;
    printf("%s(%d, %d)\n", __PRETTY_FUNCTION__,
            new_view_width_pixels, new_view_height_pixels);

    args = g_new(ViewDimensionsChangedArgs, 1);
    args->new_view_width_pixels = new_view_width_pixels;
    args->new_view_height_pixels = new_view_height_pixels;

    g_idle_add((GSourceFunc)dbus_ifc_view_dimensions_changed_idle, args);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

/***********************
 * ABOVE: DBUS SIGNALS *
 ***********************/

void
dbus_ifc_init()
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(OSSO_OK != osso_rpc_set_default_cb_f(_osso, dbus_ifc_cb_default, NULL))
        g_printerr("osso_rpc_set_default_cb_f failed.\n");

    if(OSSO_OK != osso_rpc_set_cb_f(_osso, MM_DBUS_SERVICE,
                MM_DBUS_PATH, MM_DBUS_INTERFACE, dbus_ifc_controller, NULL))
        g_printerr("osso_rpc_set_cb_f failed.\n");

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}
