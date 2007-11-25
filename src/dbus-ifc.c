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

#define DBUS_API_SUBJECT_TO_CHANGE

#include "types.h"
#include "data.h"
#include "defines.h"

#include "display.h"
#include "dbus-ifc.h"

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
dbus_ifc_set_view_center_idle(SetViewCenterArgs *args)
{
    Point center;
    printf("%s(%f, %f, %d)\n", __PRETTY_FUNCTION__,
            args->lat, args->lon, args->zoom);

    latlon2unit(args->lat, args->lon, center.unitx, center.unity);

    map_center_unit_full(center, args->zoom,
            _center_mode > 0 && _center_rotate
            ? _gps.heading : _next_map_rotate_angle);

    g_free(args);

    vprintf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
    return FALSE;
}

static gint
dbus_ifc_handle_set_view_center(GArray *args, osso_rpc_t *retval)
{
    printf("%s()\n", __PRETTY_FUNCTION__);
    SetViewCenterArgs *svca = g_new(SetViewCenterArgs, 1);

    /* Argument 1: double: latitude. */
    if(args->len >= 1
        && g_array_index(args, osso_rpc_t, 0).type == DBUS_TYPE_DOUBLE)
    {
        svca->lat = CLAMP(g_array_index(args, osso_rpc_t, 0).value.d,
                -90.0, 90.0);
    }
    else
    {
        gdouble tmp;
        unit2latlon(_center.unitx, _center.unity, svca->lat, tmp);
    }

    /* Argument 2: double: longitude. */
    if(args->len >= 2
            && g_array_index(args, osso_rpc_t, 1).type == DBUS_TYPE_DOUBLE)
    {
        svca->lon = CLAMP(g_array_index(args, osso_rpc_t, 1).value.d,
                -180.0, 180.0);
    }
    else
    {
        gdouble tmp;
        unit2latlon(_center.unitx, _center.unity, tmp, svca->lon);
    }

    /* Argument 2 (optional): int: zoom. */
    if(args->len >= 3
            && g_array_index(args, osso_rpc_t, 2).type == DBUS_TYPE_INT32)
    {
        svca->zoom = CLAMP(g_array_index(args, osso_rpc_t, 2).value.i,
                MIN_ZOOM, MAX_ZOOM);
    }
    else
        svca->zoom = _zoom;

    g_idle_add((GSourceFunc)dbus_ifc_set_view_center_idle, svca);
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
    if(!strcmp(method, MM_DBUS_METHOD_SET_VIEW_CENTER))
        return dbus_ifc_handle_set_view_center(args, retval);

    vprintf("%s(): return OSSO_ERROR\n", __PRETTY_FUNCTION__);
    return OSSO_ERROR;
}

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
