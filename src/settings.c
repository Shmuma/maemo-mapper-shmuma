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

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <dbus/dbus-glib.h>
#include <bt-dbus.h>
#include <gconf/gconf-client.h>

#ifndef LEGACY
#    include <hildon/hildon-help.h>
#    include <hildon/hildon-note.h>
#    include <hildon/hildon-color-button.h>
#    include <hildon/hildon-file-chooser-dialog.h>
#    include <hildon/hildon-number-editor.h>
#    include <hildon/hildon-banner.h>
#else
#    include <osso-helplib.h>
#    include <hildon-widgets/hildon-note.h>
#    include <hildon-widgets/hildon-color-button.h>
#    include <hildon-widgets/hildon-file-chooser-dialog.h>
#    include <hildon-widgets/hildon-number-editor.h>
#    include <hildon-widgets/hildon-banner.h>
#    include <hildon-widgets/hildon-input-mode-hint.h>
#endif

#include "types.h"
#include "data.h"
#include "defines.h"

#include "gps.h"
#include "display.h"
#include "gdk-pixbuf-rotate.h"
#include "maps.h"
#include "marshal.h"
#include "poi.h"
#include "settings.h"
#include "util.h"

#define GCONF_KEY_PREFIX "/apps/maemo/maemo-mapper"
#define GCONF_KEY_GPS_RCVR_TYPE GCONF_KEY_PREFIX"/gps_rcvr_type"
#define GCONF_KEY_GPS_BT_MAC GCONF_KEY_PREFIX"/receiver_mac"
#define GCONF_KEY_GPS_GPSD_HOST GCONF_KEY_PREFIX"/gps_gpsd_host"
#define GCONF_KEY_GPS_GPSD_PORT GCONF_KEY_PREFIX"/gps_gpsd_port"
#define GCONF_KEY_GPS_FILE_PATH GCONF_KEY_PREFIX"/gps_file_path"
#define GCONF_KEY_AUTO_DOWNLOAD GCONF_KEY_PREFIX"/auto_download"
#define GCONF_KEY_AUTO_DOWNLOAD_PRECACHE \
                                      GCONF_KEY_PREFIX"/auto_download_precache"
#define GCONF_KEY_CENTER_SENSITIVITY GCONF_KEY_PREFIX"/center_sensitivity"
#define GCONF_KEY_ENABLE_ANNOUNCE GCONF_KEY_PREFIX"/enable_announce"
#define GCONF_KEY_ANNOUNCE_NOTICE GCONF_KEY_PREFIX"/announce_notice"
#define GCONF_KEY_ROTATE_SENSITIVITY GCONF_KEY_PREFIX"/rotate_sensitivity"
#define GCONF_KEY_AC_MIN_SPEED GCONF_KEY_PREFIX"/autocenter_min_speed"
#define GCONF_KEY_ROTATE_DIR GCONF_KEY_PREFIX"/rotate_direction"
#define GCONF_KEY_DRAW_WIDTH GCONF_KEY_PREFIX"/draw_width"
#define GCONF_KEY_ENABLE_VOICE GCONF_KEY_PREFIX"/enable_voice"
#define GCONF_KEY_VOICE_SPEED GCONF_KEY_PREFIX"/voice_speed"
#define GCONF_KEY_VOICE_PITCH GCONF_KEY_PREFIX"/voice_pitch"
#define GCONF_KEY_FULLSCREEN GCONF_KEY_PREFIX"/fullscreen"
#define GCONF_KEY_UNITS GCONF_KEY_PREFIX"/units"
#define GCONF_KEY_SPEED_LIMIT_ON GCONF_KEY_PREFIX"/speed_limit_on"
#define GCONF_KEY_SPEED_LIMIT GCONF_KEY_PREFIX"/speed_limit"
#define GCONF_KEY_SPEED_LOCATION GCONF_KEY_PREFIX"/speed_location"
#define GCONF_KEY_UNBLANK_SIZE GCONF_KEY_PREFIX"/unblank_option"
#define GCONF_KEY_INFO_FONT_SIZE GCONF_KEY_PREFIX"/info_font_size"

#define GCONF_KEY_POI_DB GCONF_KEY_PREFIX"/poi_db"
#define GCONF_KEY_POI_ZOOM GCONF_KEY_PREFIX"/poi_zoom"

#define GCONF_KEY_AUTOCENTER_MODE GCONF_KEY_PREFIX"/autocenter_mode"
#define GCONF_KEY_AUTOCENTER_ROTATE GCONF_KEY_PREFIX"/autocenter_rotate"
#define GCONF_KEY_LEAD_AMOUNT GCONF_KEY_PREFIX"/lead_amount"
#define GCONF_KEY_LEAD_IS_FIXED GCONF_KEY_PREFIX"/lead_is_fixed"
#define GCONF_KEY_LAST_LAT GCONF_KEY_PREFIX"/last_latitude"
#define GCONF_KEY_LAST_LON GCONF_KEY_PREFIX"/last_longitude"
#define GCONF_KEY_LAST_ALT GCONF_KEY_PREFIX"/last_altitude"
#define GCONF_KEY_LAST_SPEED GCONF_KEY_PREFIX"/last_speed"
#define GCONF_KEY_LAST_HEADING GCONF_KEY_PREFIX"/last_heading"
#define GCONF_KEY_LAST_TIME GCONF_KEY_PREFIX"/last_timestamp"
#define GCONF_KEY_CENTER_LAT GCONF_KEY_PREFIX"/center_latitude"
#define GCONF_KEY_CENTER_LON GCONF_KEY_PREFIX"/center_longitude"
#define GCONF_KEY_CENTER_ANGLE GCONF_KEY_PREFIX"/center_angle"
#define GCONF_KEY_MAP_CORRECTION_UNITX GCONF_KEY_PREFIX"/map_correction_unitx"
#define GCONF_KEY_MAP_CORRECTION_UNITY GCONF_KEY_PREFIX"/map_correction_unity"
#define GCONF_KEY_ZOOM GCONF_KEY_PREFIX"/zoom"
#define GCONF_KEY_ROUTEDIR GCONF_KEY_PREFIX"/route_directory"
#define GCONF_KEY_TRACKFILE GCONF_KEY_PREFIX"/track_file"
#define GCONF_KEY_SHOWZOOMLEVEL GCONF_KEY_PREFIX"/show_zoomlevel"
#define GCONF_KEY_SHOWSCALE GCONF_KEY_PREFIX"/show_scale"
#define GCONF_KEY_SHOWCOMPROSE GCONF_KEY_PREFIX"/show_comprose"
#define GCONF_KEY_SHOWTRACKS GCONF_KEY_PREFIX"/show_tracks"
#define GCONF_KEY_SHOWROUTES GCONF_KEY_PREFIX"/show_routes"
#define GCONF_KEY_SHOWVELVEC GCONF_KEY_PREFIX"/show_velocity_vector" 
#define GCONF_KEY_SHOWPOIS GCONF_KEY_PREFIX"/show_poi" 
#define GCONF_KEY_ENABLE_GPS GCONF_KEY_PREFIX"/enable_gps" 
#define GCONF_KEY_ENABLE_TRACKING GCONF_KEY_PREFIX"/enable_tracking"
#define GCONF_KEY_ROUTE_LOCATIONS GCONF_KEY_PREFIX"/route_locations" 
#define GCONF_KEY_REPOSITORIES GCONF_KEY_PREFIX"/repositories" 
#define GCONF_KEY_CURRREPO GCONF_KEY_PREFIX"/curr_repo" 
#define GCONF_KEY_GPS_INFO GCONF_KEY_PREFIX"/gps_info" 
#define GCONF_KEY_ROUTE_DL_URL GCONF_KEY_PREFIX"/route_dl_url" 
#define GCONF_KEY_ROUTE_DL_RADIUS GCONF_KEY_PREFIX"/route_dl_radius" 
#define GCONF_KEY_POI_DL_URL GCONF_KEY_PREFIX"/poi_dl_url" 
#define GCONF_KEY_DEG_FORMAT GCONF_KEY_PREFIX"/deg_format" 


typedef struct _ScanInfo ScanInfo;
struct _ScanInfo {
    GtkWidget *settings_dialog;
    GtkWidget *txt_gps_bt_mac;
    GtkWidget *scan_dialog;
    GtkWidget *banner;
    GtkListStore *store;
    gint sid;
    DBusGConnection *bus;
    DBusGProxy *req_proxy;
    DBusGProxy *sig_proxy;
};

typedef struct _KeysDialogInfo KeysDialogInfo;
struct _KeysDialogInfo {
    GtkWidget *cmb[CUSTOM_KEY_ENUM_COUNT];
};

typedef struct _ColorsDialogInfo ColorsDialogInfo;
struct _ColorsDialogInfo {
    GtkWidget *col[COLORABLE_ENUM_COUNT];
};


/**
 * Save all configuration data to GCONF.
 */
void
settings_save()
{
    gchar *settings_dir;
    GConfClient *gconf_client;
    gchar buffer[16];
    printf("%s()\n", __PRETTY_FUNCTION__);

    /* Initialize settings_dir. */
    settings_dir = gnome_vfs_expand_initial_tilde(CONFIG_DIR_NAME);
    g_mkdir_with_parents(settings_dir, 0700);

    /* SAVE ALL GCONF SETTINGS. */

    gconf_client = gconf_client_get_default();
    if(!gconf_client)
    {
        popup_error(_window,
                _("Failed to initialize GConf.  Settings were not saved."));
        return;
    }

    /* Save GPS Receiver Type. */
    gconf_client_set_string(gconf_client,
            GCONF_KEY_GPS_RCVR_TYPE,
            GPS_RCVR_ENUM_TEXT[_gri.type], NULL);

    /* Save Bluetooth Receiver MAC. */
    if(_gri.bt_mac)
        gconf_client_set_string(gconf_client,
                GCONF_KEY_GPS_BT_MAC, _gri.bt_mac, NULL);
    else
        gconf_client_unset(gconf_client,
                GCONF_KEY_GPS_BT_MAC, NULL);

    /* Save GPSD Host. */
    if(_gri.gpsd_host)
        gconf_client_set_string(gconf_client,
                GCONF_KEY_GPS_GPSD_HOST, _gri.gpsd_host, NULL);
    else
        gconf_client_unset(gconf_client,
                GCONF_KEY_GPS_GPSD_HOST, NULL);

    /* Save GPSD Port. */
    gconf_client_set_int(gconf_client,
            GCONF_KEY_GPS_GPSD_PORT, _gri.gpsd_port, NULL);

    /* Save File Path. */
    if(_gri.file_path)
        gconf_client_set_string(gconf_client,
                GCONF_KEY_GPS_FILE_PATH, _gri.file_path, NULL);
    else
        gconf_client_unset(gconf_client,
                GCONF_KEY_GPS_FILE_PATH, NULL);

    /* Save Auto-Download. */
    gconf_client_set_bool(gconf_client,
            GCONF_KEY_AUTO_DOWNLOAD, _auto_download, NULL);

    /* Save Auto-Download Pre-cache. */
    gconf_client_set_int(gconf_client,
            GCONF_KEY_AUTO_DOWNLOAD_PRECACHE, _auto_download_precache, NULL);

    /* Save Auto-Center Sensitivity. */
    gconf_client_set_int(gconf_client,
            GCONF_KEY_CENTER_SENSITIVITY, _center_ratio, NULL);

    /* Save Auto-Center Lead Amount. */
    gconf_client_set_int(gconf_client,
            GCONF_KEY_LEAD_AMOUNT, _lead_ratio, NULL);

    /* Save Auto-Center Lead Fixed flag. */
    gconf_client_set_bool(gconf_client,
            GCONF_KEY_LEAD_IS_FIXED, _lead_is_fixed, NULL);

    /* Save Auto-Rotate Sensitivity. */
    gconf_client_set_int(gconf_client,
            GCONF_KEY_ROTATE_SENSITIVITY, _rotate_sens, NULL);

    /* Save Auto-Center/Rotate Minimum Speed. */
    gconf_client_set_int(gconf_client,
            GCONF_KEY_AC_MIN_SPEED, _ac_min_speed, NULL);

    /* Save Auto-Rotate Sensitivity. */
    gconf_client_set_string(gconf_client,
            GCONF_KEY_ROTATE_DIR, ROTATE_DIR_ENUM_TEXT[_rotate_dir], NULL);

    /* Save Draw Line Width. */
    gconf_client_set_int(gconf_client,
            GCONF_KEY_DRAW_WIDTH, _draw_width, NULL);

    /* Save Announce Flag. */
    gconf_client_set_bool(gconf_client,
            GCONF_KEY_ENABLE_ANNOUNCE, _enable_announce, NULL);

    /* Save Announce Advance Notice Ratio. */
    gconf_client_set_int(gconf_client,
            GCONF_KEY_ANNOUNCE_NOTICE, _announce_notice_ratio, NULL);

    /* Save Enable Voice flag. */
    gconf_client_set_bool(gconf_client,
            GCONF_KEY_ENABLE_VOICE, _enable_voice, NULL);

    /* Save fullscreen flag. */
    gconf_client_set_bool(gconf_client,
            GCONF_KEY_FULLSCREEN, _fullscreen, NULL);

    /* Save Units. */
    gconf_client_set_string(gconf_client,
            GCONF_KEY_UNITS, UNITS_ENUM_TEXT[_units], NULL);

    /* Save Custom Key Actions. */
    {
        gint i;
        for(i = 0; i < CUSTOM_KEY_ENUM_COUNT; i++)
            gconf_client_set_string(gconf_client,
                    CUSTOM_KEY_GCONF[i],
                    CUSTOM_ACTION_ENUM_TEXT[_action[i]], NULL);
    }

    /* Save Deg Format. */
    gconf_client_set_string(gconf_client,
            GCONF_KEY_DEG_FORMAT, DEG_FORMAT_ENUM_TEXT[_degformat], NULL);

    /* Save Speed Limit On flag. */
    gconf_client_set_bool(gconf_client,
            GCONF_KEY_SPEED_LIMIT_ON, _speed_limit_on, NULL);

    /* Save Speed Limit. */
    gconf_client_set_int(gconf_client,
            GCONF_KEY_SPEED_LIMIT, _speed_limit, NULL);

    /* Save Speed Location. */
    gconf_client_set_string(gconf_client,
            GCONF_KEY_SPEED_LOCATION,
            SPEED_LOCATION_ENUM_TEXT[_speed_location], NULL);

    /* Save Info Font Size. */
    gconf_client_set_string(gconf_client,
            GCONF_KEY_INFO_FONT_SIZE,
            INFO_FONT_ENUM_TEXT[_info_font_size], NULL);

    /* Save Unblank Option. */
    gconf_client_set_string(gconf_client,
            GCONF_KEY_UNBLANK_SIZE,
            UNBLANK_ENUM_TEXT[_unblank_option], NULL);

    /* Save last saved latitude. */
    gconf_client_set_float(gconf_client,
            GCONF_KEY_LAST_LAT, _gps.lat, NULL);

    /* Save last saved longitude. */
    gconf_client_set_float(gconf_client,
            GCONF_KEY_LAST_LON, _gps.lon, NULL);

    /* Save last saved altitude. */
    gconf_client_set_int(gconf_client,
            GCONF_KEY_LAST_ALT, _pos.altitude, NULL);

    /* Save last saved speed. */
    gconf_client_set_float(gconf_client,
            GCONF_KEY_LAST_SPEED, _gps.speed, NULL);

    /* Save last saved heading. */
    gconf_client_set_float(gconf_client,
            GCONF_KEY_LAST_HEADING, _gps.heading, NULL);

    /* Save last saved timestamp. */
    gconf_client_set_float(gconf_client,
            GCONF_KEY_LAST_TIME, _pos.time, NULL);

    /* Save last center point. */
    {
        gdouble center_lat, center_lon;
        unit2latlon(_center.unitx, _center.unity, &center_lat, &center_lon, _curr_repo->units);

        /* Save last center latitude. */
        gconf_client_set_float(gconf_client,
                GCONF_KEY_CENTER_LAT, center_lat, NULL);

        /* Save last center longitude. */
        gconf_client_set_float(gconf_client,
                GCONF_KEY_CENTER_LON, center_lon, NULL);

        /* Save last view angle. */
        gconf_client_set_int(gconf_client,
                GCONF_KEY_CENTER_ANGLE, _map_rotate_angle, NULL);
    }

    /* Save map correction. */
    gconf_client_set_int(gconf_client,
            GCONF_KEY_MAP_CORRECTION_UNITX, _map_correction_unitx, NULL);
    gconf_client_set_int(gconf_client,
            GCONF_KEY_MAP_CORRECTION_UNITY, _map_correction_unity, NULL);

    /* Save last Zoom Level. */
    gconf_client_set_int(gconf_client,
            GCONF_KEY_ZOOM, _zoom, NULL);

    /* Save Route Directory. */
    if(_route_dir_uri)
        gconf_client_set_string(gconf_client,
                GCONF_KEY_ROUTEDIR, _route_dir_uri, NULL);

    /* Save the repositories. */
    {
        GList *curr = _repo_list;
        GSList *temp_list = NULL;
        gint curr_repo_index = 0;

        for(curr = _repo_list; curr != NULL; curr = curr->next)
        {
            /* Build from each part of a repo, delimited by newline characters:
             * 1. url
             * 2. db_filename
             * 3. dl_zoom_steps
             * 4. view_zoom_steps
             */
            RepoData *rd = curr->data;
            gchar buffer[BUFFER_SIZE];
            snprintf(buffer, sizeof(buffer),
                    "%s\t%s\t%s\t%d\t%d\t%d\t%d\t%d\t%d",
                    rd->name,
                    rd->url,
                    rd->db_filename,
                    rd->dl_zoom_steps,
                    rd->view_zoom_steps,
                    rd->double_size,
                    rd->nextable,
                    rd->min_zoom,
                    rd->max_zoom);
            temp_list = g_slist_append(temp_list, g_strdup(buffer));
            if(rd == _curr_repo)
                gconf_client_set_int(gconf_client,
                        GCONF_KEY_CURRREPO, curr_repo_index, NULL);
            curr_repo_index++;
        }
        gconf_client_set_list(gconf_client,
                GCONF_KEY_REPOSITORIES, GCONF_VALUE_STRING, temp_list, NULL);
    }

    /* Save Last Track File. */
    if(_track_file_uri)
        gconf_client_set_string(gconf_client,
                GCONF_KEY_TRACKFILE, _track_file_uri, NULL);

    /* Save Auto-Center Mode. */
    gconf_client_set_int(gconf_client,
            GCONF_KEY_AUTOCENTER_MODE, _center_mode, NULL);

    /* Save Auto-Center Rotate Flag. */
    gconf_client_set_bool(gconf_client,
            GCONF_KEY_AUTOCENTER_ROTATE, _center_rotate, NULL);

    /* Save Show Zoom Level flag. */
    gconf_client_set_bool(gconf_client,
            GCONF_KEY_SHOWZOOMLEVEL, _show_zoomlevel, NULL);

    /* Save Show Scale flag. */
    gconf_client_set_bool(gconf_client,
            GCONF_KEY_SHOWSCALE, _show_scale, NULL);

    /* Save Show Compass Rose flag. */
    gconf_client_set_bool(gconf_client,
            GCONF_KEY_SHOWCOMPROSE, _show_comprose, NULL);

    /* Save Show Tracks flag. */
    gconf_client_set_bool(gconf_client,
            GCONF_KEY_SHOWTRACKS, _show_paths & TRACKS_MASK, NULL);

    /* Save Show Routes flag. */
    gconf_client_set_bool(gconf_client,
            GCONF_KEY_SHOWROUTES, _show_paths & ROUTES_MASK, NULL);

    /* Save Show Velocity Vector flag. */
    gconf_client_set_bool(gconf_client,
            GCONF_KEY_SHOWVELVEC, _show_velvec, NULL);

    /* Save Show POIs flag. */
    gconf_client_set_bool(gconf_client,
            GCONF_KEY_SHOWPOIS, _show_poi, NULL);

    /* Save Enable GPS flag. */
    gconf_client_set_bool(gconf_client,
            GCONF_KEY_ENABLE_GPS, _enable_gps, NULL);

    /* Save Enable Tracking flag. */
    gconf_client_set_bool(gconf_client,
            GCONF_KEY_ENABLE_TRACKING, _enable_tracking, NULL);

    /* Save Route Locations. */
    gconf_client_set_list(gconf_client,
            GCONF_KEY_ROUTE_LOCATIONS, GCONF_VALUE_STRING, _loc_list, NULL);

    /* Save GPS Info flag. */
    gconf_client_set_bool(gconf_client,
            GCONF_KEY_GPS_INFO, _gps_info, NULL);

    /* Save Route Download URL Format. */
    gconf_client_set_string(gconf_client,
            GCONF_KEY_ROUTE_DL_URL, _route_dl_url, NULL);

    /* Save Route Download Radius. */
    gconf_client_set_int(gconf_client,
            GCONF_KEY_ROUTE_DL_RADIUS, _route_dl_radius, NULL);

    /* Save POI Download URL Format. */
    gconf_client_set_string(gconf_client,
            GCONF_KEY_POI_DL_URL, _poi_dl_url, NULL);

    /* Save Colors. */
    {
        gint i;
        for(i = 0; i < COLORABLE_ENUM_COUNT; i++)
        {
            snprintf(buffer, sizeof(buffer), "#%02x%02x%02x",
                    _color[i].red >> 8,
                    _color[i].green >> 8,
                    _color[i].blue >> 8);
            gconf_client_set_string(gconf_client,
                    COLORABLE_GCONF[i], buffer, NULL);
        }
    }

    /* Save POI database. */
    if(_poi_db_filename)
        gconf_client_set_string(gconf_client,
                GCONF_KEY_POI_DB, _poi_db_filename, NULL);
    else
        gconf_client_unset(gconf_client, GCONF_KEY_POI_DB, NULL);

    /* Save Show POI below zoom. */
    gconf_client_set_int(gconf_client,
            GCONF_KEY_POI_ZOOM, _poi_zoom, NULL);

    gconf_client_clear_cache(gconf_client);
    g_object_unref(gconf_client);
    g_free(settings_dir);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

static void
scan_cb_dev_found(DBusGProxy *sig_proxy, const gchar *bda,
        const gchar *name, gpointer *class, guchar rssi, gint coff,
        ScanInfo *scan_info)
{
    GtkTreeIter iter;
    printf("%s()\n", __PRETTY_FUNCTION__);
    gtk_list_store_append(scan_info->store, &iter);
    gtk_list_store_set(scan_info->store, &iter,
            0, g_strdup(bda),
            1, g_strdup(name),
            -1);
    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

static void
scan_cb_search_complete(DBusGProxy *sig_proxy, ScanInfo *scan_info)
{
    printf("%s()\n", __PRETTY_FUNCTION__);
    gtk_widget_destroy(scan_info->banner);
    dbus_g_proxy_disconnect_signal(sig_proxy, BTSEARCH_DEV_FOUND_SIG,
            G_CALLBACK(scan_cb_dev_found), scan_info);
    dbus_g_proxy_disconnect_signal(sig_proxy, BTSEARCH_SEARCH_COMPLETE_SIG,
            G_CALLBACK(scan_cb_search_complete), scan_info);
    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

static gint
scan_start_search(ScanInfo *scan_info)
{
    GError *error = NULL;
    printf("%s()\n", __PRETTY_FUNCTION__);

    /* Initialize D-Bus. */
    if(NULL == (scan_info->bus = dbus_g_bus_get(DBUS_BUS_SYSTEM, &error)))
    {
        g_printerr("Failed to open connection to D-Bus: %s.\n",
                error->message);
        return 1;
    }

    if(NULL == (scan_info->req_proxy =dbus_g_proxy_new_for_name(scan_info->bus,
            BTSEARCH_SERVICE,
            BTSEARCH_REQ_PATH,
            BTSEARCH_REQ_INTERFACE)))
    {
        g_printerr("Failed to create D-Bus request proxy for btsearch.");
        return 2;
    }

    if(NULL == (scan_info->sig_proxy =dbus_g_proxy_new_for_name(scan_info->bus,
            BTSEARCH_SERVICE,
            BTSEARCH_SIG_PATH,
            BTSEARCH_SIG_INTERFACE)))
    {
        g_printerr("Failed to create D-Bus signal proxy for btsearch.");
        return 2;
    }

    dbus_g_object_register_marshaller(
            g_cclosure_user_marshal_VOID__STRING_STRING_POINTER_UCHAR_UINT,
            G_TYPE_NONE,
            G_TYPE_STRING,
            G_TYPE_STRING,
            DBUS_TYPE_G_UCHAR_ARRAY,
            G_TYPE_UCHAR,
            G_TYPE_UINT,
            G_TYPE_INVALID);

    dbus_g_proxy_add_signal(scan_info->sig_proxy,
            BTSEARCH_DEV_FOUND_SIG,
            G_TYPE_STRING,
            G_TYPE_STRING,
            DBUS_TYPE_G_UCHAR_ARRAY,
            G_TYPE_UCHAR,
            G_TYPE_UINT,
            G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(scan_info->sig_proxy, BTSEARCH_DEV_FOUND_SIG,
            G_CALLBACK(scan_cb_dev_found), scan_info, NULL);

    dbus_g_proxy_add_signal(scan_info->sig_proxy,
            BTSEARCH_SEARCH_COMPLETE_SIG,
            G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(scan_info->sig_proxy,
            BTSEARCH_SEARCH_COMPLETE_SIG,
            G_CALLBACK(scan_cb_search_complete), scan_info, NULL);

    error = NULL;
    if(!dbus_g_proxy_call(scan_info->req_proxy, BTSEARCH_START_SEARCH_REQ,
                &error, G_TYPE_INVALID, G_TYPE_INVALID))
    {
        if(error->domain == DBUS_GERROR
                && error->code == DBUS_GERROR_REMOTE_EXCEPTION)
        {
            g_printerr("Caught remote method exception %s: %s",
                    dbus_g_error_get_name(error),
                    error->message);
        }
        else
            g_printerr("Error: %s\n", error->message);
        return 3;
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return 0;
}

/**
 * Scan for all bluetooth devices.  This method can take a few seconds,
 * during which the UI will freeze.
 */
static gboolean
scan_bluetooth(GtkWidget *widget, ScanInfo *scan_info)
{
    GError *error = NULL;
    GtkWidget *dialog = NULL;
    GtkWidget *lst_devices = NULL;
    GtkTreeViewColumn *column = NULL;
    GtkCellRenderer *renderer = NULL;
    printf("%s()\n", __PRETTY_FUNCTION__);

    dialog = gtk_dialog_new_with_buttons(_("Select Bluetooth Device"),
            GTK_WINDOW(scan_info->settings_dialog), GTK_DIALOG_MODAL,
            GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
            GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
            NULL);

    scan_info->scan_dialog = dialog;

    scan_info->store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);

    gtk_window_set_default_size(GTK_WINDOW(dialog), 500, 300);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
            lst_devices = gtk_tree_view_new_with_model(
                GTK_TREE_MODEL(scan_info->store)), TRUE, TRUE, 0);

    g_object_unref(G_OBJECT(scan_info->store));

    gtk_tree_selection_set_mode(
            gtk_tree_view_get_selection(GTK_TREE_VIEW(lst_devices)),
            GTK_SELECTION_SINGLE);
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(lst_devices), TRUE);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(
            _("MAC"), renderer, "text", 0, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(lst_devices), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(
            _("Description"), renderer, "text", 1, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(lst_devices), column);

    gtk_widget_show_all(dialog);

    scan_info->banner = hildon_banner_show_animation(dialog, NULL,
            _("Scanning for Bluetooth Devices"));

    if(scan_start_search(scan_info))
    {
        gtk_widget_destroy(scan_info->banner);
        popup_error(scan_info->settings_dialog,
                _("An error occurred while attempting to scan for "
                "bluetooth devices."));
    }
    else while(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(dialog)))
    {
        GtkTreeIter iter;
        if(gtk_tree_selection_get_selected(
                    gtk_tree_view_get_selection(GTK_TREE_VIEW(lst_devices)),
                    NULL, &iter))
        {
            gchar *mac;
            gtk_tree_model_get(GTK_TREE_MODEL(scan_info->store),
                    &iter, 0, &mac, -1);
            gtk_entry_set_text(GTK_ENTRY(scan_info->txt_gps_bt_mac), mac);
            break;
        }
        else
            popup_error(dialog,
                    _("Please select a bluetooth device from the list."));
    }

    gtk_widget_destroy(dialog);

    /* Clean up D-Bus. */
    if(scan_info->req_proxy)
    {
        dbus_g_proxy_call(scan_info->req_proxy, BTSEARCH_STOP_SEARCH_REQ,
                    &error, G_TYPE_INVALID, G_TYPE_INVALID);
        g_object_unref(scan_info->req_proxy);
        scan_info->req_proxy = NULL;
    }
    if(scan_info->sig_proxy)
    {
        g_object_unref(scan_info->sig_proxy);
        scan_info->sig_proxy = NULL;
    }
    if(scan_info->bus)
    {
        dbus_g_connection_unref(scan_info->bus);
        scan_info->bus = NULL;
    }

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
settings_dialog_browse_forfile(GtkWidget *widget, BrowseInfo *browse_info)
{
    GtkWidget *dialog;
    printf("%s()\n", __PRETTY_FUNCTION__);

    dialog = GTK_WIDGET(
            hildon_file_chooser_dialog_new(GTK_WINDOW(browse_info->dialog),
            GTK_FILE_CHOOSER_ACTION_OPEN));

    gtk_file_chooser_set_local_only(GTK_FILE_CHOOSER(dialog), TRUE);
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog),
            gtk_entry_get_text(GTK_ENTRY(browse_info->txt)));

    if(GTK_RESPONSE_OK == gtk_dialog_run(GTK_DIALOG(dialog)))
    {
        gchar *filename = gtk_file_chooser_get_filename(
                GTK_FILE_CHOOSER(dialog));
        gtk_entry_set_text(GTK_ENTRY(browse_info->txt), filename);
        g_free(filename);
    }

    gtk_widget_destroy(dialog);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
settings_dialog_hardkeys_reset(GtkWidget *widget, KeysDialogInfo *cdi)
{
    GtkWidget *confirm;
    printf("%s()\n", __PRETTY_FUNCTION__);

    confirm = hildon_note_new_confirmation(GTK_WINDOW(_window),
            _("Reset all hardware keys to their original defaults?"));

    if(GTK_RESPONSE_OK == gtk_dialog_run(GTK_DIALOG(confirm)))
    {
        gint i;
        for(i = 0; i < CUSTOM_KEY_ENUM_COUNT; i++)
            gtk_combo_box_set_active(GTK_COMBO_BOX(cdi->cmb[i]),
                    CUSTOM_KEY_DEFAULT[i]);
    }
    gtk_widget_destroy(confirm);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
settings_dialog_hardkeys(GtkWidget *widget, GtkWidget *parent)
{
    gint i;
    static GtkWidget *dialog = NULL;
    static GtkWidget *table = NULL;
    static GtkWidget *label = NULL;
    static KeysDialogInfo bdi;
    static GtkWidget *btn_defaults = NULL;
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(dialog == NULL)
    {
        dialog = gtk_dialog_new_with_buttons(_("Hardware Keys"),
                GTK_WINDOW(parent), GTK_DIALOG_MODAL,
                GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                NULL);

        gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area),
                btn_defaults = gtk_button_new_with_label(_("Reset...")));
        g_signal_connect(G_OBJECT(btn_defaults), "clicked",
                          G_CALLBACK(settings_dialog_hardkeys_reset), &bdi);

        gtk_dialog_add_button(GTK_DIALOG(dialog),
                GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT);

        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
                table = gtk_table_new(2, 9, FALSE), TRUE, TRUE, 0);
        for(i = 0; i < CUSTOM_KEY_ENUM_COUNT; i++)
        {
            gint j;
            gtk_table_attach(GTK_TABLE(table),
                    label = gtk_label_new(""),
                    0, 1, i, i + 1, GTK_FILL, 0, 2, 1);
            gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
            gtk_label_set_markup(GTK_LABEL(label), CUSTOM_KEY_ICON[i]);
            gtk_table_attach(GTK_TABLE(table),
                    bdi.cmb[i] = gtk_combo_box_new_text(),
                    1, 2, i, i + 1, GTK_FILL, 0, 2, 1);
            for(j = 0; j < CUSTOM_ACTION_ENUM_COUNT; j++)
                gtk_combo_box_append_text(GTK_COMBO_BOX(bdi.cmb[i]),
                        CUSTOM_ACTION_ENUM_TEXT[j]);
        }
    }

    /* Initialize contents of the combo boxes. */
    for(i = 0; i < CUSTOM_KEY_ENUM_COUNT; i++)
        gtk_combo_box_set_active(GTK_COMBO_BOX(bdi.cmb[i]), _action[i]);

    gtk_widget_show_all(dialog);

OUTER_WHILE:
    while(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(dialog)))
    {
        /* Check for duplicates. */
        for(i = 0; i < CUSTOM_KEY_ENUM_COUNT; i++)
        {
            gint j;
            for(j = i + 1; j < CUSTOM_KEY_ENUM_COUNT; j++)
            {
                if(gtk_combo_box_get_active(GTK_COMBO_BOX(bdi.cmb[i]))
                        == gtk_combo_box_get_active(GTK_COMBO_BOX(bdi.cmb[j])))
                {
                    GtkWidget *confirm;
                    gchar *buffer = g_strdup_printf("%s:\n    %s\n%s",
                        _("The following action is mapped to multiple keys"),
                        CUSTOM_ACTION_ENUM_TEXT[gtk_combo_box_get_active(
                            GTK_COMBO_BOX(bdi.cmb[i]))],
                        _("Continue?"));
                    confirm = hildon_note_new_confirmation(GTK_WINDOW(_window),
                            buffer);

                    if(GTK_RESPONSE_OK != gtk_dialog_run(GTK_DIALOG(confirm)))
                    {
                        gtk_widget_destroy(confirm);
                        goto OUTER_WHILE;
                    }
                    gtk_widget_destroy(confirm);
                }
            }
        }
        for(i = 0; i < CUSTOM_KEY_ENUM_COUNT; i++)
            _action[i] = gtk_combo_box_get_active(GTK_COMBO_BOX(bdi.cmb[i]));
        break;
    }

    gtk_widget_hide(dialog);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
settings_dialog_colors_reset(GtkWidget *widget, ColorsDialogInfo *cdi)
{
    GtkWidget *confirm;
    printf("%s()\n", __PRETTY_FUNCTION__);

    confirm = hildon_note_new_confirmation(GTK_WINDOW(_window),
            _("Reset all colors to their original defaults?"));

    if(GTK_RESPONSE_OK == gtk_dialog_run(GTK_DIALOG(confirm)))
    {
        gint i;
        for(i = 0; i < COLORABLE_ENUM_COUNT; i++)
        {
            hildon_color_button_set_color(
                    HILDON_COLOR_BUTTON(cdi->col[i]),
                    &COLORABLE_DEFAULT[i]);
        }
    }
    gtk_widget_destroy(confirm);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
settings_dialog_colors(GtkWidget *widget, GtkWidget *parent)
{
    static GtkWidget *dialog = NULL;
    static GtkWidget *table = NULL;
    static GtkWidget *label = NULL;
    static GtkWidget *btn_defaults = NULL;
    static ColorsDialogInfo cdi;
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(dialog == NULL)
    {
        dialog = gtk_dialog_new_with_buttons(_("Colors"),
                GTK_WINDOW(parent), GTK_DIALOG_MODAL,
                GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                NULL);

        gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area),
                btn_defaults = gtk_button_new_with_label(_("Reset...")));
        g_signal_connect(G_OBJECT(btn_defaults), "clicked",
                          G_CALLBACK(settings_dialog_colors_reset), &cdi);

        gtk_dialog_add_button(GTK_DIALOG(dialog),
                GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT);

        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
                table = gtk_table_new(4, 3, FALSE), TRUE, TRUE, 0);

        /* GPS. */
        gtk_table_attach(GTK_TABLE(table),
                label = gtk_label_new(_("GPS")),
                0, 1, 0, 1, GTK_FILL, 0, 2, 4);
        gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
        gtk_table_attach(GTK_TABLE(table),
                cdi.col[COLORABLE_MARK] = hildon_color_button_new(),
                1, 2, 0, 1, 0, 0, 2, 4);
        gtk_table_attach(GTK_TABLE(table),
                cdi.col[COLORABLE_MARK_VELOCITY] = hildon_color_button_new(),
                2, 3, 0, 1, 0, 0, 2, 4);
        gtk_table_attach(GTK_TABLE(table),
                cdi.col[COLORABLE_MARK_OLD] = hildon_color_button_new(),
                3, 4, 0, 1, 0, 0, 2, 4);

        /* Track. */
        gtk_table_attach(GTK_TABLE(table),
                label = gtk_label_new(_("Track")),
                0, 1, 1, 2, GTK_FILL, 0, 2, 4);
        gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
        gtk_table_attach(GTK_TABLE(table),
                cdi.col[COLORABLE_TRACK] = hildon_color_button_new(),
                1, 2, 1, 2, 0, 0, 2, 4);
        gtk_table_attach(GTK_TABLE(table),
                cdi.col[COLORABLE_TRACK_MARK] = hildon_color_button_new(),
                2, 3, 1, 2, 0, 0, 2, 4);
        gtk_table_attach(GTK_TABLE(table),
                cdi.col[COLORABLE_TRACK_BREAK] = hildon_color_button_new(),
                3, 4, 1, 2, 0, 0, 2, 4);

        /* Route. */
        gtk_table_attach(GTK_TABLE(table),
                label = gtk_label_new(_("Route")),
                0, 1, 2, 3, GTK_FILL, 0, 2, 4);
        gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
        gtk_table_attach(GTK_TABLE(table),
                cdi.col[COLORABLE_ROUTE] = hildon_color_button_new(),
                1, 2, 2, 3, 0, 0, 2, 4);
        gtk_table_attach(GTK_TABLE(table),
                cdi.col[COLORABLE_ROUTE_WAY] = hildon_color_button_new(),
                2, 3, 2, 3, 0, 0, 2, 4);
        gtk_table_attach(GTK_TABLE(table),
                cdi.col[COLORABLE_ROUTE_BREAK] = hildon_color_button_new(),
                3, 4, 2, 3, 0, 0, 2, 4);

        /* POI. */
        gtk_table_attach(GTK_TABLE(table),
                label = gtk_label_new(_("POI")),
                0, 1, 3, 4, GTK_FILL, 0, 2, 4);
        gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
        gtk_table_attach(GTK_TABLE(table),
                cdi.col[COLORABLE_POI] = hildon_color_button_new(),
                1, 2, 3, 4, 0, 0, 2, 4);
    }

    /* Initialize GPS. */
    hildon_color_button_set_color(
            HILDON_COLOR_BUTTON(cdi.col[COLORABLE_MARK]),
            &_color[COLORABLE_MARK]);
    hildon_color_button_set_color(
            HILDON_COLOR_BUTTON(cdi.col[COLORABLE_MARK_VELOCITY]),
            &_color[COLORABLE_MARK_VELOCITY]);
    hildon_color_button_set_color(
            HILDON_COLOR_BUTTON(cdi.col[COLORABLE_MARK_OLD]),
            &_color[COLORABLE_MARK_OLD]);

    /* Initialize Track. */
    hildon_color_button_set_color(
            HILDON_COLOR_BUTTON(cdi.col[COLORABLE_TRACK]),
            &_color[COLORABLE_TRACK]);
    hildon_color_button_set_color(
            HILDON_COLOR_BUTTON(cdi.col[COLORABLE_TRACK_MARK]),
            &_color[COLORABLE_TRACK_MARK]);
    hildon_color_button_set_color(
            HILDON_COLOR_BUTTON(cdi.col[COLORABLE_TRACK_BREAK]),
            &_color[COLORABLE_TRACK_BREAK]);

    /* Initialize Route. */
    hildon_color_button_set_color(
            HILDON_COLOR_BUTTON(cdi.col[COLORABLE_ROUTE]),
            &_color[COLORABLE_ROUTE]);
    hildon_color_button_set_color(
            HILDON_COLOR_BUTTON(cdi.col[COLORABLE_ROUTE_WAY]),
            &_color[COLORABLE_ROUTE_WAY]);
    hildon_color_button_set_color(
            HILDON_COLOR_BUTTON(cdi.col[COLORABLE_ROUTE_BREAK]),
            &_color[COLORABLE_ROUTE_BREAK]);

    /* Initialize POI. */
    hildon_color_button_set_color(
            HILDON_COLOR_BUTTON(cdi.col[COLORABLE_POI]),
            &_color[COLORABLE_POI]);

    gtk_widget_show_all(dialog);

    while(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(dialog)))
    {
#ifndef LEGACY
        hildon_color_button_get_color(
                HILDON_COLOR_BUTTON(cdi.col[COLORABLE_MARK]), 
                &_color[COLORABLE_MARK]);

        hildon_color_button_get_color(
                HILDON_COLOR_BUTTON(cdi.col[COLORABLE_MARK_VELOCITY]), 
                &_color[COLORABLE_MARK_VELOCITY]);

        hildon_color_button_get_color(
                HILDON_COLOR_BUTTON(cdi.col[COLORABLE_MARK_OLD]),
                &_color[COLORABLE_MARK_OLD]);

        hildon_color_button_get_color(
                HILDON_COLOR_BUTTON(cdi.col[COLORABLE_TRACK]),
                &_color[COLORABLE_TRACK]);

        hildon_color_button_get_color(
                HILDON_COLOR_BUTTON(cdi.col[COLORABLE_TRACK_MARK]),
                &_color[COLORABLE_TRACK_MARK]);

        hildon_color_button_get_color(
                HILDON_COLOR_BUTTON(cdi.col[COLORABLE_TRACK_BREAK]),
                &_color[COLORABLE_TRACK_BREAK]);

        hildon_color_button_get_color(
                HILDON_COLOR_BUTTON(cdi.col[COLORABLE_ROUTE]),
                &_color[COLORABLE_ROUTE]);

        hildon_color_button_get_color(
                HILDON_COLOR_BUTTON(cdi.col[COLORABLE_ROUTE_WAY]),
                &_color[COLORABLE_ROUTE_WAY]);

        hildon_color_button_get_color(
                HILDON_COLOR_BUTTON(cdi.col[COLORABLE_ROUTE_BREAK]),
                &_color[COLORABLE_ROUTE_BREAK]);

        hildon_color_button_get_color(
                HILDON_COLOR_BUTTON(cdi.col[COLORABLE_POI]),
                &_color[COLORABLE_POI]);
#else
        GdkColor *color;

        color = hildon_color_button_get_color(
                HILDON_COLOR_BUTTON(cdi.col[COLORABLE_MARK]));
        _color[COLORABLE_MARK] = *color;

        color = hildon_color_button_get_color(
                HILDON_COLOR_BUTTON(cdi.col[COLORABLE_MARK_VELOCITY]));
        _color[COLORABLE_MARK_VELOCITY] = *color;

        color = hildon_color_button_get_color(
                HILDON_COLOR_BUTTON(cdi.col[COLORABLE_MARK_OLD]));
        _color[COLORABLE_MARK_OLD] = *color;

        color = hildon_color_button_get_color(
                HILDON_COLOR_BUTTON(cdi.col[COLORABLE_TRACK]));
        _color[COLORABLE_TRACK] = *color;

        color = hildon_color_button_get_color(
                HILDON_COLOR_BUTTON(cdi.col[COLORABLE_TRACK_MARK]));
        _color[COLORABLE_TRACK_MARK] = *color;

        color = hildon_color_button_get_color(
                HILDON_COLOR_BUTTON(cdi.col[COLORABLE_TRACK_BREAK]));
        _color[COLORABLE_TRACK_BREAK] = *color;

        color = hildon_color_button_get_color(
                HILDON_COLOR_BUTTON(cdi.col[COLORABLE_ROUTE]));
        _color[COLORABLE_ROUTE] = *color;

        color = hildon_color_button_get_color(
                HILDON_COLOR_BUTTON(cdi.col[COLORABLE_ROUTE_WAY]));
        _color[COLORABLE_ROUTE_WAY] = *color;

        color = hildon_color_button_get_color(
                HILDON_COLOR_BUTTON(cdi.col[COLORABLE_ROUTE_BREAK]));
        _color[COLORABLE_ROUTE_BREAK] = *color;

        color = hildon_color_button_get_color(
                HILDON_COLOR_BUTTON(cdi.col[COLORABLE_POI]));
        _color[COLORABLE_POI] = *color;

#endif
        
        update_gcs();
        break;
    }

    map_force_redraw();

    gtk_widget_hide(dialog);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

/**
 * Bring up the Settings dialog.  Return TRUE if and only if the recever
 * information has changed (MAC or channel).
 */
gboolean
settings_dialog()
{
    static GtkWidget *dialog = NULL;
    static GtkWidget *notebook = NULL;
    static GtkWidget *table = NULL;
    static GtkWidget *hbox = NULL;
    static GtkWidget *label = NULL;
    static GtkWidget *rad_gps_bt = NULL;
    static GtkWidget *rad_gps_gpsd = NULL;
    static GtkWidget *rad_gps_file = NULL;
    static GtkWidget *txt_gps_bt_mac = NULL;
    static GtkWidget *txt_gps_gpsd_host = NULL;
    static GtkWidget *num_gps_gpsd_port = NULL;
    static GtkWidget *txt_gps_file_path = NULL;
    static GtkWidget *num_center_ratio = NULL;
    static GtkWidget *num_lead_ratio = NULL;
    static GtkWidget *chk_lead_is_fixed = NULL;
    static GtkWidget *num_rotate_sens = NULL;
    static GtkWidget *cmb_rotate_dir = NULL;
    static GtkWidget *num_ac_min_speed = NULL;
    static GtkWidget *num_announce_notice = NULL;
    static GtkWidget *chk_enable_voice = NULL;
    static GtkWidget *chk_enable_announce = NULL;
    static GtkWidget *num_draw_width = NULL;
    static GtkWidget *cmb_units = NULL;
    static GtkWidget *cmb_degformat = NULL;
    static GtkWidget *btn_scan = NULL;
    static GtkWidget *btn_browse_gps = NULL;
    static GtkWidget *btn_buttons = NULL;
    static GtkWidget *btn_colors = NULL;

    static GtkWidget *txt_poi_db = NULL;
    static GtkWidget *btn_browse_poi = NULL;
    static GtkWidget *num_poi_zoom = NULL;
    static GtkWidget *num_auto_download_precache = NULL;
    static GtkWidget *chk_speed_limit_on = NULL;
    static GtkWidget *num_speed = NULL;
    static GtkWidget *cmb_speed_location = NULL;
    static GtkWidget *cmb_unblank_option = NULL;
    static GtkWidget *cmb_info_font_size = NULL;

    static BrowseInfo poi_browse_info = {0, 0};
    static BrowseInfo gps_file_browse_info = {0, 0};
    static ScanInfo scan_info = {0};
    gboolean rcvr_changed = FALSE;
    gint i;
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(dialog == NULL)
    {
        dialog = gtk_dialog_new_with_buttons(_("Settings"),
                GTK_WINDOW(_window), GTK_DIALOG_MODAL,
                GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                NULL);

        /* Enable the help button. */
#ifndef LEGACY
        hildon_help_dialog_help_enable(
#else
        ossohelp_dialog_help_enable(
#endif 
                GTK_DIALOG(dialog), HELP_ID_SETTINGS, _osso);

        gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area),
               btn_buttons = gtk_button_new_with_label(_("Hardware Keys...")));

        gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area),
                btn_colors = gtk_button_new_with_label(_("Colors...")));

        gtk_dialog_add_button(GTK_DIALOG(dialog),
                GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT);

        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
                notebook = gtk_notebook_new(), TRUE, TRUE, 0);

        /* Receiver page. */
        gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
                table = gtk_table_new(3, 4, FALSE),
                label = gtk_label_new(_("GPS")));

        /* Receiver MAC Address. */
        gtk_table_attach(GTK_TABLE(table),
                rad_gps_bt = gtk_radio_button_new_with_label(
                    NULL, _("Bluetooth")),
                0, 1, 0, 1, GTK_FILL, 0, 2, 4);
        gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
        gtk_table_attach(GTK_TABLE(table),
                hbox = gtk_hbox_new(FALSE, 4),
                1, 2, 0, 1, GTK_EXPAND | GTK_FILL, 0, 2, 4);
        gtk_box_pack_start(GTK_BOX(hbox),
                txt_gps_bt_mac = gtk_entry_new(),
                TRUE, TRUE, 0);
#ifndef LEGACY
        g_object_set(G_OBJECT(txt_gps_bt_mac), "hildon-input-mode",
                HILDON_GTK_INPUT_MODE_FULL, NULL);
#else
        g_object_set(G_OBJECT(txt_gps_bt_mac), HILDON_AUTOCAP, FALSE, NULL);
#endif
        gtk_box_pack_start(GTK_BOX(hbox),
                btn_scan = gtk_button_new_with_label(_("Scan...")),
                FALSE, FALSE, 0);

        /* File Path (RFComm). */
        gtk_table_attach(GTK_TABLE(table),
                rad_gps_file = gtk_radio_button_new_with_label_from_widget(
                    GTK_RADIO_BUTTON(rad_gps_bt), _("File Path")),
                0, 1, 1, 2, GTK_FILL, 0, 2, 4);
        gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
        gtk_table_attach(GTK_TABLE(table),
                hbox = gtk_hbox_new(FALSE, 4),
                1, 2, 1, 2, GTK_EXPAND | GTK_FILL, 0, 2, 4);
        gtk_box_pack_start(GTK_BOX(hbox),
                txt_gps_file_path = gtk_entry_new(),
                TRUE, TRUE, 0);
#ifndef LEGACY
        g_object_set(G_OBJECT(txt_gps_file_path), "hildon-input-mode",
                HILDON_GTK_INPUT_MODE_FULL, NULL);
#else
        g_object_set(G_OBJECT(txt_gps_file_path), HILDON_AUTOCAP, FALSE, NULL);
#endif
        gtk_box_pack_start(GTK_BOX(hbox),
                btn_browse_gps = gtk_button_new_with_label(_("Browse...")),
                FALSE, FALSE, 0);

        /* GPSD Hostname and Port. */
        gtk_table_attach(GTK_TABLE(table),
                rad_gps_gpsd = gtk_radio_button_new_with_label_from_widget(
                    GTK_RADIO_BUTTON(rad_gps_bt), _("GPSD Host")),
                0, 1, 2, 3, GTK_FILL, 0, 2, 4);
        gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
        gtk_table_attach(GTK_TABLE(table),
                hbox = gtk_hbox_new(FALSE, 4),
                1, 2, 2, 3, GTK_EXPAND | GTK_FILL, 0, 2, 4);
        gtk_box_pack_start(GTK_BOX(hbox),
                txt_gps_gpsd_host = gtk_entry_new(),
                TRUE, TRUE, 0);
#ifndef LEGACY
        g_object_set(G_OBJECT(txt_gps_gpsd_host), "hildon-input-mode",
                HILDON_GTK_INPUT_MODE_FULL, NULL);
#else
        g_object_set(G_OBJECT(txt_gps_gpsd_host), HILDON_AUTOCAP, FALSE, NULL);
#endif
        gtk_box_pack_start(GTK_BOX(hbox),
                label = gtk_label_new(_("Port")),
                FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox),
                num_gps_gpsd_port = hildon_number_editor_new(1, 65535),
                FALSE, FALSE, 0);


        /* Auto-Center page. */
        gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
                table = gtk_table_new(3, 3, FALSE),
                label = gtk_label_new(_("Auto-Center")));

        /* Lead Amount. */
        gtk_table_attach(GTK_TABLE(table),
                label = gtk_label_new(_("Lead Amount")),
                0, 1, 0, 1, GTK_FILL, 0, 2, 4);
        gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
        gtk_table_attach(GTK_TABLE(table),
                label = gtk_alignment_new(0.f, 0.5f, 0.f, 0.f),
                1, 2, 0, 1, GTK_FILL, 0, 2, 4);
        gtk_container_add(GTK_CONTAINER(label),
                num_lead_ratio = hildon_controlbar_new());
        hildon_controlbar_set_range(HILDON_CONTROLBAR(num_lead_ratio), 1, 10);
        force_min_visible_bars(HILDON_CONTROLBAR(num_lead_ratio), 1);

        gtk_table_attach(GTK_TABLE(table),
                label = gtk_alignment_new(0.f, 0.5f, 0.f, 0.f),
                2, 3, 0, 1, GTK_FILL, 0, 2, 4);
        gtk_container_add(GTK_CONTAINER(label),
            chk_lead_is_fixed = gtk_check_button_new_with_label(_("Fixed")));

        /* Auto-Center Pan Sensitivity. */
        gtk_table_attach(GTK_TABLE(table),
                label = gtk_label_new(_("Pan Sensitivity")),
                0, 1, 1, 2, GTK_FILL, 0, 2, 4);
        gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
        gtk_table_attach(GTK_TABLE(table),
                label = gtk_alignment_new(0.f, 0.5f, 0.f, 0.f),
                1, 2, 1, 2, GTK_FILL, 0, 2, 4);
        gtk_container_add(GTK_CONTAINER(label),
                num_center_ratio = hildon_controlbar_new());
        hildon_controlbar_set_range(HILDON_CONTROLBAR(num_center_ratio), 1,10);
        force_min_visible_bars(HILDON_CONTROLBAR(num_center_ratio), 1);

        gtk_table_attach(GTK_TABLE(table),
                hbox = gtk_hbox_new(FALSE, 4),
                2, 3, 1, 2, GTK_FILL, 0, 2, 4);
        gtk_box_pack_start(GTK_BOX(hbox),
                label = gtk_label_new(_("Min. Speed")),
                TRUE, TRUE, 4);
        gtk_box_pack_start(GTK_BOX(hbox),
                num_ac_min_speed = hildon_number_editor_new(0, 99),
                TRUE, TRUE, 4);

        /* Auto-Center Rotate Sensitivity. */
        gtk_table_attach(GTK_TABLE(table),
                label = gtk_label_new(_("Rotate Sensit.")),
                0, 1, 2, 3, GTK_FILL, 0, 2, 4);
        gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
        gtk_table_attach(GTK_TABLE(table),
                num_rotate_sens = hildon_controlbar_new(),
                1, 2, 2, 3, GTK_FILL, 0, 2, 4);
        hildon_controlbar_set_range(HILDON_CONTROLBAR(num_rotate_sens), 1,10);
        force_min_visible_bars(HILDON_CONTROLBAR(num_rotate_sens), 1);

        gtk_table_attach(GTK_TABLE(table),
                hbox = gtk_hbox_new(FALSE, 4),
                2, 3, 2, 3, GTK_FILL, 0, 2, 4);
        gtk_box_pack_start(GTK_BOX(hbox),
                label = gtk_label_new(_("Points")),
                TRUE, TRUE, 4);
        gtk_box_pack_start(GTK_BOX(hbox),
                cmb_rotate_dir = gtk_combo_box_new_text(),
                TRUE, TRUE, 4);
        for(i = 0; i < ROTATE_DIR_ENUM_COUNT; i++)
            gtk_combo_box_append_text(GTK_COMBO_BOX(cmb_rotate_dir),
                    ROTATE_DIR_ENUM_TEXT[i]);

        /* Announcement. */
        gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
                table = gtk_table_new(2, 3, FALSE),
                label = gtk_label_new(_("Announce")));

        /* Enable Waypoint Announcements. */
        gtk_table_attach(GTK_TABLE(table),
                chk_enable_announce = gtk_check_button_new_with_label(
                    _("Enable Waypoint Announcements")),
                0, 2, 0, 1, GTK_FILL, 0, 2, 4);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chk_enable_announce),
                _enable_announce);

        /* Announcement Advance Notice. */
        gtk_table_attach(GTK_TABLE(table),
                label = gtk_label_new(_("Advance Notice")),
                0, 1, 1, 2, GTK_FILL, 0, 2, 4);
        gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
        gtk_table_attach(GTK_TABLE(table),
                num_announce_notice = hildon_controlbar_new(),
                1, 2, 1, 2, GTK_EXPAND | GTK_FILL, 0, 2, 4);
        hildon_controlbar_set_range(
                HILDON_CONTROLBAR(num_announce_notice), 1, 20);
        force_min_visible_bars(HILDON_CONTROLBAR(num_announce_notice), 1);

        /* Enable Voice. */
        gtk_table_attach(GTK_TABLE(table),
                chk_enable_voice = gtk_check_button_new_with_label(
                    _("Enable Voice Synthesis (requires flite)")),
                0, 2, 2, 3, GTK_FILL, 0, 2, 4);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chk_enable_voice),
                _enable_voice);

        /* Misc. page. */
        gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
                table = gtk_table_new(3, 5, FALSE),
                label = gtk_label_new(_("Misc.")));

        /* Line Width. */
        gtk_table_attach(GTK_TABLE(table),
                label = gtk_label_new(_("Line Width")),
                0, 1, 0, 1, GTK_FILL, 0, 2, 4);
        gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
        gtk_table_attach(GTK_TABLE(table),
                num_draw_width = hildon_controlbar_new(),
                1, 5, 0, 1, GTK_FILL, 0, 2, 4);
        hildon_controlbar_set_range(HILDON_CONTROLBAR(num_draw_width), 1, 20);
        force_min_visible_bars(HILDON_CONTROLBAR(num_draw_width), 1);

        /* Unblank Screen */
        gtk_table_attach(GTK_TABLE(table),
                label = gtk_label_new(_("Unblank Screen")),
                0, 1, 1, 2, GTK_FILL, 0, 2, 4);
        gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
        gtk_table_attach(GTK_TABLE(table),
                cmb_unblank_option = gtk_combo_box_new_text(),
                1, 5, 1, 2, GTK_FILL, 0, 2, 4);
        for(i = 0; i < UNBLANK_ENUM_COUNT; i++)
            gtk_combo_box_append_text(GTK_COMBO_BOX(cmb_unblank_option),
                    UNBLANK_ENUM_TEXT[i]);

        /* Information Font Size. */
        gtk_table_attach(GTK_TABLE(table),
                label = gtk_label_new(_("Info Font Size")),
                0, 1, 2, 3, GTK_FILL, 0, 2, 4);
        gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
        gtk_table_attach(GTK_TABLE(table),
                cmb_info_font_size = gtk_combo_box_new_text(),
                1, 2, 2, 3, GTK_FILL, 0, 2, 4);
        for(i = 0; i < INFO_FONT_ENUM_COUNT; i++)
            gtk_combo_box_append_text(GTK_COMBO_BOX(cmb_info_font_size),
                    INFO_FONT_ENUM_TEXT[i]);

        gtk_table_attach(GTK_TABLE(table),
                gtk_vseparator_new(),
                2, 3, 2, 3, GTK_EXPAND | GTK_FILL, GTK_FILL, 2,4);


        /* Units. */
        gtk_table_attach(GTK_TABLE(table),
                label = gtk_label_new(_("Units")),
                3, 4, 2, 3, GTK_FILL, 0, 2, 4);
        gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
        gtk_table_attach(GTK_TABLE(table),
                cmb_units = gtk_combo_box_new_text(),
                4, 5, 2, 3, GTK_FILL, 0, 2, 4);
        for(i = 0; i < UNITS_ENUM_COUNT; i++)
            gtk_combo_box_append_text(
                    GTK_COMBO_BOX(cmb_units), UNITS_ENUM_TEXT[i]);

        /* Misc. 2 page. */
        gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
                table = gtk_table_new(3, 4, FALSE),
                label = gtk_label_new(_("Misc. 2")));

        /* Degrees format */
        gtk_table_attach(GTK_TABLE(table),
                label = gtk_label_new(_("Degrees Format")),
                0, 1, 0, 1, GTK_FILL, 0, 2, 4);
        gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
        gtk_table_attach(GTK_TABLE(table),
                label = gtk_alignment_new(0.f, 0.5f, 0.f, 0.f),
                1, 2, 0, 1, GTK_FILL, 0, 2, 4);
        gtk_container_add(GTK_CONTAINER(label),
                cmb_degformat = gtk_combo_box_new_text());
        for(i = 0; i < DEG_FORMAT_ENUM_COUNT; i++)
            gtk_combo_box_append_text(GTK_COMBO_BOX(cmb_degformat),
                DEG_FORMAT_ENUM_TEXT[i]);

        gtk_table_attach(GTK_TABLE(table),
                label = gtk_label_new(_("Auto-Download Pre-cache")),
                0, 1, 1, 2, GTK_FILL, 0, 2, 4);
        gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
        gtk_table_attach(GTK_TABLE(table),
                num_auto_download_precache = hildon_controlbar_new(),
                1, 2, 1, 2, GTK_FILL, 0, 2, 4);
        hildon_controlbar_set_range(
                HILDON_CONTROLBAR(num_auto_download_precache), 1, 5);
        force_min_visible_bars(
                HILDON_CONTROLBAR(num_auto_download_precache), 1);

        /* Speed warner. */
        gtk_table_attach(GTK_TABLE(table),
                hbox = gtk_hbox_new(FALSE, 4),
                0, 4, 2, 3, GTK_FILL, 0, 2, 4);

        gtk_box_pack_start(GTK_BOX(hbox),
                chk_speed_limit_on = gtk_check_button_new_with_label(
                    _("Speed Limit")),
                FALSE, FALSE, 0);

        gtk_box_pack_start(GTK_BOX(hbox),
                label = gtk_alignment_new(0.f, 0.5f, 0.f, 0.f),
                FALSE, FALSE, 0);
        gtk_container_add(GTK_CONTAINER(label),
                num_speed = hildon_number_editor_new(0, 999));

        gtk_box_pack_start(GTK_BOX(hbox),
                label = gtk_label_new(_("Location")),
                FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox),
                label = gtk_alignment_new(0.f, 0.5f, 0.f, 0.f),
                FALSE, FALSE, 0);
        gtk_container_add(GTK_CONTAINER(label),
                cmb_speed_location = gtk_combo_box_new_text());
        for(i = 0; i < SPEED_LOCATION_ENUM_COUNT; i++)
            gtk_combo_box_append_text(GTK_COMBO_BOX(cmb_speed_location),
                    SPEED_LOCATION_ENUM_TEXT[i]);


        /* POI page */
        gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
                table = gtk_table_new(2, 3, FALSE),
                label = gtk_label_new(_("POI")));

        /* POI database. */
        gtk_table_attach(GTK_TABLE(table),
                label = gtk_label_new(_("POI database")),
                0, 1, 1, 2, GTK_FILL, 0, 2, 4);
        gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
        gtk_table_attach(GTK_TABLE(table),
                hbox = gtk_hbox_new(FALSE, 4),
                1, 2, 1, 2, GTK_EXPAND | GTK_FILL, 0, 2, 4);
        gtk_box_pack_start(GTK_BOX(hbox),
                txt_poi_db = gtk_entry_new(),
                TRUE, TRUE, 0);
#ifndef LEGACY
        g_object_set(G_OBJECT(txt_poi_db), "hildon-input-mode",
                HILDON_GTK_INPUT_MODE_FULL, NULL);
#else
        g_object_set(G_OBJECT(txt_poi_db), HILDON_AUTOCAP, FALSE, NULL);
#endif
        gtk_box_pack_start(GTK_BOX(hbox),
                btn_browse_poi = gtk_button_new_with_label(_("Browse...")),
                FALSE, FALSE, 0);

        /* Show POI below zoom. */
        gtk_table_attach(GTK_TABLE(table),
                label = gtk_label_new(_("Show POI below zoom")),
                0, 1, 2, 3, GTK_FILL, 0, 2, 4);
        gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
        gtk_table_attach(GTK_TABLE(table),
                label = gtk_alignment_new(0.f, 0.5f, 0.f, 0.f),
                1, 2, 2, 3, GTK_FILL, 0, 2, 4);
        gtk_container_add(GTK_CONTAINER(label),
                num_poi_zoom = hildon_number_editor_new(0, MAX_ZOOM));

        /* Connect signals. */
        memset(&scan_info, 0, sizeof(scan_info));
        scan_info.settings_dialog = dialog;
        scan_info.txt_gps_bt_mac = txt_gps_bt_mac;
        g_signal_connect(G_OBJECT(btn_scan), "clicked",
                         G_CALLBACK(scan_bluetooth), &scan_info);
        g_signal_connect(G_OBJECT(btn_buttons), "clicked",
                         G_CALLBACK(settings_dialog_hardkeys), dialog);
        g_signal_connect(G_OBJECT(btn_colors), "clicked",
                         G_CALLBACK(settings_dialog_colors), dialog);

        poi_browse_info.dialog = dialog;
        poi_browse_info.txt = txt_poi_db;
        g_signal_connect(G_OBJECT(btn_browse_poi), "clicked",
                G_CALLBACK(settings_dialog_browse_forfile), &poi_browse_info);

        gps_file_browse_info.dialog = dialog;
        gps_file_browse_info.txt = txt_gps_file_path;
        g_signal_connect(G_OBJECT(btn_browse_gps), "clicked",
                G_CALLBACK(settings_dialog_browse_forfile),
                &gps_file_browse_info);
    }


    /* Initialize fields. */
    if(_gri.bt_mac)
        gtk_entry_set_text(GTK_ENTRY(txt_gps_bt_mac), _gri.bt_mac);
    if(_gri.gpsd_host)
        gtk_entry_set_text(GTK_ENTRY(txt_gps_gpsd_host), _gri.gpsd_host);
    hildon_number_editor_set_value(
            HILDON_NUMBER_EDITOR(num_gps_gpsd_port), _gri.gpsd_port);
    if(_gri.file_path)
        gtk_entry_set_text(GTK_ENTRY(txt_gps_file_path), _gri.file_path);
    switch(_gri.type)
    {
        case GPS_RCVR_GPSD:
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(rad_gps_gpsd),TRUE);
            break;
        case GPS_RCVR_FILE:
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(rad_gps_file),TRUE);
            break;
        default: /* Including GPS_RCVR_BT */
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(rad_gps_bt), TRUE);
            break;
    }

    if(_poi_db_filename)
        gtk_entry_set_text(GTK_ENTRY(txt_poi_db), _poi_db_filename);
    hildon_number_editor_set_value(HILDON_NUMBER_EDITOR(num_poi_zoom),
            _poi_zoom);
    hildon_controlbar_set_value(HILDON_CONTROLBAR(num_center_ratio),
            _center_ratio);
    hildon_controlbar_set_value(HILDON_CONTROLBAR(num_lead_ratio),
            _lead_ratio);
    gtk_toggle_button_set_active(
            GTK_TOGGLE_BUTTON(chk_lead_is_fixed), _lead_is_fixed);
    hildon_controlbar_set_value(HILDON_CONTROLBAR(num_rotate_sens),
            _rotate_sens);
    gtk_combo_box_set_active(GTK_COMBO_BOX(cmb_rotate_dir), _rotate_dir);
    hildon_number_editor_set_value(HILDON_NUMBER_EDITOR(num_ac_min_speed),
            _ac_min_speed);
    hildon_controlbar_set_value(HILDON_CONTROLBAR(num_announce_notice),
            _announce_notice_ratio);
    hildon_controlbar_set_value(HILDON_CONTROLBAR(num_draw_width),
            _draw_width);
    gtk_combo_box_set_active(GTK_COMBO_BOX(cmb_units), _units);
    gtk_combo_box_set_active(GTK_COMBO_BOX(cmb_degformat), _degformat);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chk_speed_limit_on),
            _speed_limit_on);
    hildon_controlbar_set_value(HILDON_CONTROLBAR(num_auto_download_precache),
            _auto_download_precache);
    hildon_number_editor_set_value(HILDON_NUMBER_EDITOR(num_speed),
            _speed_limit);
    gtk_combo_box_set_active(GTK_COMBO_BOX(cmb_speed_location),
            _speed_location);
    gtk_combo_box_set_active(GTK_COMBO_BOX(cmb_unblank_option),
            _unblank_option);
    gtk_combo_box_set_active(GTK_COMBO_BOX(cmb_info_font_size),
            _info_font_size);

    gtk_widget_show_all(dialog);

    while(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(dialog)))
    {
        GpsRcvrType new_grtype;

        if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(rad_gps_gpsd))
                && !*gtk_entry_get_text(GTK_ENTRY(txt_gps_gpsd_host)))
        {
            popup_error(dialog, _("Please specify a GPSD hostname."));
            continue;
        }

        if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(rad_gps_file))
                && !*gtk_entry_get_text(GTK_ENTRY(txt_gps_file_path)))
        {
            popup_error(dialog, _("Please specify a GPS file pathname."));
            continue;
        }

        /* Set _gri.bt_mac if necessary. */
        if(!*gtk_entry_get_text(GTK_ENTRY(txt_gps_bt_mac)))
        {
            /* User specified no rcvr mac - set _gri.bt_mac to NULL. */
            if(_gri.bt_mac)
            {
                g_free(_gri.bt_mac);
                _gri.bt_mac = NULL;
                rcvr_changed = (_gri.type == GPS_RCVR_BT);
            }
        }
        else if(!_gri.bt_mac || strcmp(_gri.bt_mac,
                      gtk_entry_get_text(GTK_ENTRY(txt_gps_bt_mac))))
        {
            /* User specified a new rcvr mac. */
            g_free(_gri.bt_mac);
            _gri.bt_mac = g_strdup(gtk_entry_get_text(
                        GTK_ENTRY(txt_gps_bt_mac)));
            rcvr_changed = (_gri.type == GPS_RCVR_BT);
        }

        /* Set _gri.gpsd_host if necessary. */
        if(!*gtk_entry_get_text(GTK_ENTRY(txt_gps_gpsd_host)))
        {
            /* User specified no rcvr mac - set _gri.gpsd_host to NULL. */
            if(_gri.gpsd_host)
            {
                g_free(_gri.gpsd_host);
                _gri.gpsd_host = NULL;
                rcvr_changed = (_gri.type == GPS_RCVR_GPSD);
            }
        }
        else if(!_gri.gpsd_host || strcmp(_gri.gpsd_host,
                      gtk_entry_get_text(GTK_ENTRY(txt_gps_gpsd_host))))
        {
            /* User specified a new rcvr mac. */
            g_free(_gri.gpsd_host);
            _gri.gpsd_host = g_strdup(gtk_entry_get_text(
                        GTK_ENTRY(txt_gps_gpsd_host)));
            rcvr_changed = (_gri.type == GPS_RCVR_GPSD);
        }

        if(_gri.gpsd_port != hildon_number_editor_get_value(
                    HILDON_NUMBER_EDITOR(num_gps_gpsd_port)))
        {
            _gri.gpsd_port = hildon_number_editor_get_value(
                    HILDON_NUMBER_EDITOR(num_gps_gpsd_port));
            rcvr_changed = (_gri.type == GPS_RCVR_GPSD);
        }

        /* Set _gri.file_path if necessary. */
        if(!*gtk_entry_get_text(GTK_ENTRY(txt_gps_file_path)))
        {
            /* User specified no rcvr mac - set _gri.file_path to NULL. */
            if(_gri.file_path)
            {
                g_free(_gri.file_path);
                _gri.file_path = NULL;
                rcvr_changed = (_gri.type == GPS_RCVR_FILE);
            }
        }
        else if(!_gri.file_path || strcmp(_gri.file_path,
                      gtk_entry_get_text(GTK_ENTRY(txt_gps_file_path))))
        {
            /* User specified a new rcvr mac. */
            g_free(_gri.file_path);
            _gri.file_path = g_strdup(gtk_entry_get_text(
                        GTK_ENTRY(txt_gps_file_path)));
            rcvr_changed = (_gri.type == GPS_RCVR_FILE);
        }

        if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(rad_gps_bt)))
            new_grtype = GPS_RCVR_BT;
        else if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(rad_gps_gpsd)))
            new_grtype = GPS_RCVR_GPSD;
        else if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(rad_gps_file)))
            new_grtype = GPS_RCVR_FILE;
        else
            new_grtype = GPS_RCVR_BT;

        if(new_grtype != _gri.type)
        {
            _gri.type = new_grtype;
            rcvr_changed = TRUE;
        }

        _center_ratio = hildon_controlbar_get_value(
                HILDON_CONTROLBAR(num_center_ratio));

        _lead_ratio = hildon_controlbar_get_value(
                HILDON_CONTROLBAR(num_lead_ratio));

        _lead_is_fixed = gtk_toggle_button_get_active(
                GTK_TOGGLE_BUTTON(chk_lead_is_fixed));

        _rotate_sens = hildon_controlbar_get_value(
                HILDON_CONTROLBAR(num_rotate_sens));

        _ac_min_speed = hildon_number_editor_get_value(
                HILDON_NUMBER_EDITOR(num_ac_min_speed));

        _rotate_dir = gtk_combo_box_get_active(GTK_COMBO_BOX(cmb_rotate_dir));

        _auto_download_precache = hildon_controlbar_get_value(
                HILDON_CONTROLBAR(num_auto_download_precache));

        _draw_width = hildon_controlbar_get_value(
                HILDON_CONTROLBAR(num_draw_width));

        _units = gtk_combo_box_get_active(GTK_COMBO_BOX(cmb_units));
        _degformat = gtk_combo_box_get_active(GTK_COMBO_BOX(cmb_degformat));

        _speed_limit_on = gtk_toggle_button_get_active(
                GTK_TOGGLE_BUTTON(chk_speed_limit_on));
        _speed_limit = hildon_number_editor_get_value(
                HILDON_NUMBER_EDITOR(num_speed));
        _speed_location = gtk_combo_box_get_active(
                GTK_COMBO_BOX(cmb_speed_location));

        _unblank_option = gtk_combo_box_get_active(
                GTK_COMBO_BOX(cmb_unblank_option));

        _info_font_size = gtk_combo_box_get_active(
                GTK_COMBO_BOX(cmb_info_font_size));

        _announce_notice_ratio = hildon_controlbar_get_value(
                HILDON_CONTROLBAR(num_announce_notice));

        _enable_announce = gtk_toggle_button_get_active(
                GTK_TOGGLE_BUTTON(chk_enable_announce));

        _enable_voice = gtk_toggle_button_get_active(
                GTK_TOGGLE_BUTTON(chk_enable_voice));

        /* Check if user specified a different POI database from before. */
        if((!_poi_db_filename && *gtk_entry_get_text(GTK_ENTRY(txt_poi_db)))
                || strcmp(_poi_db_filename,
                    gtk_entry_get_text(GTK_ENTRY(txt_poi_db))))
        {
            /* Clear old filename/dirname, if necessary. */
            if(_poi_db_filename)
            {
                g_free(_poi_db_filename);
                _poi_db_filename = NULL;
                g_free(_poi_db_dirname);
                _poi_db_dirname = NULL;
            }

            if(*gtk_entry_get_text(GTK_ENTRY(txt_poi_db)))
            {
                _poi_db_filename = g_strdup(gtk_entry_get_text(
                            GTK_ENTRY(txt_poi_db)));
                _poi_db_dirname = g_path_get_dirname(_poi_db_filename);
            }

            poi_db_connect();
        }

        _poi_zoom = hildon_number_editor_get_value(
                HILDON_NUMBER_EDITOR(num_poi_zoom));

        update_gcs();

        settings_save();

        map_force_redraw();
        map_refresh_mark(TRUE);

        break;
    }

    gtk_widget_hide(dialog);

    vprintf("%s(): return %d\n", __PRETTY_FUNCTION__, rcvr_changed);
    return rcvr_changed;
}

RepoData*
settings_parse_repo(gchar *str)
{
    /* Parse each part of a repo, delimited by newline characters:
     * 1. name
     * 2. url
     * 3. db_filename
     * 4. dl_zoom_steps
     * 5. view_zoom_steps
     */
    gchar *token, *error_check;
    printf("%s(%s)\n", __PRETTY_FUNCTION__, str);

    RepoData *rd = g_new0(RepoData, 1);

    /* Parse name. */
    token = strsep(&str, "\n\t");
    if(token)
        rd->name = g_strdup(token);

    /* Parse URL format. */
    token = strsep(&str, "\n\t");
    if(token)
        rd->url = g_strdup(token);

    /* Parse cache dir. */
    token = strsep(&str, "\n\t");
    if(token)
        rd->db_filename = gnome_vfs_expand_initial_tilde(token);

    /* Parse download zoom steps. */
    token = strsep(&str, "\n\t");
    if(!token || !*token || !(rd->dl_zoom_steps = atoi(token)))
        rd->dl_zoom_steps = 2;

    /* Parse view zoom steps. */
    token = strsep(&str, "\n\t");
    if(!token || !*token || !(rd->view_zoom_steps = atoi(token)))
        rd->view_zoom_steps = 1;

    /* Parse double-size. */
    token = strsep(&str, "\n\t");
    if(token)
        rd->double_size = atoi(token); /* Default is zero (FALSE) */

    /* Parse next-able. */
    token = strsep(&str, "\n\t");
    if(!token || !*token
            || (rd->nextable = strtol(token, &error_check, 10), token == str))
        rd->nextable = TRUE;

    /* Parse min zoom. */
    token = strsep(&str, "\n\t");
    if(!token || !*token
            || (rd->min_zoom = strtol(token, &error_check, 10), token == str))
        rd->min_zoom = 4;

    /* Parse max zoom. */
    token = strsep(&str, "\n\t");
    if(!token || !*token
            || (rd->max_zoom = strtol(token, &error_check, 10), token == str))
        rd->max_zoom = 20;

    set_repo_type(rd);

    vprintf("%s(): return %p\n", __PRETTY_FUNCTION__, rd);
    return rd;
}

/**
 * Initialize all configuration from GCONF.  This should not be called more
 * than once during execution.
 */
void
settings_init()
{
    GConfValue *value;
    GConfClient *gconf_client = gconf_client_get_default();
    gchar *str;
    printf("%s()\n", __PRETTY_FUNCTION__);

    /* Initialize some constants. */
    CUSTOM_KEY_GCONF[CUSTOM_KEY_UP] = GCONF_KEY_PREFIX"/key_up";
    CUSTOM_KEY_GCONF[CUSTOM_KEY_DOWN] = GCONF_KEY_PREFIX"/key_down";
    CUSTOM_KEY_GCONF[CUSTOM_KEY_LEFT] = GCONF_KEY_PREFIX"/key_left";
    CUSTOM_KEY_GCONF[CUSTOM_KEY_RIGHT] = GCONF_KEY_PREFIX"/key_right";
    CUSTOM_KEY_GCONF[CUSTOM_KEY_SELECT] = GCONF_KEY_PREFIX"/key_select";
    CUSTOM_KEY_GCONF[CUSTOM_KEY_INCREASE] = GCONF_KEY_PREFIX"/key_increase";
    CUSTOM_KEY_GCONF[CUSTOM_KEY_DECREASE] = GCONF_KEY_PREFIX"/key_decrease";
    CUSTOM_KEY_GCONF[CUSTOM_KEY_FULLSCREEN]= GCONF_KEY_PREFIX"/key_fullscreen";
    CUSTOM_KEY_GCONF[CUSTOM_KEY_ESC] = GCONF_KEY_PREFIX"/key_esc";

    COLORABLE_GCONF[COLORABLE_MARK] = GCONF_KEY_PREFIX"/color_mark";
    COLORABLE_GCONF[COLORABLE_MARK_VELOCITY]
        = GCONF_KEY_PREFIX"/color_mark_velocity";
    COLORABLE_GCONF[COLORABLE_MARK_OLD] = GCONF_KEY_PREFIX"/color_mark_old";
    COLORABLE_GCONF[COLORABLE_TRACK] = GCONF_KEY_PREFIX"/color_track";
    COLORABLE_GCONF[COLORABLE_TRACK_MARK] =GCONF_KEY_PREFIX"/color_track_mark";
    COLORABLE_GCONF[COLORABLE_TRACK_BREAK]
        = GCONF_KEY_PREFIX"/color_track_break";
    COLORABLE_GCONF[COLORABLE_ROUTE] = GCONF_KEY_PREFIX"/color_route";
    COLORABLE_GCONF[COLORABLE_ROUTE_WAY] = GCONF_KEY_PREFIX"/color_route_way";
    COLORABLE_GCONF[COLORABLE_ROUTE_BREAK]
        = GCONF_KEY_PREFIX"/color_route_break";
    COLORABLE_GCONF[COLORABLE_POI] = GCONF_KEY_PREFIX"/color_poi";

    if(!gconf_client)
    {
        popup_error(_window, _("Failed to initialize GConf.  Quitting."));
        exit(1);
    }

    /* Get GPS Receiver Type.  Default is Bluetooth Receiver. */
    {
        gchar *gri_type_str = gconf_client_get_string(gconf_client,
                GCONF_KEY_GPS_RCVR_TYPE, NULL);
        gint i = 0;
        if(gri_type_str)
        {
            for(i = GPS_RCVR_ENUM_COUNT - 1; i > 0; i--)
                if(!strcmp(gri_type_str, GPS_RCVR_ENUM_TEXT[i]))
                    break;
            g_free(gri_type_str);
        }
        _gri.type = i;
    }

    /* Get Bluetooth Receiver MAC.  Default is NULL. */
    _gri.bt_mac = gconf_client_get_string(
            gconf_client, GCONF_KEY_GPS_BT_MAC, NULL);

    /* Get GPSD Host.  Default is localhost. */
    _gri.gpsd_host = gconf_client_get_string(
            gconf_client, GCONF_KEY_GPS_GPSD_HOST, NULL);
    if(!_gri.gpsd_host)
        _gri.gpsd_host = g_strdup("127.0.0.1");

    /* Get GPSD Port.  Default is GPSD_PORT_DEFAULT (2947). */
    if(!(_gri.gpsd_port = gconf_client_get_int(
            gconf_client, GCONF_KEY_GPS_GPSD_PORT, NULL)))
        _gri.gpsd_port = GPSD_PORT_DEFAULT;

    /* Get File Path.  Default is /dev/pgps. */
    _gri.file_path = gconf_client_get_string(
            gconf_client, GCONF_KEY_GPS_FILE_PATH, NULL);
    if(!_gri.file_path)
        _gri.file_path = g_strdup("/dev/pgps");

    /* Get Auto-Download.  Default is FALSE. */
    _auto_download = gconf_client_get_bool(gconf_client,
            GCONF_KEY_AUTO_DOWNLOAD, NULL);

    /* Get Auto-Download Pre-cache - Default is 2. */
    _auto_download_precache = gconf_client_get_int(gconf_client,
            GCONF_KEY_AUTO_DOWNLOAD_PRECACHE, NULL);
    if(!_auto_download_precache)
        _auto_download_precache = 2;

    /* Get Center Ratio - Default is 5. */
    _center_ratio = gconf_client_get_int(gconf_client,
            GCONF_KEY_CENTER_SENSITIVITY, NULL);
    if(!_center_ratio)
        _center_ratio = 5;

    /* Get Lead Ratio - Default is 5. */
    _lead_ratio = gconf_client_get_int(gconf_client,
            GCONF_KEY_LEAD_AMOUNT, NULL);
    if(!_lead_ratio)
        _lead_ratio = 5;

    /* Get Lead Is Fixed flag - Default is FALSE. */
    _lead_is_fixed = gconf_client_get_bool(gconf_client,
            GCONF_KEY_LEAD_IS_FIXED, NULL);

    /* Get Rotate Sensitivity - Default is 5. */
    _rotate_sens = gconf_client_get_int(gconf_client,
            GCONF_KEY_ROTATE_SENSITIVITY, NULL);
    if(!_rotate_sens)
        _rotate_sens = 5;

    /* Get Auto-Center/Rotate Minimum Speed - Default is 2. */
    value = gconf_client_get(gconf_client, GCONF_KEY_AC_MIN_SPEED, NULL);
    if(value)
    {
        _ac_min_speed = gconf_value_get_int(value);
        gconf_value_free(value);
    }
    else
        _ac_min_speed = 2;

    /* Get Rotate Dir - Default is ROTATE_DIR_UP. */
    {
        gchar *rotate_dir_str = gconf_client_get_string(gconf_client,
                GCONF_KEY_ROTATE_DIR, NULL);
        gint i = -1;
        if(rotate_dir_str)
            for(i = ROTATE_DIR_ENUM_COUNT - 1; i >= 0; i--)
                if(!strcmp(rotate_dir_str, ROTATE_DIR_ENUM_TEXT[i]))
                    break;
        if(i == -1)
            i = ROTATE_DIR_UP;
        _rotate_dir = i;
    }

    /* Get Draw Line Width- Default is 5. */
    _draw_width = gconf_client_get_int(gconf_client,
            GCONF_KEY_DRAW_WIDTH, NULL);
    if(!_draw_width)
        _draw_width = 5;

    /* Get Enable Announcements flag.  Default is TRUE. */
    value = gconf_client_get(gconf_client, GCONF_KEY_ENABLE_ANNOUNCE, NULL);
    if(value)
    {
        _enable_announce = gconf_value_get_bool(value);
        gconf_value_free(value);
    }
    else
        _enable_announce = TRUE;

    /* Get Announce Advance Notice - Default is 30. */
    value = gconf_client_get(gconf_client, GCONF_KEY_ANNOUNCE_NOTICE, NULL);
    if(value)
    {
        _announce_notice_ratio = gconf_value_get_int(value);
        gconf_value_free(value);
    }
    else
        _announce_notice_ratio = 8;

    /* Get Enable Voice flag.  Default is TRUE. */
    value = gconf_client_get(gconf_client, GCONF_KEY_ENABLE_VOICE, NULL);
    if(value)
    {
        _enable_voice = gconf_value_get_bool(value);
        gconf_value_free(value);
    }
    else
        _enable_voice = TRUE;

    /* Get Fullscreen flag. Default is FALSE. */
    _fullscreen = gconf_client_get_bool(gconf_client,
            GCONF_KEY_FULLSCREEN, NULL);

    /* Get Units.  Default is UNITS_KM. */
    {
        gchar *units_str = gconf_client_get_string(gconf_client,
                GCONF_KEY_UNITS, NULL);
        gint i = 0;
        if(units_str)
            for(i = UNITS_ENUM_COUNT - 1; i > 0; i--)
                if(!strcmp(units_str, UNITS_ENUM_TEXT[i]))
                    break;
        _units = i;
    }

    /* Get Custom Key Actions. */
    {
        gint i;
        for(i = 0; i < CUSTOM_KEY_ENUM_COUNT; i++)
        {
            gint j = CUSTOM_KEY_DEFAULT[i];
            gchar *str = gconf_client_get_string(gconf_client,
                    CUSTOM_KEY_GCONF[i], NULL);
            if(str)
                for(j = CUSTOM_ACTION_ENUM_COUNT - 1; j > 0; j--)
                    if(!strcmp(str, CUSTOM_ACTION_ENUM_TEXT[j]))
                        break;
            _action[i] = j;
        }
    }

    /* Get Deg format.  Default is DDPDDDDD. */
    {
        gchar *degformat_key_str = gconf_client_get_string(gconf_client,
                GCONF_KEY_DEG_FORMAT, NULL);
        gint i = 0;
        if(degformat_key_str)
            for(i = DEG_FORMAT_ENUM_COUNT - 1; i > 0; i--)
                if(!strcmp(degformat_key_str, DEG_FORMAT_ENUM_TEXT[i]))
                    break;
        _degformat = i;
    }

    /* Get Speed Limit On flag.  Default is FALSE. */
    _speed_limit_on = gconf_client_get_bool(gconf_client,
            GCONF_KEY_SPEED_LIMIT_ON, NULL);

    /* Get Speed Limit */
    _speed_limit = gconf_client_get_int(gconf_client,
            GCONF_KEY_SPEED_LIMIT, NULL);
    if(_speed_limit <= 0)
        _speed_limit = 100;

    /* Get Speed Location.  Default is SPEED_LOCATION_TOP_LEFT. */
    {
        gchar *speed_location_str = gconf_client_get_string(gconf_client,
                GCONF_KEY_SPEED_LOCATION, NULL);
        gint i = 0;
        if(speed_location_str)
            for(i = SPEED_LOCATION_ENUM_COUNT - 1; i > 0; i--)
                if(!strcmp(speed_location_str, SPEED_LOCATION_ENUM_TEXT[i]))
                    break;
        _speed_location = i;
    }

    /* Get Unblank Option.  Default is UNBLANK_FULLSCREEN. */
    {
        gchar *unblank_option_str = gconf_client_get_string(gconf_client,
                GCONF_KEY_UNBLANK_SIZE, NULL);
        gint i = -1;
        if(unblank_option_str)
            for(i = UNBLANK_ENUM_COUNT - 1; i >= 0; i--)
                if(!strcmp(unblank_option_str, UNBLANK_ENUM_TEXT[i]))
                    break;
        if(i == -1)
            i = UNBLANK_FULLSCREEN;
        _unblank_option = i;
    }

    /* Get Info Font Size.  Default is INFO_FONT_MEDIUM. */
    {
        gchar *info_font_size_str = gconf_client_get_string(gconf_client,
                GCONF_KEY_INFO_FONT_SIZE, NULL);
        gint i = -1;
        if(info_font_size_str)
            for(i = INFO_FONT_ENUM_COUNT - 1; i >= 0; i--)
                if(!strcmp(info_font_size_str, INFO_FONT_ENUM_TEXT[i]))
                    break;
        if(i == -1)
            i = INFO_FONT_MEDIUM;
        _info_font_size = i;
    }

    /* Get last saved latitude.  Default is 50.f. */
    value = gconf_client_get(gconf_client, GCONF_KEY_LAST_LAT, NULL);
    if(value)
    {
        _gps.lat = gconf_value_get_float(value);
        gconf_value_free(value);
    }
    else
        _gps.lat = 50.f;

    /* Get last saved longitude.  Default is 0. */
    _gps.lon = gconf_client_get_float(gconf_client, GCONF_KEY_LAST_LON, NULL);

    /* Get last saved altitude.  Default is 0. */
    _pos.altitude = gconf_client_get_int(
            gconf_client, GCONF_KEY_LAST_ALT, NULL);

    /* Get last saved speed.  Default is 0. */
    _gps.speed = gconf_client_get_float(
            gconf_client, GCONF_KEY_LAST_SPEED, NULL);

    /* Get last saved speed.  Default is 0. */
    _gps.heading = gconf_client_get_float(
            gconf_client, GCONF_KEY_LAST_HEADING, NULL);

    /* Get last saved timestamp.  Default is 0. */
    _pos.time= gconf_client_get_float(gconf_client, GCONF_KEY_LAST_TIME, NULL);

    /* Load the repositories. */
    {
        GSList *list, *curr;
        gint curr_repo_index = gconf_client_get_int(gconf_client,
            GCONF_KEY_CURRREPO, NULL);
        list = gconf_client_get_list(gconf_client,
            GCONF_KEY_REPOSITORIES, GCONF_VALUE_STRING, NULL);

        for(curr = list; curr != NULL; curr = curr->next)
        {
            RepoData *rd = settings_parse_repo(curr->data);
            _repo_list = g_list_append(_repo_list, rd);
            if(!curr_repo_index--)
                repo_set_curr(rd);
            g_free(curr->data);
        }
        g_slist_free(list);
    }


    if(_repo_list == NULL)
    {
        /* We have no repositories - create a default one. */
        RepoData *repo = g_new(RepoData, 1);

        repo->db_filename = gnome_vfs_expand_initial_tilde(
                REPO_DEFAULT_CACHE_DIR);
        repo->url=g_strdup(REPO_DEFAULT_MAP_URI);
        repo->dl_zoom_steps = REPO_DEFAULT_DL_ZOOM_STEPS;
        repo->name = g_strdup(REPO_DEFAULT_NAME);
        repo->view_zoom_steps = REPO_DEFAULT_VIEW_ZOOM_STEPS;
        repo->double_size = FALSE;
        repo->nextable = TRUE;
        repo->min_zoom = REPO_DEFAULT_MIN_ZOOM;
        repo->max_zoom = REPO_DEFAULT_MAX_ZOOM;
        set_repo_type(repo);

        _repo_list = g_list_append(_repo_list, repo);
        repo_set_curr(repo);
    }

    /* Get last center point. */
    {
        gdouble center_lat, center_lon;

        /* Get last saved latitude.  Default is last saved latitude. */
        value = gconf_client_get(gconf_client, GCONF_KEY_CENTER_LAT, NULL);
        if(value)
        {
            center_lat = gconf_value_get_float(value);
            gconf_value_free(value);
        }
        else
        {
            _is_first_time = TRUE;
            center_lat = _gps.lat;
        }

        /* Get last saved longitude.  Default is last saved longitude. */
        value = gconf_client_get(gconf_client, GCONF_KEY_CENTER_LON, NULL);
        if(value)
        {
            center_lon = gconf_value_get_float(value);
            gconf_value_free(value);
        }
        else
            center_lon = _gps.lon;

        latlon2unit(center_lat, center_lon, &_center.unitx, &_center.unity, _curr_repo->units);
        _next_center = _center;
    }

    /* Get map correction.  Default is 0. */
    _map_correction_unitx = gconf_client_get_int(gconf_client,
            GCONF_KEY_MAP_CORRECTION_UNITX, NULL);
    _map_correction_unity = gconf_client_get_int(gconf_client,
            GCONF_KEY_MAP_CORRECTION_UNITY, NULL);

    /* Get last viewing angle.  Default is 0. */
    _map_rotate_angle = _next_map_rotate_angle = gconf_client_get_int(
            gconf_client, GCONF_KEY_CENTER_ANGLE, NULL);
    gdk_pixbuf_rotate_matrix_fill_for_rotation(
            _map_rotate_matrix,
            deg2rad(ROTATE_DIR_ENUM_DEGREES[_rotate_dir] - _map_rotate_angle));
    gdk_pixbuf_rotate_matrix_fill_for_rotation(
            _map_reverse_matrix,
            deg2rad(_map_rotate_angle - ROTATE_DIR_ENUM_DEGREES[_rotate_dir]));


    /* Get last Zoom Level.  Default is 16. */
    value = gconf_client_get(gconf_client, GCONF_KEY_ZOOM, NULL);
    if(value)
    {
        _zoom = gconf_value_get_int(value) / _curr_repo->view_zoom_steps
            * _curr_repo->view_zoom_steps;
        gconf_value_free(value);
    }
    else
        _zoom = 16 / _curr_repo->view_zoom_steps
            * _curr_repo->view_zoom_steps;
    BOUND(_zoom, 0, MAX_ZOOM);
    _next_zoom = _zoom;

    /* Get Route Directory.  Default is NULL. */
    _route_dir_uri = gconf_client_get_string(gconf_client,
            GCONF_KEY_ROUTEDIR, NULL);

    /* Get Last Track File.  Default is NULL. */
    _track_file_uri = gconf_client_get_string(gconf_client,
            GCONF_KEY_TRACKFILE, NULL);

    /* Get Auto-Center Mode.  Default is CENTER_LEAD. */
    value = gconf_client_get(gconf_client, GCONF_KEY_AUTOCENTER_MODE, NULL);
    if(value)
    {
        _center_mode = gconf_value_get_int(value);
        gconf_value_free(value);
    }
    else
        _center_mode = CENTER_LEAD;

    /* Get Auto-Center Rotate Flag.  Default is TRUE. */
    value = gconf_client_get(gconf_client, GCONF_KEY_AUTOCENTER_ROTATE, NULL);
    if(value)
    {
        _center_rotate = gconf_value_get_bool(value);
        gconf_value_free(value);
    }
    else
        _center_rotate = TRUE;

    /* Get Show Zoom Level flag.  Default is FALSE. */
    _show_zoomlevel = gconf_client_get_bool(gconf_client,
            GCONF_KEY_SHOWZOOMLEVEL, NULL);

    /* Get Show Scale flag.  Default is TRUE. */
    value = gconf_client_get(gconf_client, GCONF_KEY_SHOWSCALE, NULL);
    if(value)
    {
        _show_scale = gconf_value_get_bool(value);
        gconf_value_free(value);
    }
    else
        _show_scale = TRUE;

    /* Get Show Compass Rose flag.  Default is TRUE. */
    value = gconf_client_get(gconf_client, GCONF_KEY_SHOWCOMPROSE, NULL);
    if(value)
    {
        _show_comprose = gconf_value_get_bool(value);
        gconf_value_free(value);
    }
    else
        _show_comprose = TRUE;

    /* Get Show Tracks flag.  Default is TRUE. */
    value = gconf_client_get(gconf_client, GCONF_KEY_SHOWTRACKS, NULL);
    if(value)
    {
        _show_paths |= (gconf_value_get_bool(value) ? TRACKS_MASK : 0);
        gconf_value_free(value);
    }
    else
        _show_paths |= TRACKS_MASK;

    /* Get Show Routes flag.  Default is TRUE. */
    value = gconf_client_get(gconf_client, GCONF_KEY_SHOWROUTES, NULL);
    if(value)
    {
        _show_paths |= (gconf_value_get_bool(value) ? ROUTES_MASK : 0);
        gconf_value_free(value);
    }
    else
        _show_paths |= ROUTES_MASK;

    /* Get Show Velocity Vector flag.  Default is TRUE. */
    value = gconf_client_get(gconf_client, GCONF_KEY_SHOWVELVEC, NULL);
    if(value)
    {
        _show_velvec = gconf_value_get_bool(value);
        gconf_value_free(value);
    }
    else
        _show_velvec = TRUE;

    /* Get Show Velocity Vector flag.  Default is TRUE. */
    value = gconf_client_get(gconf_client, GCONF_KEY_SHOWPOIS, NULL);
    if(value)
    {
        _show_poi = gconf_value_get_bool(value);
        gconf_value_free(value);
    }
    else
        _show_poi = TRUE;

    /* Get Enable GPS flag.  Default is TRUE. */
    value = gconf_client_get(gconf_client, GCONF_KEY_ENABLE_GPS, NULL);
    if(value)
    {
        _enable_gps = gconf_value_get_bool(value);
        gconf_value_free(value);
    }
    else
        _enable_gps = TRUE;

    /* Get Enable Tracking flag.  Default is TRUE. */
    value = gconf_client_get(gconf_client, GCONF_KEY_ENABLE_TRACKING, NULL);
    if(value)
    {
        _enable_tracking = gconf_value_get_bool(value);
        gconf_value_free(value);
    }
    else
        _enable_tracking = TRUE;

    /* Initialize _gps_state based on _enable_gps. */
    _gps_state = RCVR_OFF;

    /* Load the route locations. */
    {
        GSList *curr;
        _loc_list = gconf_client_get_list(gconf_client,
            GCONF_KEY_ROUTE_LOCATIONS, GCONF_VALUE_STRING, NULL);
        _loc_model = gtk_list_store_new(1, G_TYPE_STRING);
        for(curr = _loc_list; curr != NULL; curr = curr->next)
        {
            GtkTreeIter iter;
            gtk_list_store_insert_with_values(_loc_model, &iter, INT_MAX,
                    0, curr->data, -1);
        }
    }

    /* Get POI Database.  Default is in REPO_DEFAULT_CACHE_BASE */
    _poi_db_filename = gconf_client_get_string(gconf_client,
            GCONF_KEY_POI_DB, NULL);
    if(_poi_db_filename == NULL)
    {
        gchar *poi_base = gnome_vfs_expand_initial_tilde(
                REPO_DEFAULT_CACHE_BASE);
        _poi_db_filename = gnome_vfs_uri_make_full_from_relative(
                poi_base, "poi.db");
        g_free(poi_base);
    }

    _poi_db_dirname = g_path_get_dirname(_poi_db_filename);

    _poi_zoom = gconf_client_get_int(gconf_client,
            GCONF_KEY_POI_ZOOM, NULL);
    if(!_poi_zoom)
        _poi_zoom = MAX_ZOOM - 10;


    /* Get GPS Info flag.  Default is FALSE. */
    _gps_info = gconf_client_get_bool(gconf_client, GCONF_KEY_GPS_INFO, NULL);

    /* Get Route Download URL.  Default is:
     * "http://www.gnuite.com/cgi-bin/gpx.cgi?saddr=%s&daddr=%s" */
    _route_dl_url = gconf_client_get_string(gconf_client,
            GCONF_KEY_ROUTE_DL_URL, NULL);
    if(_route_dl_url == NULL)
        _route_dl_url = g_strdup(
                "http://www.gnuite.com/cgi-bin/gpx.cgi?saddr=%s&daddr=%s");

    /* Get Route Download Radius.  Default is 4. */
    value = gconf_client_get(gconf_client, GCONF_KEY_ROUTE_DL_RADIUS, NULL);
    if(value)
    {
        _route_dl_radius = gconf_value_get_int(value);
        gconf_value_free(value);
    }
    else
        _route_dl_radius = 8;

    /* Get POI Download URL.  Default is:
     * "http://www.gnuite.com/cgi-bin/poi.cgi?saddr=%s&query=%s&page=%d" */
    _poi_dl_url = gconf_client_get_string(gconf_client,
            GCONF_KEY_POI_DL_URL, NULL);
    if(_poi_dl_url == NULL)
        _poi_dl_url = g_strdup(
            "http://www.gnuite.com/cgi-bin/poi.cgi?saddr=%s&query=%s&page=%d");

    /* Get Colors. */
    {
        gint i;
        for(i = 0; i < COLORABLE_ENUM_COUNT; i++)
        {
            str = gconf_client_get_string(gconf_client,
                    COLORABLE_GCONF[i], NULL);
            if(!str || !gdk_color_parse(str, &_color[i]))
                _color[i] = COLORABLE_DEFAULT[i];
        }
    }

    gconf_client_clear_cache(gconf_client);
    g_object_unref(gconf_client);

    /* GPS data init */
    _gps.fix = 1;
    _gps.satinuse = 0;
    _gps.satinview = 0;

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

