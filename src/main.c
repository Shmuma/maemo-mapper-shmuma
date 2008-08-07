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

#ifdef HAVE_CONFIG_H
#    include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <dbus/dbus-glib.h>
#include <locale.h>

#include <gconf/gconf-client.h>
#include <device_symbols.h>
#include <conicconnection.h>
#include <conicconnectionevent.h>

#ifndef LEGACY
#    include <hildon/hildon-program.h>
#    include <hildon/hildon-banner.h>
#else
#    include <hildon-widgets/hildon-program.h>
#    include <hildon-widgets/hildon-banner.h>
#endif

#include "types.h"
#include "data.h"
#include "defines.h"

#include "cmenu.h"
#include "dbus-ifc.h"
#include "display.h"
#include "gps.h"
#include "gpx.h"
#include "input.h"
#include "main.h"
#include "maps.h"
#include "menu.h"
#include "path.h"
#include "poi.h"
#include "settings.h"
#include "util.h"

static void osso_cb_hw_state(osso_hw_state_t *state, gpointer data);

static HildonProgram *_program = NULL;

static ConIcConnection *_conic_conn = NULL;
static gboolean _conic_is_connecting = FALSE;
static gboolean _conic_conn_failed = FALSE;
static GMutex *_conic_connection_mutex = NULL;
static GCond *_conic_connection_cond = NULL;

/* Dynamically-sized in-memory map cache. */
static size_t _map_cache_size = (32*1024*1024);
static gboolean _map_cache_enabled = TRUE;

static void
conic_conn_event(ConIcConnection *connection, ConIcConnectionEvent *event)
{
    ConIcConnectionStatus status;
    printf("%s()\n", __PRETTY_FUNCTION__);

    g_mutex_lock(_conic_connection_mutex);

    status = con_ic_connection_event_get_status(event);

    if((_conic_is_connected = (status == CON_IC_STATUS_CONNECTED)))
    {
        /* We're connected. */
        _conic_conn_failed = FALSE;
        if(_download_banner != NULL)
            gtk_widget_show(_download_banner);
    }
    else
    {
        /* We're not connected. */
        /* Mark as a failed connection, if we had been trying to connect. */
        _conic_conn_failed = _conic_is_connecting;
        if(_download_banner != NULL)
            gtk_widget_hide(_download_banner);
    }

    _conic_is_connecting = FALSE; /* No longer trying to connect. */
    g_cond_broadcast(_conic_connection_cond);
    g_mutex_unlock(_conic_connection_mutex);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

void
conic_recommend_connected()
{
    printf("%s()\n", __PRETTY_FUNCTION__);

#ifdef __arm__
    g_mutex_lock(_conic_connection_mutex);
    if(!_conic_is_connecting)
    {
        /* Fire up a connection request. */
        con_ic_connection_connect(_conic_conn, CON_IC_CONNECT_FLAG_NONE);
        _conic_is_connecting = TRUE;
    }
    g_mutex_unlock(_conic_connection_mutex);
#endif

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

gboolean
conic_ensure_connected()
{
    printf("%s()\n", __PRETTY_FUNCTION__);

#ifdef __arm__
    while(_window && !_conic_is_connected)
    {   
        g_mutex_lock(_conic_connection_mutex);
        /* If we're not connected, and if we're not connecting, and if we're
         * not in the wake of a connection failure, then try to connect. */
        if(!_conic_is_connected && !_conic_is_connecting &&!_conic_conn_failed)
        {
            /* Fire up a connection request. */
            con_ic_connection_connect(_conic_conn, CON_IC_CONNECT_FLAG_NONE);
            _conic_is_connecting = TRUE;
        }
        g_cond_wait(_conic_connection_cond, _conic_connection_mutex);
        g_mutex_unlock(_conic_connection_mutex);
    }
#else
    _conic_is_connected = TRUE;
#endif

    vprintf("%s(): return %d\n", __PRETTY_FUNCTION__,
            _window && _conic_is_connected);
    return _window && _conic_is_connected;
}

/**
 * Save state and destroy all non-UI elements created by this program in
 * preparation for exiting.
 */
static void
maemo_mapper_destroy()
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    /* _program and widgets have already been destroyed. */
    _window = NULL;

    gps_destroy(FALSE);

    path_destroy();

    settings_save();

    poi_destroy();

    g_mutex_lock(_mut_priority_mutex);
    _mut_priority_tree = g_tree_new((GCompareFunc)mut_priority_comparefunc);
    g_mutex_unlock(_mut_priority_mutex);

    /* Allow remaining downloads to finish. */
    g_mutex_lock(_conic_connection_mutex);
    g_cond_broadcast(_conic_connection_cond);
    g_mutex_unlock(_conic_connection_mutex);
    g_thread_pool_free(_mut_thread_pool, TRUE, TRUE);

    if(_curr_repo->db)
    {
        RepoData* repo_p;
#ifdef MAPDB_SQLITE
        g_mutex_lock(_mapdb_mutex);
        sqlite3_close(_curr_repo->db);
        _curr_repo->db = NULL;
        g_mutex_unlock(_mapdb_mutex);
#else
        g_mutex_lock(_mapdb_mutex);
        repo_p = _curr_repo;
        while (repo_p) {
            if (repo_p->db) {
                /* perform reorganization for layers which are auto refreshed */
                if (repo_p->layer_level && repo_p->layer_refresh_interval)
                    gdbm_reorganize (repo_p->db);
                gdbm_close(repo_p->db);
            }
            repo_p->db = NULL;
            repo_p = repo_p->layers;
        }
        g_mutex_unlock(_mapdb_mutex);
#endif
    }
    map_cache_destroy();

    gps_destroy(TRUE);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

/**
 * Initialize everything required in preparation for calling gtk_main().
 */
static void
maemo_mapper_init(gint argc, gchar **argv)
{
    GtkWidget *hbox, *label, *vbox;
    printf("%s()\n", __PRETTY_FUNCTION__);

    /* Set enum-based constants. */
    UNITS_ENUM_TEXT[UNITS_KM] = _("km");
    UNITS_ENUM_TEXT[UNITS_MI] = _("mi.");
    UNITS_ENUM_TEXT[UNITS_NM] = _("n.m.");

    ROTATE_DIR_ENUM_TEXT[ROTATE_DIR_UP] = _("Up");
    ROTATE_DIR_ENUM_TEXT[ROTATE_DIR_RIGHT] = _("Right");
    ROTATE_DIR_ENUM_TEXT[ROTATE_DIR_DOWN] = _("Down");
    ROTATE_DIR_ENUM_TEXT[ROTATE_DIR_LEFT] = _("Left");

    UNBLANK_ENUM_TEXT[UNBLANK_WITH_GPS] = _("When Receiving Any GPS Data");
    UNBLANK_ENUM_TEXT[UNBLANK_WHEN_MOVING] = _("When Moving");
    UNBLANK_ENUM_TEXT[UNBLANK_FULLSCREEN] = _("When Moving (Full Screen Only)");
    UNBLANK_ENUM_TEXT[UNBLANK_WAYPOINT] = _("When Approaching a Waypoint");
    UNBLANK_ENUM_TEXT[UNBLANK_NEVER] = _("Never");

    INFO_FONT_ENUM_TEXT[INFO_FONT_XXSMALL] = "xx-small";
    INFO_FONT_ENUM_TEXT[INFO_FONT_XSMALL] = "x-small";
    INFO_FONT_ENUM_TEXT[INFO_FONT_SMALL] = "small";
    INFO_FONT_ENUM_TEXT[INFO_FONT_MEDIUM] = "medium";
    INFO_FONT_ENUM_TEXT[INFO_FONT_LARGE] = "large";
    INFO_FONT_ENUM_TEXT[INFO_FONT_XLARGE] = "x-large";
    INFO_FONT_ENUM_TEXT[INFO_FONT_XXLARGE] = "xx-large";

    CUSTOM_KEY_ICON[CUSTOM_KEY_UP] = HWK_BUTTON_UP;
    CUSTOM_KEY_ICON[CUSTOM_KEY_LEFT] = HWK_BUTTON_LEFT;
    CUSTOM_KEY_ICON[CUSTOM_KEY_DOWN] = HWK_BUTTON_DOWN;
    CUSTOM_KEY_ICON[CUSTOM_KEY_RIGHT] = HWK_BUTTON_RIGHT;
    CUSTOM_KEY_ICON[CUSTOM_KEY_SELECT] = HWK_BUTTON_SELECT;
    CUSTOM_KEY_ICON[CUSTOM_KEY_INCREASE] = HWK_BUTTON_INCREASE;
    CUSTOM_KEY_ICON[CUSTOM_KEY_DECREASE] = HWK_BUTTON_DECREASE;
    CUSTOM_KEY_ICON[CUSTOM_KEY_FULLSCREEN] = HWK_BUTTON_VIEW;
    CUSTOM_KEY_ICON[CUSTOM_KEY_ESC] = HWK_BUTTON_CANCEL;

    CUSTOM_KEY_DEFAULT[CUSTOM_KEY_UP] = CUSTOM_ACTION_RESET_VIEW_ANGLE;
    CUSTOM_KEY_DEFAULT[CUSTOM_KEY_LEFT] =CUSTOM_ACTION_ROTATE_COUNTERCLOCKWISE;
    CUSTOM_KEY_DEFAULT[CUSTOM_KEY_DOWN] = CUSTOM_ACTION_TOGGLE_AUTOROTATE;
    CUSTOM_KEY_DEFAULT[CUSTOM_KEY_RIGHT] = CUSTOM_ACTION_ROTATE_CLOCKWISE;
    CUSTOM_KEY_DEFAULT[CUSTOM_KEY_SELECT] = CUSTOM_ACTION_TOGGLE_AUTOCENTER;
    CUSTOM_KEY_DEFAULT[CUSTOM_KEY_INCREASE] = CUSTOM_ACTION_ZOOM_IN;
    CUSTOM_KEY_DEFAULT[CUSTOM_KEY_DECREASE] = CUSTOM_ACTION_ZOOM_OUT;
    CUSTOM_KEY_DEFAULT[CUSTOM_KEY_FULLSCREEN]= CUSTOM_ACTION_TOGGLE_FULLSCREEN;
    CUSTOM_KEY_DEFAULT[CUSTOM_KEY_ESC] = CUSTOM_ACTION_TOGGLE_TRACKS;

    CUSTOM_ACTION_ENUM_TEXT[CUSTOM_ACTION_PAN_NORTH] = _("Pan North");
    CUSTOM_ACTION_ENUM_TEXT[CUSTOM_ACTION_PAN_WEST] = _("Pan West");
    CUSTOM_ACTION_ENUM_TEXT[CUSTOM_ACTION_PAN_SOUTH] = _("Pan South");
    CUSTOM_ACTION_ENUM_TEXT[CUSTOM_ACTION_PAN_EAST] = _("Pan East");
    CUSTOM_ACTION_ENUM_TEXT[CUSTOM_ACTION_PAN_UP] = _("Pan Up");
    CUSTOM_ACTION_ENUM_TEXT[CUSTOM_ACTION_PAN_DOWN] = _("Pan Down");
    CUSTOM_ACTION_ENUM_TEXT[CUSTOM_ACTION_PAN_LEFT] = _("Pan Left");
    CUSTOM_ACTION_ENUM_TEXT[CUSTOM_ACTION_PAN_RIGHT] = _("Pan Right");
    CUSTOM_ACTION_ENUM_TEXT[CUSTOM_ACTION_RESET_VIEW_ANGLE]
        = _("Reset Viewing Angle");
    CUSTOM_ACTION_ENUM_TEXT[CUSTOM_ACTION_ROTATE_CLOCKWISE]
        = _("Rotate View Clockwise");
    CUSTOM_ACTION_ENUM_TEXT[CUSTOM_ACTION_ROTATE_COUNTERCLOCKWISE]
        = _("Rotate View Counter-Clockwise");
    CUSTOM_ACTION_ENUM_TEXT[CUSTOM_ACTION_TOGGLE_AUTOCENTER]
        = _("Toggle Auto-Center");
    CUSTOM_ACTION_ENUM_TEXT[CUSTOM_ACTION_TOGGLE_AUTOROTATE]
        = _("Toggle Auto-Rotate");
    CUSTOM_ACTION_ENUM_TEXT[CUSTOM_ACTION_TOGGLE_FULLSCREEN]
        = _("Toggle Fullscreen");
    CUSTOM_ACTION_ENUM_TEXT[CUSTOM_ACTION_ZOOM_IN] = _("Zoom In");
    CUSTOM_ACTION_ENUM_TEXT[CUSTOM_ACTION_ZOOM_OUT] = _("Zoom Out");
    CUSTOM_ACTION_ENUM_TEXT[CUSTOM_ACTION_TOGGLE_TRACKING]
        = _("Toggle Tracking");
    CUSTOM_ACTION_ENUM_TEXT[CUSTOM_ACTION_TOGGLE_TRACKS]
        = _("Toggle Tracks/Routes");
    CUSTOM_ACTION_ENUM_TEXT[CUSTOM_ACTION_TOGGLE_SCALE] = _("Toggle Scale");
    CUSTOM_ACTION_ENUM_TEXT[CUSTOM_ACTION_TOGGLE_POI] = _("Toggle POIs");
    CUSTOM_ACTION_ENUM_TEXT[CUSTOM_ACTION_CHANGE_REPO]
        = _("Select Next Repository");
    CUSTOM_ACTION_ENUM_TEXT[CUSTOM_ACTION_ROUTE_DISTNEXT]
        = _("Show Distance to Next Waypoint");
    CUSTOM_ACTION_ENUM_TEXT[CUSTOM_ACTION_ROUTE_DISTLAST]
        = _("Show Distance to End of Route");
    CUSTOM_ACTION_ENUM_TEXT[CUSTOM_ACTION_TRACK_BREAK]=_("Insert Track Break");
    CUSTOM_ACTION_ENUM_TEXT[CUSTOM_ACTION_TRACK_CLEAR] = _("Clear Track");
    CUSTOM_ACTION_ENUM_TEXT[CUSTOM_ACTION_TRACK_DISTLAST]
        = _("Show Distance from Last Break");
    CUSTOM_ACTION_ENUM_TEXT[CUSTOM_ACTION_TRACK_DISTFIRST]
        = _("Show Distance from Beginning");
    CUSTOM_ACTION_ENUM_TEXT[CUSTOM_ACTION_TOGGLE_GPS] = _("Toggle GPS");
    CUSTOM_ACTION_ENUM_TEXT[CUSTOM_ACTION_TOGGLE_GPSINFO]=_("Toggle GPS Info");
    CUSTOM_ACTION_ENUM_TEXT[CUSTOM_ACTION_TOGGLE_SPEEDLIMIT]
        = _("Toggle Speed Limit");
    CUSTOM_ACTION_ENUM_TEXT[CUSTOM_ACTION_RESET_BLUETOOTH]
        = _("Reset Bluetooth");
    CUSTOM_ACTION_ENUM_TEXT[CUSTOM_ACTION_TOGGLE_LAYERS] = _("Toggle Layers");

    DEG_FORMAT_ENUM_TEXT[DDPDDDDD] = "-dd.ddddd°";
    DEG_FORMAT_ENUM_TEXT[DD_MMPMMM] = "-dd°mm.mmm'";
    DEG_FORMAT_ENUM_TEXT[DD_MM_SSPS] = "-dd°mm'ss.s\"";
    DEG_FORMAT_ENUM_TEXT[DDPDDDDD_NSEW] = "dd.ddddd° S";
    DEG_FORMAT_ENUM_TEXT[DD_MMPMMM_NSEW] = "dd°mm.mmm' S";
    DEG_FORMAT_ENUM_TEXT[DD_MM_SSPS_NSEW] = "dd°mm'ss.s\" S";
    DEG_FORMAT_ENUM_TEXT[NSEW_DDPDDDDD] = "S dd.ddddd°";
    DEG_FORMAT_ENUM_TEXT[NSEW_DD_MMPMMM] = "S dd° mm.mmm'";
    DEG_FORMAT_ENUM_TEXT[NSEW_DD_MM_SSPS] = "S dd° mm' ss.s\"";

    SPEED_LOCATION_ENUM_TEXT[SPEED_LOCATION_TOP_LEFT] = _("Top-Left");
    SPEED_LOCATION_ENUM_TEXT[SPEED_LOCATION_TOP_RIGHT] = _("Top-Right");
    SPEED_LOCATION_ENUM_TEXT[SPEED_LOCATION_BOTTOM_RIGHT] = _("Bottom-Right");
    SPEED_LOCATION_ENUM_TEXT[SPEED_LOCATION_BOTTOM_LEFT] = _("Bottom-Left");

    GPS_RCVR_ENUM_TEXT[GPS_RCVR_BT] = _("Bluetooth");
    GPS_RCVR_ENUM_TEXT[GPS_RCVR_GPSD] = _("GPSD");
    GPS_RCVR_ENUM_TEXT[GPS_RCVR_FILE] = _("File");

    /* Set up track array (must be done before config). */
    memset(&_track, 0, sizeof(_track));
    memset(&_route, 0, sizeof(_route));
    /* initialisation of paths is done in path_init() */

    _mapdb_mutex = g_mutex_new();
    _mut_priority_mutex = g_mutex_new();
    _mouse_mutex = g_mutex_new();

    _conic_connection_mutex = g_mutex_new();
    _conic_connection_cond = g_cond_new();

    settings_init();
    map_cache_init(_map_cache_size);

    /* Initialize _program. */
    _program = HILDON_PROGRAM(hildon_program_get_instance());
    g_set_application_name("Maemo Mapper");

    /* Initialize _window. */
    _window = GTK_WIDGET(hildon_window_new());
    hildon_program_add_window(_program, HILDON_WINDOW(_window));

    /* Lets go fullscreen if so requested in saved config */
    if (_fullscreen) {
      gtk_window_fullscreen(GTK_WINDOW(_window));
    }

    /* Create and add widgets and supporting data. */
    hbox = gtk_hbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(_window), hbox);

    _gps_widget = gtk_frame_new("GPS Info");
    gtk_container_add(GTK_CONTAINER(_gps_widget),
            vbox = gtk_vbox_new(FALSE, 0));
    gtk_widget_set_size_request(GTK_WIDGET(_gps_widget), 180, 0);
    gtk_box_pack_start(GTK_BOX(hbox), _gps_widget, FALSE, TRUE, 0);

    label = gtk_label_new(" ");
    gtk_widget_set_size_request(GTK_WIDGET(label), -1, 10);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, TRUE, 0);

    _text_lat = gtk_label_new(" --- ");
    gtk_widget_set_size_request(GTK_WIDGET(_text_lat), -1, 30);
    gtk_box_pack_start(GTK_BOX(vbox), _text_lat, FALSE, TRUE, 0);

    _text_lon = gtk_label_new(" --- ");
    gtk_widget_set_size_request(GTK_WIDGET(_text_lon), -1, 30);
    gtk_box_pack_start(GTK_BOX(vbox), _text_lon, FALSE, TRUE, 0);

    _text_speed = gtk_label_new(" --- ");
    gtk_widget_set_size_request(GTK_WIDGET(_text_speed), -1, 30);
    gtk_box_pack_start(GTK_BOX(vbox), _text_speed, FALSE, TRUE, 0);

    _text_alt = gtk_label_new(" --- ");
    gtk_widget_set_size_request(GTK_WIDGET(_text_alt), -1, 30);
    gtk_box_pack_start(GTK_BOX(vbox), _text_alt, FALSE, TRUE, 0);

    label = gtk_label_new(" ");
    gtk_widget_set_size_request(GTK_WIDGET(label), -1, 10);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, TRUE, 0);

    _sat_panel = gtk_drawing_area_new ();
    gtk_widget_set_size_request (_sat_panel, -1, 100);
    gtk_box_pack_start(GTK_BOX(vbox), _sat_panel, TRUE, TRUE, 0);

    label = gtk_label_new(" ");
    gtk_widget_set_size_request(GTK_WIDGET(label), -1, 10);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, TRUE, 0);

    _text_time = gtk_label_new("--:--:--");
    gtk_widget_set_size_request(GTK_WIDGET(_text_time), -1, 30);
    gtk_box_pack_start(GTK_BOX(vbox), _text_time, FALSE, TRUE, 0);

    _heading_panel = gtk_drawing_area_new ();
    gtk_widget_set_size_request (_heading_panel, -1, 100);
    gtk_box_pack_start(GTK_BOX(vbox), _heading_panel, TRUE, TRUE, 0);

    _map_widget = gtk_drawing_area_new();

    gtk_box_pack_start(GTK_BOX(hbox), _map_widget, TRUE, TRUE, 0);

    gtk_widget_show_all(hbox);
    gps_show_info(); /* hides info, if necessary. */

    gtk_widget_realize(_map_widget);

    /* Tweak the foreground and background colors a little bit... */
    {
        GdkColor color;
        GdkGCValues values;
        GdkColormap *colormap = gtk_widget_get_colormap(_map_widget);

        gdk_gc_get_values(
                _map_widget->style->fg_gc[GTK_STATE_NORMAL],
                &values);
        gdk_colormap_query_color(colormap, values.foreground.pixel, &color);
        gtk_widget_modify_fg(_map_widget, GTK_STATE_ACTIVE, &color);

        gdk_gc_get_values(
                _map_widget->style->bg_gc[GTK_STATE_NORMAL],
                &values);
        gdk_colormap_query_color(colormap, values.foreground.pixel, &color);
        gtk_widget_modify_bg(_map_widget, GTK_STATE_ACTIVE, &color);

        /* Use a black background for _map_widget, since missing tiles are
         * also drawn with a black background. */
        color.red = 0; color.green = 0; color.blue = 0;
        gtk_widget_modify_bg(_map_widget,
                GTK_STATE_NORMAL, &color);
    }

    _map_pixmap = gdk_pixmap_new(_map_widget->window, 1, 1, -1);
    /* -1: use bit depth of widget->window. */

    _map_pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, 1, 1);

    _mut_exists_table = g_hash_table_new(
            (GHashFunc)mut_exists_hashfunc, (GEqualFunc)mut_exists_equalfunc);
    _mut_priority_tree = g_tree_new((GCompareFunc)mut_priority_comparefunc);

    _mut_thread_pool = g_thread_pool_new(
            (GFunc)thread_proc_mut, NULL, NUM_DOWNLOAD_THREADS, FALSE, NULL);
    _mrt_thread_pool = g_thread_pool_new(
            (GFunc)thread_render_map, NULL, 1, FALSE, NULL);

    /* Connect signals. */
    g_signal_connect(G_OBJECT(_window), "destroy",
            G_CALLBACK(gtk_main_quit), NULL);

    memset(&_autoroute_data, 0, sizeof(_autoroute_data));

    latlon2unit(_gps.lat, _gps.lon, _pos.unitx, _pos.unity);

    /* Initialize our line styles. */
    update_gcs();

    menu_init();
    cmenu_init();
    path_init();
    gps_init();
    input_init();
    poi_db_connect();
    display_init();
    dbus_ifc_init();

    /* If present, attempt to load the file specified on the command line. */
    if(argc > 1)
    {
        GnomeVFSResult vfs_result;
        gint size;
        gchar *buffer;
        gchar *file_uri;

        /* Get the selected filename. */
        file_uri = gnome_vfs_make_uri_from_shell_arg(argv[1]);

        if(GNOME_VFS_OK != (vfs_result = gnome_vfs_read_entire_file(
                        file_uri, &size, &buffer)))
        {
            gchar buffer[BUFFER_SIZE];
            snprintf(buffer, sizeof(buffer),
                    "%s:\n%s", _("Failed to open file for reading"),
                    gnome_vfs_result_to_string(vfs_result));
            popup_error(_window, buffer);
        }
        else
        {
            if(gpx_path_parse(&_route, buffer, size, 0))
            {
                path_save_route_to_db();
                MACRO_BANNER_SHOW_INFO(_window, _("Route Opened"));
            }
            else
                popup_error(_window, _("Error parsing GPX file."));
            g_free(buffer);
        }
        g_free(file_uri);
    }

    /* If we have a route, calculate the next point. */
    route_find_nearest_point();

    _conic_conn = con_ic_connection_new();
    g_object_set(_conic_conn, "automatic-connection-events", TRUE, NULL);
    g_signal_connect(G_OBJECT(_conic_conn), "connection-event",
            G_CALLBACK(conic_conn_event), NULL);

    g_idle_add((GSourceFunc)window_present, NULL);


    osso_hw_set_event_cb(_osso, NULL, osso_cb_hw_state, NULL);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

static gboolean
osso_cb_hw_state_idle(osso_hw_state_t *state)
{
    static gboolean _must_save_data = FALSE;
    printf("%s(inact=%d, save=%d, shut=%d, memlow=%d, state=%d)\n",
            __PRETTY_FUNCTION__, state->system_inactivity_ind,
            state->save_unsaved_data_ind, state->shutdown_ind,
            state->memory_low_ind, state->sig_device_mode_ind);

    if(state->shutdown_ind)
    {
        maemo_mapper_destroy();
        exit(1);
    }

    if(state->save_unsaved_data_ind)
    {
        settings_save();
        _must_save_data = TRUE;
    }

    if(state->memory_low_ind)
    {
        // Disable the map cache and set the next max cache size to
        // slightly less than the current cache size.
        _map_cache_size = map_cache_resize(0) * 0.8;
        _map_cache_enabled = FALSE;
    }
    else
    {
        if(!_map_cache_enabled)
        {
            // Restore the map cache.
            map_cache_resize(_map_cache_size);
            _map_cache_enabled = TRUE;
        }
    }

    g_free(state);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return FALSE;
}

static void
osso_cb_hw_state(osso_hw_state_t *state, gpointer data)
{
    printf("%s()\n", __PRETTY_FUNCTION__);
    osso_hw_state_t *state_copy = g_new(osso_hw_state_t, 1);
    memcpy(state_copy, state, sizeof(osso_hw_state_t));
    g_idle_add((GSourceFunc)osso_cb_hw_state_idle, state_copy);
    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

gint
main(gint argc, gchar *argv[])
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    /* Initialize localization. */
    setlocale(LC_ALL, "");
    bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
    textdomain(GETTEXT_PACKAGE);

    g_thread_init(NULL);

    /* Initialize _osso. */
    _osso = osso_initialize("com.gnuite.maemo_mapper", VERSION, TRUE, NULL);
    if(!_osso)
    {
        g_printerr("osso_initialize failed.\n");
        return 1;
    }

    gtk_init(&argc, &argv);

    /* Init gconf. */
    g_type_init();
    gconf_init(argc, argv, NULL);

    /* Init Gnome-VFS. */
    gnome_vfs_init();

#ifdef DEBUG
    /* This is just some helpful DBUS testing code. */
    if(argc >= 3)
    {
        /* Try to set the center to a new lat/lon. */
        GError *error = NULL;
        gchar *error_check;
        gdouble lat, lon;
        DBusGConnection *bus;
        DBusGProxy *proxy;
        
        bus = dbus_g_bus_get(DBUS_BUS_SESSION, &error);
        if(!bus || error)
        {
            g_printerr("Error: %s\n", error->message);
            return -1;
        }

        proxy = dbus_g_proxy_new_for_name(bus,
                        MM_DBUS_SERVICE, MM_DBUS_PATH, MM_DBUS_INTERFACE);

        lat = g_ascii_strtod((argv[1]), &error_check);
        if(error_check == argv[1])
        {
            g_printerr("Failed to parse string as float: %s\n", argv[1]);
            return 1;
        }

        lon = g_ascii_strtod((argv[2]), &error_check);
        if(error_check == argv[2])
        {
            g_printerr("Failed to parse string as float: %s\n", argv[2]);
            return 2;
        }

        error = NULL;
        if(argc >= 4)
        {
            /* We are specifying a zoom. */
            gint zoom;

            zoom = g_ascii_strtod((argv[3]), &error_check);
            if(error_check == argv[3])
            {
                g_printerr("Failed to parse string as integer: %s\n", argv[3]);
                return 3;
            }
            if(!dbus_g_proxy_call(proxy, MM_DBUS_METHOD_SET_VIEW_POSITION,
                        &error,
                        G_TYPE_DOUBLE, lat, G_TYPE_DOUBLE, lon,
                        G_TYPE_INT, zoom, G_TYPE_INVALID,
                        G_TYPE_INVALID)
                    || error)
            {
                g_printerr("Error: %s\n", error->message);
                return 4;
            }
        }
        else
        {
            /* Not specifying a zoom. */
            if(!dbus_g_proxy_call(proxy, MM_DBUS_METHOD_SET_VIEW_POSITION,
                        &error,
                        G_TYPE_DOUBLE, lat, G_TYPE_DOUBLE, lon, G_TYPE_INVALID,
                        G_TYPE_INVALID)
                    || error)
            {
                g_printerr("Error: %s\n", error->message);
                return -2;
            }
        }

        g_object_unref(proxy);
        dbus_g_connection_unref(bus);

        return 0;
    }
#endif

    maemo_mapper_init(argc, argv);

    gtk_main();

    maemo_mapper_destroy();

    osso_deinitialize(_osso);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    exit(0);
}

