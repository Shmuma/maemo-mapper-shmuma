/* vi: set et sw=4 ts=4 cino=t0,(0: */
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

#define _GNU_SOURCE

#include <dialog.h>
#include <string.h>
#include <math.h>
#include <gtk/gtk.h>
#include <gconf/gconf-client.h>

#ifndef LEGACY
#    include <hildon/hildon-note.h>
#    include <hildon/hildon-banner.h>
#else
#    include <hildon-widgets/hildon-note.h>
#    include <hildon-widgets/hildon-banner.h>
#endif

#include "data.h"
#include "defines.h"

#include "cmenu.h"
#include "display.h"
#include "gdk-pixbuf-rotate.h"
#include "menu.h"
#include "path.h"
#include "poi.h"
#include "util.h"

#define GCONF_SUPL_KEY_PREFIX "/system/osso/supl"
#define GCONF_KEY_SUPL_LAT GCONF_SUPL_KEY_PREFIX"/pos_latitude"
#define GCONF_KEY_SUPL_LON GCONF_SUPL_KEY_PREFIX"/pos_longitude"
#define GCONF_KEY_SUPL_TIME GCONF_SUPL_KEY_PREFIX"/pos_timestamp"

static void
cmenu_show_latlon(gint unitx, gint unity)
{
  gdouble lat, lon;
  gint tmp_degformat = _degformat;
  gint fallback_deg_format = _degformat;
  gchar buffer[80], tmp1[LL_FMT_LEN], tmp2[LL_FMT_LEN];
  
  printf("%s()\n", __PRETTY_FUNCTION__);

  unit2latlon(unitx, unity, lat, lon);
  
  // Check that the current coord system supports the select position
  if(!coord_system_check_lat_lon (lat, lon, &fallback_deg_format))
  {
  	_degformat = fallback_deg_format;
  }
      
  format_lat_lon(lat, lon, tmp1, tmp2);
  

  if(DEG_FORMAT_ENUM_TEXT[_degformat].field_2_in_use)
  {
	  snprintf(buffer, sizeof(buffer),
	            "%s: %s\n"
	          "%s: %s",
	          DEG_FORMAT_ENUM_TEXT[_degformat].long_field_1, tmp1,
	          DEG_FORMAT_ENUM_TEXT[_degformat].long_field_2, tmp2);
  }
  else
  {
	  snprintf(buffer, sizeof(buffer),
  	            "%s: %s",
  	          DEG_FORMAT_ENUM_TEXT[_degformat].long_field_1, tmp1);
  }
  
  MACRO_BANNER_SHOW_INFO(_window, buffer);

  _degformat = tmp_degformat;
  
  vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

static void
cmenu_clip_latlon(gint unitx, gint unity)
{
    gchar buffer[80];
    gdouble lat, lon;
    printf("%s()\n", __PRETTY_FUNCTION__);

    unit2latlon(unitx, unity, lat, lon);

    snprintf(buffer, sizeof(buffer), "%.06f,%.06f", lat, lon);

    gtk_clipboard_set_text(
            gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), buffer, -1);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

static void
cmenu_route_to(gint unitx, gint unity)
{
    gchar buffer[80];
    gchar strlat[32];
    gchar strlon[32];
    gdouble lat, lon;
    printf("%s()\n", __PRETTY_FUNCTION__);

    unit2latlon(unitx, unity, lat, lon);

    g_ascii_formatd(strlat, 32, "%.06f", lat);
    g_ascii_formatd(strlon, 32, "%.06f", lon);
    snprintf(buffer, sizeof(buffer), "%s, %s", strlat, strlon);

    route_download(buffer);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

static void
cmenu_distance_to(gint unitx, gint unity)
{
    gchar buffer[80];
    gdouble lat, lon;
    printf("%s()\n", __PRETTY_FUNCTION__);

    unit2latlon(unitx, unity, lat, lon);

    snprintf(buffer, sizeof(buffer), "%s: %.02f %s", _("Distance"),
            calculate_distance(_gps.lat, _gps.lon, lat, lon)
              * UNITS_CONVERT[_units],
            UNITS_ENUM_TEXT[_units]);
    MACRO_BANNER_SHOW_INFO(_window, buffer);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

static void
cmenu_add_route(gint unitx, gint unity)
{
    printf("%s()\n", __PRETTY_FUNCTION__);
    MACRO_PATH_INCREMENT_TAIL(_route);
    _route.tail->unitx = _cmenu_unitx;
    _route.tail->unity = _cmenu_unity;
    route_find_nearest_point();
    map_force_redraw();
    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

static gboolean
cmenu_cb_loc_show_latlon(GtkMenuItem *item)
{
    gdouble lat, lon;
    printf("%s()\n", __PRETTY_FUNCTION__);

    unit2latlon(_cmenu_unitx, _cmenu_unity, lat, lon);

    latlon_dialog(lat, lon);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
cmenu_cb_loc_route_to(GtkMenuItem *item)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    cmenu_route_to(_cmenu_unitx, _cmenu_unity);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
cmenu_cb_loc_download_poi(GtkMenuItem *item)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    poi_download_dialog(_cmenu_unitx, _cmenu_unity);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
cmenu_cb_loc_browse_poi(GtkMenuItem *item)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    poi_browse_dialog(_cmenu_unitx, _cmenu_unity);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
cmenu_cb_loc_distance_to(GtkMenuItem *item)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    cmenu_distance_to(_cmenu_unitx, _cmenu_unity);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
cmenu_cb_loc_add_route(GtkMenuItem *item)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    cmenu_add_route(_cmenu_unitx, _cmenu_unity);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
cmenu_cb_loc_add_way(GtkMenuItem *item)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    route_add_way_dialog(_cmenu_unitx, _cmenu_unity);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
cmenu_cb_loc_add_poi(GtkMenuItem *item)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    poi_add_dialog(_window, _cmenu_unitx, _cmenu_unity);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
cmenu_cb_loc_set_gps(GtkMenuItem *item)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    _pos.unitx = _cmenu_unitx;
    _pos.unity = _cmenu_unity;
    unit2latlon(_pos.unitx, _pos.unity, _gps.lat, _gps.lon);

    /* Move mark to new location. */
    map_refresh_mark(_center_mode > 0);

    GConfClient *gconf_client = gconf_client_get_default();
    GTimeVal curtime;

    gconf_client_set_float(gconf_client, GCONF_KEY_SUPL_LON, _gps.lon, NULL);
    gconf_client_set_float(gconf_client, GCONF_KEY_SUPL_LAT, _gps.lat, NULL);
    g_get_current_time(&curtime);
    gconf_client_set_float(gconf_client, GCONF_KEY_SUPL_TIME, curtime.tv_sec, NULL);

    g_object_unref(gconf_client);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
cmenu_cb_loc_apply_correction(GtkMenuItem *item)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item)))
    {
        /* Get difference between tap point and GPS location. */
        _map_correction_unitx = _cmenu_unitx - _pos.unitx;
        _map_correction_unity = _cmenu_unity - _pos.unity;
        map_refresh_mark(TRUE);
        MACRO_BANNER_SHOW_INFO(_window, _("Map correction applied."));
    }
    else
    {
        _map_correction_unitx = 0;
        _map_correction_unity = 0;
        map_refresh_mark(TRUE);
        MACRO_BANNER_SHOW_INFO(_window, _("Map correction removed."));
    }

    printf("Map correction now set to: %d, %d\n");

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
cmenu_cb_way_show_latlon(GtkMenuItem *item)
{
    WayPoint *way;
    printf("%s()\n", __PRETTY_FUNCTION__);

    if((way = find_nearest_waypoint(_cmenu_unitx, _cmenu_unity)))
        cmenu_show_latlon(way->point->unitx, way->point->unity);
    else
    {
        MACRO_BANNER_SHOW_INFO(_window, _("There are no waypoints."));
    }


    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
cmenu_cb_way_show_desc(GtkMenuItem *item)
{
    WayPoint *way;
    printf("%s()\n", __PRETTY_FUNCTION__);

    if((way = find_nearest_waypoint(_cmenu_unitx, _cmenu_unity)))
    {
        MACRO_BANNER_SHOW_INFO(_window, way->desc);
    }
    else
    {
        MACRO_BANNER_SHOW_INFO(_window, _("There are no waypoints."));
    }

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
cmenu_cb_way_clip_latlon(GtkMenuItem *item)
{
    WayPoint *way;
    printf("%s()\n", __PRETTY_FUNCTION__);

    if((way = find_nearest_waypoint(_cmenu_unitx, _cmenu_unity)))
        cmenu_clip_latlon(way->point->unitx, way->point->unity);
    else
    {
        MACRO_BANNER_SHOW_INFO(_window, _("There are no waypoints."));
    }

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
cmenu_cb_way_clip_desc(GtkMenuItem *item)
{
    WayPoint *way;
    printf("%s()\n", __PRETTY_FUNCTION__);

    if((way = find_nearest_waypoint(_cmenu_unitx, _cmenu_unity)))
        gtk_clipboard_set_text(
                gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), way->desc, -1);
    else
    {
        MACRO_BANNER_SHOW_INFO(_window, _("There are no waypoints."));
    }

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
cmenu_cb_way_route_to(GtkMenuItem *item)
{
    WayPoint *way;
    printf("%s()\n", __PRETTY_FUNCTION__);

    if((way = find_nearest_waypoint(_cmenu_unitx, _cmenu_unity)))
        cmenu_route_to(way->point->unitx, way->point->unity);
    else
    {
        MACRO_BANNER_SHOW_INFO(_window, _("There are no waypoints."));
    }

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
cmenu_cb_way_distance_to(GtkMenuItem *item)
{
    WayPoint *way;
    printf("%s()\n", __PRETTY_FUNCTION__);

    if((way = find_nearest_waypoint(_cmenu_unitx, _cmenu_unity)))
        route_show_distance_to(way->point);
    else
    {
        MACRO_BANNER_SHOW_INFO(_window, _("There are no waypoints."));
    }

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
cmenu_cb_way_delete(GtkMenuItem *item)
{
    WayPoint *way;
    printf("%s()\n", __PRETTY_FUNCTION__);

    if((way = find_nearest_waypoint(_cmenu_unitx, _cmenu_unity)))
    {
        gchar buffer[BUFFER_SIZE];
        GtkWidget *confirm;

        snprintf(buffer, sizeof(buffer), "%s:\n%s\n",
                _("Confirm delete of waypoint"), way->desc);
        confirm = hildon_note_new_confirmation(GTK_WINDOW(_window), buffer);

        if(GTK_RESPONSE_OK == gtk_dialog_run(GTK_DIALOG(confirm)))
        {
            Point *pdel_min, *pdel_max, *pdel_start, *pdel_end;
            gint num_del;

            /* Delete surrounding route data, too. */
            if(way == _route.whead)
                pdel_min = _route.head;
            else
                pdel_min = way[-1].point;

            if(way == _route.wtail)
                pdel_max = _route.tail;
            else
                pdel_max = way[1].point;

            /* Find largest continuous segment around the waypoint, EXCLUDING
             * pdel_min and pdel_max. */
            for(pdel_start = way->point - 1; pdel_start->unity
                    && pdel_start > pdel_min; pdel_start--) { }
            for(pdel_end = way->point + 1; pdel_end->unity
                    && pdel_end < pdel_max; pdel_end++) { }

            /* If pdel_end is set to _route.tail, and if _route.tail is a
             * non-zero point, then delete _route.tail. */
            if(pdel_end == _route.tail && pdel_end->unity)
                pdel_end++; /* delete _route.tail too */
            /* else, if *both* endpoints are zero points, delete one. */
            else if(!pdel_start->unity && !pdel_end->unity)
                pdel_start--;

            /* Delete BETWEEN pdel_start and pdel_end, exclusive. */
            num_del = pdel_end - pdel_start - 1;

            memmove(pdel_start + 1, pdel_end,
                    (_route.tail - pdel_end + 1) * sizeof(Point));
            _route.tail -= num_del;

            /* Remove waypoint and move/adjust subsequent waypoints. */
            g_free(way->desc);
            while(way++ != _route.wtail)
            {
                way[-1] = *way;
                way[-1].point -= num_del;
            }
            _route.wtail--;

            route_find_nearest_point();
            map_force_redraw();
        }
        gtk_widget_destroy(confirm);
    }
    else
    {
        MACRO_BANNER_SHOW_INFO(_window, _("There are no waypoints."));
    }

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
cmenu_cb_way_add_poi(GtkMenuItem *item)
{
    WayPoint *way;
    printf("%s()\n", __PRETTY_FUNCTION__);

    if((way = find_nearest_waypoint(_cmenu_unitx, _cmenu_unity)))
        poi_add_dialog(_window, way->point->unitx, way->point->unity);
    else
    {
        MACRO_BANNER_SHOW_INFO(_window, _("There are no waypoints."));
    }

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
cmenu_cb_poi_route_to(GtkMenuItem *item)
{
    PoiInfo poi;
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(select_poi(_cmenu_unitx, _cmenu_unity, &poi, FALSE))
        /* FALSE = not quick */
    {
        gint unitx, unity;
        latlon2unit(poi.lat, poi.lon, unitx, unity);
        cmenu_route_to(unitx, unity);
    }

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
cmenu_cb_poi_distance_to(GtkMenuItem *item)
{
    PoiInfo poi;
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(select_poi(_cmenu_unitx, _cmenu_unity, &poi, FALSE))
    {
        gint unitx, unity;
        latlon2unit(poi.lat, poi.lon, unitx, unity);
        cmenu_distance_to(unitx, unity);
    }

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
cmenu_cb_poi_add_route(GtkMenuItem *item)
{
    PoiInfo poi;
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(select_poi(_cmenu_unitx, _cmenu_unity, &poi, FALSE))
    {
        gint unitx, unity;
        latlon2unit(poi.lat, poi.lon, unitx, unity);
        cmenu_add_route(unitx, unity);
    }

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
cmenu_cb_poi_add_way(GtkMenuItem *item)
{
    PoiInfo poi;
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(select_poi(_cmenu_unitx, _cmenu_unity, &poi, FALSE))
    {
        gint unitx, unity;
        latlon2unit(poi.lat, poi.lon, unitx, unity);
        route_add_way_dialog(unitx, unity);
    }

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
cmenu_cb_poi_edit_poi(GtkMenuItem *item)
{
    PoiInfo poi;
    printf("%s()\n", __PRETTY_FUNCTION__);

    memset(&poi, 0, sizeof(poi));
    select_poi(_cmenu_unitx, _cmenu_unity, &poi, FALSE);
    poi_view_dialog(_window, &poi);
    if(poi.label)
        g_free(poi.label);
    if(poi.desc)
        g_free(poi.desc);
    if(poi.clabel)
        g_free(poi.clabel);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
cmenu_cb_hide(GtkMenuItem *item)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(_mouse_is_down)
        g_mutex_unlock(_mouse_mutex);
    _mouse_is_down = _mouse_is_dragging = FALSE;

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

void cmenu_init()
{
    /* Create needed handles. */
    GtkWidget *submenu;
    GtkWidget *menu_item;
    printf("%s()\n", __PRETTY_FUNCTION__);

    /* Setup the context menu. */
    _map_cmenu = GTK_MENU(gtk_menu_new());

    /* Setup the map context menu. */
    gtk_menu_append(_map_cmenu, menu_item
            = gtk_menu_item_new_with_label(_("Tap Point")));
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item),
            submenu = gtk_menu_new());

    /* Setup the map context menu. */
    gtk_menu_append(submenu, _cmenu_loc_show_latlon_item
            = gtk_menu_item_new_with_label(_("Show Position")));
    gtk_menu_append(submenu, gtk_separator_menu_item_new());
    gtk_menu_append(submenu, _cmenu_loc_distance_to_item
            = gtk_menu_item_new_with_label(_("Show Distance to")));
    gtk_menu_append(submenu, _cmenu_loc_route_to_item
            = gtk_menu_item_new_with_label(_("Download Route to...")));
    gtk_menu_append(submenu, _cmenu_loc_download_poi_item
                = gtk_menu_item_new_with_label(_("Download POI...")));
    gtk_menu_append(submenu, _cmenu_loc_browse_poi_item
                = gtk_menu_item_new_with_label(_("Browse POI...")));
    gtk_menu_append(submenu, gtk_separator_menu_item_new());
    gtk_menu_append(submenu, _cmenu_loc_add_route_item
                = gtk_menu_item_new_with_label(_("Add Route Point")));
    gtk_menu_append(submenu, _cmenu_loc_add_way_item
                = gtk_menu_item_new_with_label(_("Add Waypoint...")));
    gtk_menu_append(submenu, _cmenu_loc_add_poi_item
                = gtk_menu_item_new_with_label(_("Add POI...")));
    gtk_menu_append(submenu, gtk_separator_menu_item_new());
    gtk_menu_append(submenu, _cmenu_loc_set_gps_item
                = gtk_menu_item_new_with_label(_("Set as GPS Location")));
    gtk_menu_append(submenu, _cmenu_loc_apply_correction_item
                = gtk_check_menu_item_new_with_label(
                    _("Apply Map Correction")));
    gtk_check_menu_item_set_active(
            GTK_CHECK_MENU_ITEM(_cmenu_loc_apply_correction_item),
            _map_correction_unitx != 0 || _map_correction_unity != 0);

    /* Setup the waypoint context menu. */
    gtk_menu_append(_map_cmenu, menu_item
            = gtk_menu_item_new_with_label(_("Waypoint")));
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item),
            submenu = gtk_menu_new());

    gtk_menu_append(submenu, _cmenu_way_show_latlon_item
            = gtk_menu_item_new_with_label(_("Show Position")));
    gtk_menu_append(submenu, _cmenu_way_show_desc_item
            = gtk_menu_item_new_with_label(_("Show Description")));
    gtk_menu_append(submenu, _cmenu_way_clip_latlon_item
            = gtk_menu_item_new_with_label(_("Copy Position")));
    gtk_menu_append(submenu, _cmenu_way_clip_desc_item
            = gtk_menu_item_new_with_label(_("Copy Description")));
    gtk_menu_append(submenu, gtk_separator_menu_item_new());
    gtk_menu_append(submenu, _cmenu_way_distance_to_item
            = gtk_menu_item_new_with_label(_("Show Distance to")));
    gtk_menu_append(submenu, _cmenu_way_route_to_item
            = gtk_menu_item_new_with_label(_("Download Route to...")));
    gtk_menu_append(submenu, _cmenu_way_delete_item
            = gtk_menu_item_new_with_label(_("Delete...")));
    gtk_menu_append(submenu, gtk_separator_menu_item_new());
    gtk_menu_append(submenu, _cmenu_way_add_poi_item
                = gtk_menu_item_new_with_label(_("Add POI...")));
    gtk_menu_append(submenu, gtk_separator_menu_item_new());
    gtk_menu_append(submenu, _cmenu_way_goto_nextway_item
                = gtk_menu_item_new_with_label(_("Go to Next")));

    /* Setup the POI context menu. */
    gtk_menu_append(_map_cmenu, _cmenu_poi_submenu
            = gtk_menu_item_new_with_label(_("POI")));
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(_cmenu_poi_submenu),
            submenu = gtk_menu_new());

    gtk_menu_append(submenu, _cmenu_poi_edit_poi_item
                = gtk_menu_item_new_with_label(_("View/Edit...")));
    gtk_menu_append(submenu, gtk_separator_menu_item_new());
    gtk_menu_append(submenu, _cmenu_poi_distance_to_item
            = gtk_menu_item_new_with_label(_("Show Distance to")));
    gtk_menu_append(submenu, _cmenu_poi_route_to_item
            = gtk_menu_item_new_with_label(_("Download Route to...")));
    gtk_menu_append(submenu, gtk_separator_menu_item_new());
    gtk_menu_append(submenu, _cmenu_poi_add_route_item
                = gtk_menu_item_new_with_label(_("Add Route Point")));
    gtk_menu_append(submenu, _cmenu_poi_add_way_item
                = gtk_menu_item_new_with_label(_("Add Waypoint...")));
    gtk_menu_append(submenu, gtk_separator_menu_item_new());
    gtk_menu_append(submenu, _cmenu_poi_goto_nearpoi_item
                = gtk_menu_item_new_with_label(_("Go to Nearest")));

    /* Connect signals for context menu. */
    g_signal_connect(G_OBJECT(_cmenu_loc_show_latlon_item), "activate",
                      G_CALLBACK(cmenu_cb_loc_show_latlon), NULL);
    g_signal_connect(G_OBJECT(_cmenu_loc_route_to_item), "activate",
                      G_CALLBACK(cmenu_cb_loc_route_to), NULL);
    g_signal_connect(G_OBJECT(_cmenu_loc_download_poi_item), "activate",
                      G_CALLBACK(cmenu_cb_loc_download_poi), NULL);
    g_signal_connect(G_OBJECT(_cmenu_loc_browse_poi_item), "activate",
                      G_CALLBACK(cmenu_cb_loc_browse_poi), NULL);
    g_signal_connect(G_OBJECT(_cmenu_loc_distance_to_item), "activate",
                      G_CALLBACK(cmenu_cb_loc_distance_to), NULL);
    g_signal_connect(G_OBJECT(_cmenu_loc_add_route_item), "activate",
                        G_CALLBACK(cmenu_cb_loc_add_route), NULL);
    g_signal_connect(G_OBJECT(_cmenu_loc_add_way_item), "activate",
                        G_CALLBACK(cmenu_cb_loc_add_way), NULL);
    g_signal_connect(G_OBJECT(_cmenu_loc_add_poi_item), "activate",
                        G_CALLBACK(cmenu_cb_loc_add_poi), NULL);
    g_signal_connect(G_OBJECT(_cmenu_loc_set_gps_item), "activate",
                        G_CALLBACK(cmenu_cb_loc_set_gps), NULL);
    g_signal_connect(G_OBJECT(_cmenu_loc_apply_correction_item), "toggled",
                        G_CALLBACK(cmenu_cb_loc_apply_correction), NULL);

    g_signal_connect(G_OBJECT(_cmenu_way_show_latlon_item), "activate",
                      G_CALLBACK(cmenu_cb_way_show_latlon), NULL);
    g_signal_connect(G_OBJECT(_cmenu_way_show_desc_item), "activate",
                      G_CALLBACK(cmenu_cb_way_show_desc), NULL);
    g_signal_connect(G_OBJECT(_cmenu_way_clip_latlon_item), "activate",
                      G_CALLBACK(cmenu_cb_way_clip_latlon), NULL);
    g_signal_connect(G_OBJECT(_cmenu_way_clip_desc_item), "activate",
                      G_CALLBACK(cmenu_cb_way_clip_desc), NULL);
    g_signal_connect(G_OBJECT(_cmenu_way_route_to_item), "activate",
                      G_CALLBACK(cmenu_cb_way_route_to), NULL);
    g_signal_connect(G_OBJECT(_cmenu_way_distance_to_item), "activate",
                      G_CALLBACK(cmenu_cb_way_distance_to), NULL);
    g_signal_connect(G_OBJECT(_cmenu_way_delete_item), "activate",
                      G_CALLBACK(cmenu_cb_way_delete), NULL);
    g_signal_connect(G_OBJECT(_cmenu_way_add_poi_item), "activate",
                        G_CALLBACK(cmenu_cb_way_add_poi), NULL);
    g_signal_connect(G_OBJECT(_cmenu_way_goto_nextway_item), "activate",
                        G_CALLBACK(menu_cb_view_goto_nextway), NULL);

    g_signal_connect(G_OBJECT(_cmenu_poi_edit_poi_item), "activate",
                        G_CALLBACK(cmenu_cb_poi_edit_poi), NULL);
    g_signal_connect(G_OBJECT(_cmenu_poi_route_to_item), "activate",
                      G_CALLBACK(cmenu_cb_poi_route_to), NULL);
    g_signal_connect(G_OBJECT(_cmenu_poi_distance_to_item), "activate",
                      G_CALLBACK(cmenu_cb_poi_distance_to), NULL);
    g_signal_connect(G_OBJECT(_cmenu_poi_add_route_item), "activate",
                        G_CALLBACK(cmenu_cb_poi_add_route), NULL);
    g_signal_connect(G_OBJECT(_cmenu_poi_add_way_item), "activate",
                        G_CALLBACK(cmenu_cb_poi_add_way), NULL);
    g_signal_connect(G_OBJECT(_cmenu_poi_goto_nearpoi_item), "activate",
                        G_CALLBACK(menu_cb_view_goto_nearpoi), NULL);

    gtk_widget_show_all(GTK_WIDGET(_map_cmenu));

#ifdef MAEMO_CHANGES
    gtk_widget_tap_and_hold_setup(_map_widget, GTK_WIDGET(_map_cmenu), NULL, 0);
#endif

    /* Add a "hide" signal event handler to handle dismissing the context
     * menu. */
    g_signal_connect(GTK_WIDGET(_map_cmenu), "hide",
            G_CALLBACK(cmenu_cb_hide), NULL);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

static void
on_apply_correction_toggled(GtkToggleButton *button, Point *p)
{
    GtkWidget *dialog;

    if (gtk_toggle_button_get_active(button))
    {
        /* Get difference between tap point and GPS location. */
        _map_correction_unitx = p->unitx - _pos.unitx;
        _map_correction_unity = p->unity - _pos.unity;
        map_refresh_mark(TRUE);
        MACRO_BANNER_SHOW_INFO(_window, _("Map correction applied."));
    }
    else
    {
        _map_correction_unitx = 0;
        _map_correction_unity = 0;
        map_refresh_mark(TRUE);
        MACRO_BANNER_SHOW_INFO(_window, _("Map correction removed."));
    }

    g_debug("Map correction now set to: %d, %d",
            _map_correction_unitx, _map_correction_unity);
    /* close the dialog */
    dialog = gtk_widget_get_toplevel((GtkWidget *)button);
    gtk_dialog_response(GTK_DIALOG(dialog), GTK_RESPONSE_CLOSE);
}

void
map_menu_point_map(Point *p)
{
    GtkWidget *dialog, *button;
    MapController *controller;
    MapDialog *dlg;
    gint response;
    enum {
        SHOW_LATLON = 0,
        DISTANCE_TO,
        ROUTE_TO,
        DOWNLOAD_POI,
        BROWSE_POI,
        ADD_ROUTE,
        ADD_WAYPOINT,
        ADD_POI,
        GPS_LOCATION,
    };

    controller = map_controller_get_instance();
    dialog = map_dialog_new(_("Map Point"),
                            map_controller_get_main_window(controller),
                            FALSE);
    dlg = (MapDialog *)dialog;

    /* TODO: rewrite these handlers to use the Point */
    _cmenu_unitx = p->unitx;
    _cmenu_unity = p->unity;

    button = map_dialog_create_button(dlg, _("Show Position"), SHOW_LATLON);

    button = map_dialog_create_button(dlg, _("Show Distance to"), DISTANCE_TO);

    button = map_dialog_create_button(dlg, _("Download Route to..."), ROUTE_TO);

    button = map_dialog_create_button(dlg, _("Download POI..."), DOWNLOAD_POI);

    button = map_dialog_create_button(dlg, _("Browse POI..."), BROWSE_POI);

    button = map_dialog_create_button(dlg, _("Add Route Point"), ADD_ROUTE);

    button = map_dialog_create_button(dlg, _("Add Waypoint..."), ADD_WAYPOINT);

    button = map_dialog_create_button(dlg, _("Add POI..."), ADD_POI);

    button = map_dialog_create_button(dlg, _("Set as GPS Location"),
                                      GPS_LOCATION);

    button = gtk_toggle_button_new_with_label(_("Apply Map Correction"));
    hildon_gtk_widget_set_theme_size(button, HILDON_SIZE_FINGER_HEIGHT);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button),
                                 _map_correction_unitx != 0 ||
                                 _map_correction_unity != 0);
    gtk_widget_show(button);
    map_dialog_add_widget(dlg, button);
    g_signal_connect(button, "toggled",
                     G_CALLBACK(on_apply_correction_toggled), p);

    response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    switch (response)
    {
    case SHOW_LATLON:
        cmenu_cb_loc_show_latlon(NULL); break;
    case DISTANCE_TO:
        cmenu_cb_loc_distance_to(NULL); break;
    case ROUTE_TO:
        cmenu_cb_loc_route_to(NULL); break;
    case DOWNLOAD_POI:
        cmenu_cb_loc_download_poi(NULL); break;
    case BROWSE_POI:
        cmenu_cb_loc_browse_poi(NULL); break;
    case ADD_ROUTE:
        cmenu_cb_loc_add_route(NULL); break;
    case ADD_WAYPOINT:
        cmenu_cb_loc_add_way(NULL); break;
    case ADD_POI:
        cmenu_cb_loc_add_poi(NULL); break;
    case GPS_LOCATION:
        cmenu_cb_loc_set_gps(NULL); break;
    }
}

