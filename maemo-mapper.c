/*
 * This file is part of maemo-mapper
 *
 * Copyright (C) 2006 John Costigan
 *
 *
 * This software is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or(at your option)any later version.
 *
 * This software is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this software; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#define _GNU_SOURCE

#include <config.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stddef.h>
#include <locale.h>
#include <math.h>
#include <errno.h>
#include <sys/wait.h>
#include <gtk/gtk.h>
#include <fcntl.h>
#include <gdk/gdkkeysyms.h>
#include <libosso.h>
#include <hildon-widgets/hildon-program.h>
#include <hildon-widgets/hildon-controlbar.h>
#include <hildon-widgets/hildon-note.h>
#include <hildon-widgets/hildon-file-chooser-dialog.h>
#include <hildon-widgets/hildon-number-editor.h>
#include <hildon-widgets/hildon-banner.h>
#include <libgnomevfs/gnome-vfs.h>
#include <gconf/gconf-client.h>
#include <libxml/parser.h>

/* BELOW: for getting input from the GPS receiver. */
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/rfcomm.h>

/****************************************************************************
 * BELOW: DEFINES ***********************************************************
 ****************************************************************************/

#ifndef DEBUG
#define printf(...)
#endif

/* Set the below if to determine whether to get verbose output. */
#if 0
#define vprintf printf
#else
#define vprintf(...)
#endif

#define BOUND(x, a, b) { \
    if((x) < (a)) \
        (x) = (a); \
    else if((x) > (b)) \
        (x) = (b); \
}

#define PI   (3.14159265358979323846f)

/** MAX_ZOOM defines the largest map zoom level we will download.
 * (MAX_ZOOM - 1) is the largest map zoom level that the user can zoom to.
 */
#define MAX_ZOOM 17

#define TILE_SIZE_PIXELS (256)
#define TILE_SIZE_P2 (8)
#define TILE_PIXBUF_STRIDE (768)
#define BUF_WIDTH_TILES (4)
#define BUF_HEIGHT_TILES (3)
#define BUF_WIDTH_PIXELS (1024)
#define BUF_HEIGHT_PIXELS (768)

#define ARRAY_CHUNK_SIZE (1024)

#define WORLD_SIZE_UNITS (1 << 26)

#define tile2grid(tile) ((tile) << 3)
#define grid2tile(grid) ((grid) >> 3)
#define tile2pixel(tile) ((tile) << 8)
#define pixel2tile(pixel) ((pixel) >> 8)
#define tile2unit(tile) ((tile) << (8 + _zoom))
#define unit2tile(unit) ((unit) >> (8 + _zoom))
#define tile2zunit(tile, zoom) ((tile) << (8 + zoom))
#define unit2ztile(unit, zoom) ((unit) >> (8 + zoom))

#define grid2pixel(grid) ((grid) << 5)
#define pixel2grid(pixel) ((pixel) >> 5)
#define grid2unit(grid) ((grid) << (5 + _zoom))
#define unit2grid(unit) ((unit) >> (5 + _zoom))

#define pixel2unit(pixel) ((pixel) << _zoom)
#define unit2pixel(pixel) ((pixel) >> _zoom)

#define unit2bufx(unit) (unit2pixel(unit) - tile2pixel(_base_tilex))
#define bufx2unit(x) (pixel2unit(x) + tile2unit(_base_tilex))
#define unit2bufy(unit) (unit2pixel(unit) - tile2pixel(_base_tiley))
#define bufy2unit(y) (pixel2unit(y) + tile2unit(_base_tiley))

#define unit2x(unit) (unit2pixel(unit) - tile2pixel(_base_tilex) - _offsetx)
#define x2unit(x) (pixel2unit(x + _offsetx) + tile2unit(_base_tilex))
#define unit2y(unit) (unit2pixel(unit) - tile2pixel(_base_tiley) - _offsety)
#define y2unit(y) (pixel2unit(y + _offsety) + tile2unit(_base_tiley))

#define leadx2unit() (_pos.unitx + (_lead_ratio) * pixel2unit(_vel_offsetx))
#define leady2unit() (_pos.unity + (0.6f*_lead_ratio)*pixel2unit(_vel_offsety))

#define tile2punit(tile, zoom) ((((tile) << 1) + 1) << (7 + zoom))
#define unit2ptile(unit) ((unit) >> (7 + _zoom))

/* Pans are done two "grids" at a time, or 64 pixels. */
#define PAN_UNITS (grid2unit(2))

#define GCONF_KEY_PREFIX "/apps/maemo-mapper"
#define GCONF_KEY_RCVR_MAC GCONF_KEY_PREFIX"/receiver_mac"
#define GCONF_KEY_RCVR_CHAN GCONF_KEY_PREFIX"/receiver_channel"
#define GCONF_KEY_MAP_URI_FORMAT GCONF_KEY_PREFIX"/map_uri_format"
#define GCONF_KEY_MAP_ZOOM_STEPS GCONF_KEY_PREFIX"/map_zoom_steps"
#define GCONF_KEY_MAP_DIR_NAME GCONF_KEY_PREFIX"/map_cache_dir"
#define GCONF_KEY_AUTO_DOWNLOAD GCONF_KEY_PREFIX"/auto_download"
#define GCONF_KEY_CENTER_SENSITIVITY GCONF_KEY_PREFIX"/center_sensitivity"
#define GCONF_KEY_ANNOUNCE_NOTICE GCONF_KEY_PREFIX"/announce_notice"
#define GCONF_KEY_DRAW_LINE_WIDTH GCONF_KEY_PREFIX"/draw_line_width"
#define GCONF_KEY_ENABLE_VOICE GCONF_KEY_PREFIX"/enable_voice"
#define GCONF_KEY_VOICE_SYNTH_PATH GCONF_KEY_PREFIX"/voice_synth_path"
#define GCONF_KEY_ALWAYS_KEEP_ON \
                                   GCONF_KEY_PREFIX"/always_keep_on"
#define GCONF_KEY_AUTOCENTER_MODE GCONF_KEY_PREFIX"/autocenter_mode"
#define GCONF_KEY_LEAD_AMOUNT GCONF_KEY_PREFIX"/lead_amount"
#define GCONF_KEY_LAT GCONF_KEY_PREFIX"/last_latitude"
#define GCONF_KEY_LON GCONF_KEY_PREFIX"/last_longitude"
#define GCONF_KEY_CENTER_LAT GCONF_KEY_PREFIX"/center_latitude"
#define GCONF_KEY_CENTER_LON GCONF_KEY_PREFIX"/center_longitude"
#define GCONF_KEY_ZOOM GCONF_KEY_PREFIX"/zoom"
#define GCONF_KEY_ROUTEDIR GCONF_KEY_PREFIX"/route_directory"
#define GCONF_KEY_TRACKFILE GCONF_KEY_PREFIX"/track_file"
#define GCONF_KEY_SHOWTRACKS GCONF_KEY_PREFIX"/show_tracks"
#define GCONF_KEY_SHOWROUTES GCONF_KEY_PREFIX"/show_routes"
#define GCONF_KEY_SHOWVELVEC GCONF_KEY_PREFIX"/show_velocity_vector"
#define GCONF_KEY_ENABLE_GPS GCONF_KEY_PREFIX"/enable_gps"
#define GCONF_KEY_ROUTE_LOCATIONS GCONF_KEY_PREFIX"/route_locations"


#define MACRO_RECALC_CENTER(center_unitx, center_unity) { \
    switch(_center_mode) \
    { \
        case CENTER_LEAD: \
            center_unitx = leadx2unit(); \
            center_unity = leady2unit(); \
            break; \
        case CENTER_LATLON: \
            center_unitx = _pos.unitx; \
            center_unity = _pos.unity; \
            break; \
        default: \
             center_unitx = _center.unitx; \
             center_unity = _center.unity; \
            ; \
    } \
};

#define MERCATOR_SPAN (-6.28318377773622f)
#define MERCATOR_TOP (3.14159188886811f)
#define latlon2unit(lat, lon, unitx, unity) { \
    gfloat tmp; \
    unitx = (lon + 180.f) * (WORLD_SIZE_UNITS / 360.f) + 0.5f; \
    tmp = sinf(lat * (PI / 180.f)); \
    unity = 0.5f + (WORLD_SIZE_UNITS / MERCATOR_SPAN) \
        * (logf((1.f + tmp) / (1.f - tmp)) * 0.5f - MERCATOR_TOP); \
}

#define unit2latlon(unitx, unity, lat, lon) { \
    (lon) = ((unitx) * (360.f / WORLD_SIZE_UNITS)) - 180.f; \
    (lat) = (360.f * (atanf(expf(((unity) \
                                  * (MERCATOR_SPAN / WORLD_SIZE_UNITS)) \
                     + MERCATOR_TOP)))) * (1.f / PI) - 90.f; \
}

#define MACRO_RECALC_OFFSET() { \
    _offsetx = grid2pixel( \
            unit2grid(_center.unitx) \
            - _screen_grids_halfwidth \
            - tile2grid(_base_tilex)); \
    _offsety = grid2pixel( \
            unit2grid(_center.unity) \
            - _screen_grids_halfheight \
            - tile2grid(_base_tiley)); \
}

#define MACRO_RECALC_FOCUS_BASE() { \
    _focus.unitx = x2unit(_screen_width_pixels * _center_ratio / 20); \
    _focus.unity = y2unit(_screen_height_pixels * _center_ratio / 20); \
}

#define MACRO_RECALC_FOCUS_SIZE() { \
    _focus_unitwidth = pixel2unit( \
            (10 - _center_ratio) * _screen_width_pixels / 10); \
    _focus_unitheight = pixel2unit( \
            (10 - _center_ratio) * _screen_height_pixels / 10); \
}

#define MACRO_RECALC_CENTER_BOUNDS() { \
  _min_center.unitx = pixel2unit(grid2pixel(_screen_grids_halfwidth)); \
  _min_center.unity = pixel2unit(grid2pixel(_screen_grids_halfheight)); \
  _max_center.unitx = WORLD_SIZE_UNITS-grid2unit(_screen_grids_halfwidth) - 1; \
  _max_center.unity = WORLD_SIZE_UNITS-grid2unit(_screen_grids_halfheight)- 1; \
}

#define MACRO_INIT_TRACK(track) { \
    (track).head = (track).tail = g_new(TrackPoint, ARRAY_CHUNK_SIZE); \
    (track).tail->unitx = (track).tail->unity = 0;  \
    (track).cap = (track).head + ARRAY_CHUNK_SIZE; \
    (track).whead = g_new(WayPoint, ARRAY_CHUNK_SIZE); \
    (track).wtail = (track).whead - 1; \
    (track).wcap = (track).whead + ARRAY_CHUNK_SIZE; \
}

#define MACRO_CLEAR_TRACK(track) if((track).head) { \
    WayPoint *curr; \
    g_free((track).head); \
    (track).head = (track).tail = (track).cap = NULL; \
    for(curr = (track).whead - 1; curr++ != (track).wtail; ) \
        g_free(curr->desc); \
    g_free((track).whead); \
    (track).whead = (track).wtail = (track).wcap = NULL; \
}

#define DISTANCE_ROUGH(point) \
    (abs((gint)((point).unitx) - _pos.unitx) \
     + abs((gint)((point).unity) - _pos.unity))

#define MACRO_QUEUE_DRAW_AREA() \
    gtk_widget_queue_draw_area( \
            _map_widget, \
            0, 0, \
            _screen_width_pixels, \
            _screen_height_pixels)

#define KEEP_DISPLAY_ON() { \
    /* Note that the flag means keep on ONLY when fullscreen. */ \
    if(_always_keep_on || _fullscreen) \
    { \
        osso_display_state_on(_osso); \
        osso_display_blanking_pause(_osso); \
    } \
}

#define TRACKS_MASK 0x00000001
#define ROUTES_MASK 0x00000002

/****************************************************************************
 * ABOVE: DEFINES ***********************************************************
 ****************************************************************************/



/****************************************************************************
 * BELOW: TYPEDEFS **********************************************************
 ****************************************************************************/

/** This enumerated type defines the possible connection states. */
typedef enum
{
    /** The receiver is "off", meaning that either the bluetooth radio is
     * off or the user has requested not to connect to the GPS receiver.
     * No gtk_banner is visible. */
    RCVR_OFF,

    /** The connection with the receiver is down.  A gtk_banner is visible with
     * the text, "Connecting to GPS receiver". */
    RCVR_DOWN,

    /** The connection with the receiver is up, but a GPS fix is not available.
     * A gtk_banner is visible with the text, "(Re-)Establishing GPS fix". */
    RCVR_UP,

    /** The connection with the receiver is up and a GPS fix IS available.
     * No gtk_banner is visible. */
    RCVR_FIXED
} ConnState;

/** Possible center modes.  The "WAS" modes imply no current center mode;
 * they only hint at what the last center mode was, so that it can be
 * recalled. */
typedef enum
{
    CENTER_WAS_LATLON = -2,
    CENTER_WAS_LEAD = -1,
    CENTER_LEAD = 1,
    CENTER_LATLON = 2
} CenterMode;

/** This enum defines the states of the SAX parsing state machine. */
typedef enum
{
    START,
    INSIDE_GPX,
    INSIDE_TRACK,
    INSIDE_TRACK_SEGMENT,
    INSIDE_TRACK_POINT,
    INSIDE_TRACK_POINT_DESC,
    FINISH,
    UNKNOWN,
    ERROR,
} SaxState;

/** A general definition of a point in the Maemo Mapper unit system. */
typedef struct _TrackPoint TrackPoint;
struct _TrackPoint {
    guint unitx;
    guint unity;
};

/** A WayPoint, which is a TrackPoint with a description. */
typedef struct _WayPoint WayPoint;
struct _WayPoint {
    TrackPoint *point;
    gchar *desc;
};

/** A Track is a set of TrackPoints and WayPoints. */
typedef struct _Track Track;
struct _Track {
    TrackPoint *head; /* points to first element in array; NULL if empty. */
    TrackPoint *tail; /* points to last element in array. */
    TrackPoint *cap; /* points after last slot in array. */
    WayPoint *whead; /* points to first element in array; NULL if empty. */
    WayPoint *wtail; /* points to last element in array. */
    WayPoint *wcap; /* points after last slot in array. */
};

/** Data used during the SAX parsing operation. */
typedef struct _SaxData SaxData;
struct _SaxData {
    Track track;
    SaxState state;
    SaxState prev_state;
    guint unknown_depth;
    gboolean at_least_one_trkpt;
    GString *chars;
};

/** Data used during the asynchronous progress update phase of automatic map
 * downloading. */
typedef struct _ProgressUpdateInfo ProgressUpdateInfo;
struct _ProgressUpdateInfo
{
    GnomeVFSAsyncHandle *handle;
    GList *src_list;
    GList *dest_list;
    guint tilex, tiley, zoom; /* for refresh. */
    guint hash;
};

/** Data used during the asynchronous automatic route downloading operation. */
typedef struct _AutoRouteDownloadData AutoRouteDownloadData;
struct _AutoRouteDownloadData {
    gboolean enabled;
    gboolean in_progress;
    gchar *dest;
    GnomeVFSAsyncHandle *handle;
    gchar *bytes;
    guint bytes_read;
    guint bytes_maxsize;
};


/****************************************************************************
 * ABOVE: TYPEDEFS **********************************************************
 ****************************************************************************/



/****************************************************************************
 * BELOW: DATA **************************************************************
 ****************************************************************************/

/** The main GtkWindow of the application. */
static HildonProgram *_program = NULL;

/** The main GtkContainer of the application. */
static GtkWidget *_window = NULL;

/** The main OSSO context of the application. */
static osso_context_t *_osso = NULL;

/** The widget that provides the visual display of the map. */
static GtkWidget *_map_widget = NULL;

/** The backing pixmap of _map_widget. */
static GdkPixmap *_map_pixmap = NULL;


/** The file descriptor of our connection with the GPS receiver. */
static gint _fd = -1;

/** The GIOChannel through which communication with the GPS receiver is
 * performed. */
static GIOChannel *_channel = NULL;

/** The Source ID of the connect watch. */
static guint _connect_sid = 0;
/** The Source ID of the error watch. */
static guint _error_sid = 0;
/** The Source ID of the input watch. */
static guint _input_sid = 0;
/** The Source ID of the "Connect Later" idle. */
static guint _clater_sid = 0;


/** GPS data. */
static gfloat _pos_lat = 0.f;
static gfloat _pos_lon = 0.f;
static TrackPoint _pos = {0, 0};

static gfloat _speed = 0;
static gfloat _heading = 0;
static gint _vel_offsetx = 0;
static gint _vel_offsety = 0;


/** The current connection state. */
static ConnState _conn_state = RCVR_OFF;


/** VARIABLES FOR MAINTAINING STATE OF THE CURRENT VIEW. */

/** The "zoom" level defines the resolution of a pixel, from 0 to MAX_ZOOM.
 * Each pixel in the current view is exactly (1 << _zoom) "units" wide. */
static guint _zoom = 3; /* zoom level, from 0 to MAX_ZOOM. */
static TrackPoint _center = {-1, -1}; /* current center location, X. */
static TrackPoint _prev_center = {-1, -1}; /* previous center location, X */

/** The "base tile" is the upper-left tile in the pixmap. */
static guint _base_tilex = -5;
static guint _base_tiley = -5;

/** The "offset" defines the upper-left corner of the visible portion of the
 * 1024x768 pixmap. */
static guint _offsetx = -1;
static guint _offsety = -1;


/** CACHED SCREEN INFORMATION THAT IS DEPENDENT ON THE CURRENT VIEW. */
static guint _screen_grids_halfwidth = 0;
static guint _screen_grids_halfheight = 0;
static guint _screen_width_pixels = 0;
static guint _screen_height_pixels = 0;
static TrackPoint _focus = {-1, -1};
static guint _focus_unitwidth = 0;
static guint _focus_unitheight = 0;
static TrackPoint _min_center = {-1, -1};
static TrackPoint _max_center = {-1, -1};
static guint _world_size_tiles = -1;


/** VARIABLES FOR ACCESSING THE LOCATION/BOUNDS OF THE CURRENT MARK. */
static gint _mark_x1 = -1;
static gint _mark_x2 = -1;
static gint _mark_y1 = -1;
static gint _mark_y2 = -1;
static gint _mark_minx = -1;
static gint _mark_miny = -1;
static gint _mark_width = -1;
static gint _mark_height = -1;

/** The current track and route. */
static Track _track;
static Track _route;

/** Data for tracking waypoints for the purpose of announcement. */
/* _near_point is the route point to which we are closest. */
static TrackPoint *_near_point = NULL;
static guint _near_point_dist_rough = -1;
/* _next_way is what we currently interpret to be the next waypoint. */
static WayPoint *_next_way = NULL;
static guint _next_way_dist_rough = -1;
static gchar *_last_spoken_phrase = NULL;
/* _next_point is the route point immediately following _next_way. */
static TrackPoint *_next_point = NULL;
static guint _next_point_dist_rough = -1;


/** THE GtkGC OBJECTS USED FOR DRAWING. */
static GdkGC *_mark_current_gc = NULL;
static GdkGC *_mark_old_gc = NULL;
static GdkGC *_vel_current_gc = NULL;
static GdkGC *_vel_old_gc = NULL;
static GdkGC *_track_gc = NULL;
static GdkGC *_track_way_gc = NULL;
static GdkGC *_route_gc = NULL;
static GdkGC *_route_way_gc = NULL;
static GdkGC *_next_way_gc = NULL;

/** MENU ITEMS. */
static GtkWidget *_menu_route_download_item = NULL;
static GtkWidget *_menu_route_open_item = NULL;
static GtkWidget *_menu_route_save_item = NULL;
static GtkWidget *_menu_route_reset_item = NULL;
static GtkWidget *_menu_route_clear_item = NULL;
static GtkWidget *_menu_track_open_item = NULL;
static GtkWidget *_menu_track_save_item = NULL;
static GtkWidget *_menu_track_mark_way_item = NULL;
static GtkWidget *_menu_track_clear_item = NULL;
static GtkWidget *_menu_maps_dlroute_item = NULL;
static GtkWidget *_menu_maps_dlarea_item = NULL;
static GtkWidget *_menu_auto_download_item = NULL;
static GtkWidget *_menu_show_tracks_item = NULL;
static GtkWidget *_menu_show_routes_item = NULL;
static GtkWidget *_menu_show_velvec_item = NULL;
static GtkWidget *_menu_ac_latlon_item = NULL;
static GtkWidget *_menu_ac_lead_item = NULL;
static GtkWidget *_menu_ac_none_item = NULL;
static GtkWidget *_menu_fullscreen_item = NULL;
static GtkWidget *_menu_enable_gps_item = NULL;
static GtkWidget *_menu_settings_item = NULL;
static GtkWidget *_menu_close_item = NULL;

/** BANNERS. */
GtkWidget *_connect_banner = NULL;
GtkWidget *_fix_banner = NULL;
GtkWidget *_download_banner = NULL;

/** DOWNLOAD PROGRESS. */
static guint _num_downloads = 0;
static guint _curr_download = 0;
static GHashTable *_downloads_hash = NULL;

/** CONFIGURATION INFORMATION. */
static struct sockaddr_rc _rcvr_addr = { 0 };
static gchar *_rcvr_mac = NULL;
static gchar *_map_dir_name = NULL;
static gchar *_map_uri_format = NULL;
static gchar *_route_dir_uri = NULL;
static gchar *_track_file_uri = NULL;
static CenterMode _center_mode = CENTER_LEAD;
static gboolean _fullscreen = FALSE;
static gboolean _enable_gps = FALSE;
static gint _show_tracks = 0;
static gboolean _show_velvec = TRUE;
static gboolean _auto_download = FALSE;
static guint _zoom_steps = 2;
static guint _lead_ratio = 5;
static guint _center_ratio = 7;
static guint _draw_line_width = 5;
static guint _announce_notice_ratio = 6;
static gboolean _enable_voice = TRUE;
static gchar *_voice_synth_path = NULL;
static gboolean _always_keep_on = FALSE;
static GSList *_loc_list;
static GtkListStore *_loc_model;

/** The singleton auto-route-download data. */
static AutoRouteDownloadData _autoroute_data;

/****************************************************************************
 * ABOVE: DATA **************************************************************
 ****************************************************************************/



/****************************************************************************
 * BELOW: CALLBACK DECLARATIONS *********************************************
 ****************************************************************************/

static gint
dbus_cb_default(const gchar *interface, const gchar *method,
        GArray *arguments, gpointer data, osso_rpc_t *retval);
static void
osso_cb_hw_state(osso_hw_state_t *state, gpointer data);
static gboolean
window_cb_key_press(GtkWidget* widget, GdkEventKey *event);

static gboolean
map_cb_configure(GtkWidget *widget, GdkEventConfigure *event);
static gboolean
map_cb_expose(GtkWidget *widget, GdkEventExpose *event);
static gboolean
map_cb_button_release(GtkWidget *widget, GdkEventButton *event);

static gboolean
channel_cb_error(GIOChannel *src, GIOCondition condition, gpointer data);
static gboolean
channel_cb_connect(GIOChannel *src, GIOCondition condition, gpointer data);
static gboolean
channel_cb_input(GIOChannel *src, GIOCondition condition, gpointer data);

static gboolean
menu_cb_route_download(GtkAction *action);
static gboolean
menu_cb_route_open(GtkAction *action);
static gboolean
menu_cb_route_save(GtkAction *action);
static gboolean
menu_cb_route_reset(GtkAction *action);
static gboolean
menu_cb_route_clear(GtkAction *action);

static gboolean
menu_cb_track_open(GtkAction *action);
static gboolean
menu_cb_track_save(GtkAction *action);
static gboolean
menu_cb_track_mark_way(GtkAction *action);
static gboolean
menu_cb_track_clear(GtkAction *action);

static gboolean
menu_cb_show_tracks(GtkAction *action);
static gboolean
menu_cb_show_routes(GtkAction *action);
static gboolean
menu_cb_show_velvec(GtkAction *action);

static gboolean
menu_cb_maps_dlroute(GtkAction *action);
static gboolean
menu_cb_maps_dlarea(GtkAction *action);
static gboolean
menu_cb_auto_download(GtkAction *action);

static gboolean
menu_cb_ac_latlon(GtkAction *action);
static gboolean
menu_cb_ac_lead(GtkAction *action);
static gboolean
menu_cb_ac_none(GtkAction *action);

static gboolean
menu_cb_fullscreen(GtkAction *action);
static gboolean
menu_cb_enable_gps(GtkAction *action);

static gboolean
menu_cb_settings(GtkAction *action);

static gint
map_download_cb_async(GnomeVFSAsyncHandle *handle,
        GnomeVFSXferProgressInfo *info, ProgressUpdateInfo *pui);

static gboolean
auto_route_dl_idle_refresh();
static gboolean
auto_route_dl_idle();

/****************************************************************************
 * ABOVE: CALLBACK DECLARATIONS *********************************************
 ****************************************************************************/



/****************************************************************************
 * BELOW: ROUTINES **********************************************************
 ****************************************************************************/

/**
 * Pop up a modal dialog box with simple error information in it.
 */
static void
popup_error(const gchar *error)
{
    GtkWidget *dialog;
    printf("%s(\"%s\")\n", __PRETTY_FUNCTION__, error);

    dialog = hildon_note_new_information(GTK_WINDOW(_window), error);

    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

/**
 * Set the connection state.  This function controls all connection-related
 * banners.
 */
static void
set_conn_state(ConnState new_conn_state)
{
    printf("%s(%d)\n", __PRETTY_FUNCTION__, new_conn_state);

    switch(_conn_state = new_conn_state)
    {
        case RCVR_OFF:
        case RCVR_FIXED:
            if(_connect_banner)
            {
                gtk_widget_destroy(_connect_banner);
                _connect_banner = NULL;
            }
            if(_fix_banner)
            {
                gtk_widget_destroy(_fix_banner);
                _fix_banner = NULL;
            }
            break;
        case RCVR_DOWN:
            if(_fix_banner)
            {
                gtk_widget_destroy(_fix_banner);
                _fix_banner = NULL;
            }
            if(!_connect_banner)
                _connect_banner = hildon_banner_show_animation(
                        _window, NULL, "Searching for GPS receiver");
            break;
        case RCVR_UP:
            if(_connect_banner)
            {
                gtk_widget_destroy(_connect_banner);
                _connect_banner = NULL;
            }
            if(!_fix_banner)
                _fix_banner = hildon_banner_show_progress(
                        _window, NULL, "Establishing GPS fix");
            break;
        default: ; /* to quell warning. */
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

/**
 * Updates _near_point, _next_way, and _next_point.  If quick is FALSE (as
 * it is when this function is called from route_find_nearest_point), then
 * the entire list (starting from _near_point) is searched.  Otherwise, we
 * stop searching when we find a point that is farther away.
 */
static gboolean
route_update_nears(gboolean quick)
{
    gboolean ret = FALSE;
    TrackPoint *curr, *near;
    WayPoint *wcurr, *wnext;
    guint near_dist_rough;
    printf("%s(%d)\n", __PRETTY_FUNCTION__, quick);
    
    /* If we have waypoints (_next_way != NULL), then determine the "next
     * waypoint", which is defined as the waypoint after the nearest point,
     * UNLESS we've passed that waypoint, in which case the waypoint after
     * that waypoint becomes the "next" waypoint. */
    if(_next_way)
    {
        /* First, set near_dist_rough with the new distance from _near_point. */
        near = _near_point;
        near_dist_rough = DISTANCE_ROUGH(*near);

        /* Now, search _route for a closer point.  If quick is TRUE, then we'll
         * only search forward, only as long as we keep finding closer points.
         */
        for(curr = _near_point; curr++ != _route.tail; )
            if(curr->unity)
            {
                guint dist_rough = DISTANCE_ROUGH(*curr);
                if(dist_rough <= near_dist_rough)
                {
                    near = curr;
                    near_dist_rough = dist_rough;
                }
                else if(quick)
                    break;
            }

        /* Update _near_point. */
        _near_point = near;
        _near_point_dist_rough = near_dist_rough;

        for(wnext = wcurr = _next_way; wcurr != _route.wtail; wcurr++)
        {
            if(wcurr->point < near
            /* Okay, this else if expression warrants explanation.  If the
             * nearest track point happens to be a waypoint, then we want to
             * check if we have "passed" that waypoint.  To check this, we
             * test the distance from _pos to the waypoint and from _pos to
             * _next_point, and if the former is increasing and the latter is
             * decreasing, then we have passed the waypoint, and thus we
             * should skip it.  Note that if there is no _next_point, then
             * there is no next waypoint, so we do not skip it in that case. */
                || (wcurr->point == near && quick
                    && (_next_point
                     && (DISTANCE_ROUGH(*near) > _next_way_dist_rough
                      && DISTANCE_ROUGH(*_next_point)<_next_point_dist_rough))))
                wnext = wcurr + 1;
            else
                break;
        }

        if(wnext == _route.wtail && (wnext->point < near
                || (wnext->point == near && quick
                    && (!_next_point
                     || (DISTANCE_ROUGH(*near) > _next_way_dist_rough
                      &&DISTANCE_ROUGH(*_next_point)<_next_point_dist_rough)))))
        {
            printf("Setting _next_way to NULL\n");
            _next_way = NULL;
            _next_point = NULL;
            _next_way_dist_rough = -1;
            _next_point_dist_rough = -1;
            ret = TRUE;
        }
        /* Only update _next_way (and consequently _next_point) if _next_way is
         * different, and record that fact for return. */
        else
        {
            if(!quick || _next_way != wnext)
            {
                _next_way = wnext;
                _next_point = wnext->point;
                if(_next_point == _route.tail)
                    _next_point = NULL;
                else
                {
                    while((++_next_point)->unity == 0)
                    {
                        if(_next_point == _route.tail)
                        {
                            _next_point = NULL;
                            break;
                        }
                    }
                }
                ret = TRUE;
            }
            _next_way_dist_rough = DISTANCE_ROUGH(*wnext->point);
            if(_next_point)
                _next_point_dist_rough = DISTANCE_ROUGH(*_next_point);
        }
    }

    vprintf("%s(): return %d\n", __PRETTY_FUNCTION__, ret);
    return ret;
}

/**
 * Reset the _near_point data by searching the entire route for the nearest
 * route point and waypoint.
 */
static void
route_find_nearest_point()
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    /* Initialize _near_point. */
    _near_point = _route.head;
    while(_near_point->unity == 0 && _near_point != _route.tail)
        _near_point++;

    /* Initialize _next_way. */
    if(_route.wtail == _route.whead - 1
            || (_autoroute_data.enabled && _route.wtail == _route.whead))
        _next_way = NULL;
    else
        /* We have at least one waypoint. */
        _next_way = (_autoroute_data.enabled ? _route.whead + 1 : _route.whead);
    _next_way_dist_rough = -1;

    /* Initialize _next_point. */
    _next_point = NULL;
    _next_point_dist_rough = -1;

    route_update_nears(FALSE);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

/**
 * Render a single track line to _map_pixmap.  If either point on the line
 * is a break (defined as unity == 0), a circle is drawn at the other point.
 * IT IS AN ERROR FOR BOTH POINTS TO INDICATE A BREAK.
 */
static void
map_render_track_line(GdkGC *gc_norm, GdkGC *gc_way,
        guint unitx1, guint unity1, guint unitx2, guint unity2)
{
    vprintf("%s()\n", __PRETTY_FUNCTION__);

    if(!unity1)
    {
        guint x2, y2;
        x2 = unit2bufx(unitx2);
        y2 = unit2bufy(unity2);
        /* Make sure this circle will be visible. */
        if((x2 < BUF_WIDTH_PIXELS)
                && (y2 < BUF_HEIGHT_PIXELS))
            gdk_draw_arc(_map_pixmap, gc_way,
                    FALSE, /* FALSE: not filled. */
                    x2 - _draw_line_width,
                    y2 - _draw_line_width,
                    2 * _draw_line_width,
                    2 * _draw_line_width,
                    0, /* start at 0 degrees. */
                    360 * 64);
    }
    else if(!unity2)
    {
        guint x1, y1;
        x1 = unit2bufx(unitx1);
        y1 = unit2bufy(unity1);
        /* Make sure this circle will be visible. */
        if((x1 < BUF_WIDTH_PIXELS)
                && ((unsigned)y1 < BUF_HEIGHT_PIXELS))
            gdk_draw_arc(_map_pixmap, gc_way,
                    FALSE, /* FALSE: not filled. */
                    x1 - _draw_line_width,
                    y1 - _draw_line_width,
                    2 * _draw_line_width,
                    2 * _draw_line_width,
                    0, /* start at 0 degrees. */
                    360 * 64);
    }
    else
    {
        gint x1, y1, x2, y2;
        x1 = unit2bufx(unitx1);
        y1 = unit2bufy(unity1);
        x2 = unit2bufx(unitx2);
        y2 = unit2bufy(unity2);
        /* Make sure this line could possibly be visible. */
        if(!((x1 > BUF_WIDTH_PIXELS && x2 > BUF_WIDTH_PIXELS)
                || (x1 < 0 && x2 < 0)
                || (y1 > BUF_HEIGHT_PIXELS && y2 > BUF_HEIGHT_PIXELS)
                || (y1 < 0 && y2 < 0)))
            gdk_draw_line(_map_pixmap, gc_norm, x1, y1, x2, y2);
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

/**
 * Render all track data onto the _map_pixmap.  Note that this does not
 * clear the pixmap of previous track data (use map_force_redraw() for
 * that), and also note that this method does not queue any redraws, so it
 * is up to the caller to decide which part of the track really needs to be
 * redrawn.
 */
static void
map_render_track(Track *track, GdkGC *line_gc, GdkGC *way_gc)
{
    TrackPoint *curr;
    WayPoint *wcurr;
    printf("%s()\n", __PRETTY_FUNCTION__);

    for(curr = track->head, wcurr = track->whead; curr != track->tail; curr++)
    {
        if(wcurr <= track->wtail && curr == wcurr->point)
        {
            guint x1 = unit2bufx(wcurr->point->unitx);
            guint y1 = unit2bufy(wcurr->point->unity);
            if((x1 < BUF_WIDTH_PIXELS)
                    && (y1 < BUF_HEIGHT_PIXELS))
            {
                gdk_draw_arc(_map_pixmap,
                        (wcurr == _next_way ? _next_way_gc : way_gc),
                        FALSE, /* FALSE: not filled. */
                        x1 - _draw_line_width,
                        y1 - _draw_line_width,
                        2 * _draw_line_width,
                        2 * _draw_line_width,
                        0, /* start at 0 degrees. */
                        360 * 64);
            }
            wcurr++;
            if(curr[1].unity == 0)
                continue;
        }
        map_render_track_line(line_gc, way_gc,
                curr->unitx, curr->unity, curr[1].unitx, curr[1].unity);
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}
static void
map_render_tracks()
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if((_show_tracks & ROUTES_MASK) && _route.head)
    {
        map_render_track(&_route, _route_gc, _route_way_gc);
        if(_next_way == NULL)
        {
            guint x1 = unit2bufx(_route.tail[-1].unitx);
            guint y1 = unit2bufy(_route.tail[-1].unity);
            if((x1 < BUF_WIDTH_PIXELS)
                    && (y1 < BUF_HEIGHT_PIXELS))
            {
                gdk_draw_arc(_map_pixmap,
                        _next_way_gc,
                        FALSE, /* FALSE: not filled. */
                        x1 - _draw_line_width,
                        y1 - _draw_line_width,
                        2 * _draw_line_width,
                        2 * _draw_line_width,
                        0, /* start at 0 degrees. */
                        360 * 64);
            }
        }
    }
    if(_show_tracks & TRACKS_MASK)
        map_render_track(&_track, _track_gc, _track_way_gc);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

static void
track_resize(Track *track, guint size)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(track->head + size != track->cap)
    {
        TrackPoint *old_head = track->head;
        WayPoint *curr;
        track->head = g_renew(TrackPoint, old_head, size);
        track->cap = track->head + size;
        if(track->head != old_head)
        {
            track->tail = track->head + (track->tail - old_head);

            /* Adjust all of the waypoints. */
            for(curr = track->whead - 1; curr++ != track->wtail; )
                curr->point = track->head + (curr->point - old_head);
        }
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

static void
track_wresize(Track *track, guint wsize)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(track->whead + wsize != track->wcap)
    {
        WayPoint *old_whead = track->whead;
        track->whead = g_renew(WayPoint, old_whead, wsize);
        track->wtail = track->whead + (track->wtail - old_whead);
        track->wcap = track->whead + wsize;
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

#define MACRO_TRACK_INCREMENT_TAIL(track) { \
    if(++(track).tail == (track).cap) \
        track_resize(&(track), (track).cap - (track).head + ARRAY_CHUNK_SIZE); \
}

#define MACRO_TRACK_INCREMENT_WTAIL(track) { \
    if(++(track).wtail == (track).wcap) \
        track_wresize(&(track), \
                (track).wcap - (track).whead + ARRAY_CHUNK_SIZE); \
}

/**
 * Add a point to the _track list.  This function is slightly overloaded,
 * since it is what houses the check for "have we moved
 * significantly": it also initiates the re-calculation of the _near_point
 * data, as well as calling osso_display_state_on() when we have the focus.
 */
static void
track_add(guint unitx, guint unity, gboolean newly_fixed)
{
    printf("%s(%u, %u)\n", __PRETTY_FUNCTION__, unitx, unity);

    if(abs((gint)unitx - _track.tail->unitx) > _draw_line_width
            || abs((gint)unity - _track.tail->unity) > _draw_line_width)
    {
        if(unity != 0 && _route.head
                && (newly_fixed ? (route_find_nearest_point(), TRUE)
                                : route_update_nears(TRUE)))
        {
            map_render_tracks();
            MACRO_QUEUE_DRAW_AREA();
        }
        if(_show_tracks & TRACKS_MASK)
        {
            /* Instead of calling map_render_tracks(), we'll draw the new line
             * ourselves and call gtk_widget_queue_draw_area(). */
            gint tx1, ty1, tx2, ty2;
            map_render_track_line(_track_gc, _track_way_gc,
                    _track.tail->unitx, _track.tail->unity, unitx, unity);
            if(unity != 0 && _track.tail->unity != 0)
            {
                tx1 = unit2x(_track.tail->unitx);
                ty1 = unit2y(_track.tail->unity);
                tx2 = unit2x(unitx);
                ty2 = unit2y(unity);
                gtk_widget_queue_draw_area(_map_widget,
                        MIN(tx1, tx2) - _draw_line_width,
                        MIN(ty1, ty2) - _draw_line_width,
                        abs(tx1 - tx2) + (2 * _draw_line_width),
                        abs(ty1 - ty2) + (2 * _draw_line_width));
            }
        }
        MACRO_TRACK_INCREMENT_TAIL(_track);

        _track.tail->unitx = unitx;
        _track.tail->unity = unity;

        if(_autoroute_data.enabled && !_autoroute_data.in_progress
                && _near_point_dist_rough > 400)
        {
            _autoroute_data.in_progress = TRUE;
            g_idle_add((GSourceFunc)auto_route_dl_idle, NULL);
        }

        /* Keep the display on. */
        KEEP_DISPLAY_ON();
    }

    /* Check if we should announce upcoming waypoints. */
    if(unity && _next_way_dist_rough
            < (10 + (guint)_speed) * _announce_notice_ratio * 5)
    {
        if(_enable_voice && strcmp(_next_way->desc, _last_spoken_phrase))
        {
            gchar *buffer;
            g_free(_last_spoken_phrase);
            _last_spoken_phrase = g_strdup(_next_way->desc);
            if(!fork())
            {
                /* We are the fork child.  Synthesize the voice. */
                buffer = g_strdup_printf(
                        "Approaching Waypoint. %s",
                        _last_spoken_phrase);
                printf("%s %s\n", _voice_synth_path, buffer);
                execl(_voice_synth_path, _voice_synth_path,
                        "-t", buffer, (char *)NULL);
                printf("DONE!\n");
                g_free(buffer);
                exit(0);
            }
        }
        hildon_banner_show_information(_window, NULL, _next_way->desc);
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

/**
 * Disconnect from the receiver.  This method cleans up any and everything
 * that might be associated with the receiver.
 */
static void
rcvr_disconnect()
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    /* Remove watches. */
    if(_clater_sid)
    {
        g_source_remove(_clater_sid);
        _clater_sid = 0;
    }
    if(_error_sid)
    {
        g_source_remove(_error_sid);
        _error_sid = 0;
    }
    if(_connect_sid)
    {
        g_source_remove(_connect_sid);
        _connect_sid = 0;
    }
    if(_input_sid)
    {
        g_source_remove(_input_sid);
        _input_sid = 0;
    }

    /* Destroy the GIOChannel object. */
    if(_channel)
    {
        g_io_channel_shutdown(_channel, FALSE, NULL);
        g_io_channel_unref(_channel);
        _channel = NULL;
    }

    /* Close the file descriptor. */
    if(_fd != -1)
    {
        close(_fd);
        _fd = -1;
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

static void rcvr_connect_later(); /* Forward declaration. */

/**
 * Connect to the receiver.
 * This method assumes that _fd is -1 and _channel is NULL.  If unsure, call
 * rcvr_disconnect() first.
 * Since this is an idle function, this function returns whether or not it
 * should be called again, which is always FALSE.
 */
static gboolean
rcvr_connect_now()
{
    printf("%s(%d)\n", __PRETTY_FUNCTION__, _conn_state);

    if(_conn_state == RCVR_DOWN && _rcvr_mac) {
#ifndef DEBUG
        /* Create the file descriptor. */
        if(*_rcvr_mac == '/')
            _fd = open(_rcvr_mac, O_RDONLY);
        else
            _fd = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);

        /* If file descriptor creation failed, try again later.  Note that
         * there is no need to call rcvr_disconnect() because the file
         * descriptor creation is the first step, so if it fails, there's
         * nothing to clean up. */
        if(_fd == -1)
            rcvr_connect_later();
        else
        {
            _channel = g_io_channel_unix_new(_fd);
            g_io_channel_set_flags(_channel, G_IO_FLAG_NONBLOCK, NULL);
            _error_sid = g_io_add_watch_full(_channel, G_PRIORITY_HIGH_IDLE,
                    G_IO_ERR | G_IO_HUP, channel_cb_error, NULL, NULL);
            _connect_sid = g_io_add_watch_full(_channel, G_PRIORITY_HIGH_IDLE,
                    G_IO_OUT, channel_cb_connect, NULL, NULL);
            if(*_rcvr_mac != '/'
                    && connect(_fd, (struct sockaddr*)&_rcvr_addr,
                        sizeof(_rcvr_addr))
                    && errno != EAGAIN)
            {
                /* Connection failed.  Disconnect and try again later. */
                rcvr_disconnect();
                rcvr_connect_later();
            }
        }
#else
        /* We're in DEBUG mode, so instead of connecting, skip to FIXED. */
        set_conn_state(RCVR_FIXED);
#endif
    }

    _clater_sid = 0;

    vprintf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
    return FALSE;
}

/**
 * Place a request to connect about 1 second after the function is called.
 */
static void
rcvr_connect_later()
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    _clater_sid = g_timeout_add(1000, (GSourceFunc)rcvr_connect_now, NULL);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

/**
 * Convert the float lat/lon/speed/heading data into integer units.
 */
static void
integerize_data()
{
    gfloat tmp;
    printf("%s()\n", __PRETTY_FUNCTION__);

    latlon2unit(_pos_lat, _pos_lon, _pos.unitx, _pos.unity);

    tmp = (_heading * (1.f / 180.f)) * PI;
    _vel_offsetx = (gint)(floorf(_speed * sinf(tmp) + 0.5f));
    _vel_offsety = -(gint)(floorf(_speed * cosf(tmp) + 0.5f));

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

/**
 * Update all GtkGC objects to reflect the current _draw_line_width.
 */
#define UPDATE_GC(gc) \
    gdk_gc_set_line_attributes(gc, \
            _draw_line_width, GDK_LINE_SOLID, GDK_CAP_ROUND, GDK_JOIN_ROUND);
static void
update_gcs()
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    UPDATE_GC(_mark_current_gc);
    UPDATE_GC(_mark_old_gc);
    UPDATE_GC(_vel_current_gc);
    UPDATE_GC(_vel_old_gc);
    UPDATE_GC(_track_gc);
    UPDATE_GC(_track_way_gc);
    UPDATE_GC(_route_gc);
    UPDATE_GC(_route_way_gc);
    UPDATE_GC(_next_way_gc);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

/**
 * Change the map cache directory and update dependent variables.
 */
static gboolean
config_set_map_dir_name(gchar *new_map_dir_name)
{
    GnomeVFSURI *map_dir_uri;
    gboolean retval = FALSE;
    printf("%s()\n", __PRETTY_FUNCTION__);

    /* Rest of the function devoted to making sure the directory exists. */
    map_dir_uri = gnome_vfs_uri_new(new_map_dir_name);
    if(!gnome_vfs_uri_exists(map_dir_uri))
    {
        GnomeVFSURI *parent, *curr_uri;
        GList *list = NULL;

        list = g_list_prepend(list, curr_uri = map_dir_uri);
        while(GNOME_VFS_ERROR_NOT_FOUND == gnome_vfs_make_directory_for_uri(
                        parent = gnome_vfs_uri_get_parent(curr_uri), 0755))
            list = g_list_prepend(list, curr_uri = parent);

        while(list != NULL)
        {
            retval = (GNOME_VFS_OK == gnome_vfs_make_directory_for_uri(
                        (GnomeVFSURI*)list->data, 0755));
            gnome_vfs_uri_unref((GnomeVFSURI*)list->data);
            list = g_list_remove(list, list->data);
        }
        /* Retval now equals result of last make-dir attempt. */
    }
    else
        retval = TRUE;

    if(retval)
    {
        if(_map_dir_name)
            g_free(_map_dir_name);
        _map_dir_name = new_map_dir_name;
    }

    vprintf("%s(): return %d\n", __PRETTY_FUNCTION__, retval);
    return retval;
}

/**
 * Save all configuration data to GCONF.
 */
static void
config_save()
{
    GConfClient *gconf_client = gconf_client_get_default();
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(!gconf_client)
    {
        fprintf(stderr, "Failed to initialize GConf.  Aborting.\n");
        return;
    }

    /* Save Receiver MAC from GConf. */
    if(_rcvr_mac)
        gconf_client_set_string(gconf_client,
                GCONF_KEY_RCVR_MAC, _rcvr_mac, NULL);
    else
        gconf_client_unset(gconf_client,
                GCONF_KEY_RCVR_MAC, NULL);

    /* Save Receiver Channel to GConf. */
    gconf_client_set_int(gconf_client,
            GCONF_KEY_RCVR_CHAN, _rcvr_addr.rc_channel, NULL);

    /* Save Map Download URI Format. */
    if(_map_uri_format)
        gconf_client_set_string(gconf_client,
                GCONF_KEY_MAP_URI_FORMAT, _map_uri_format, NULL);

    /* Save Map Download Zoom Steps. */
    gconf_client_set_int(gconf_client,
            GCONF_KEY_MAP_ZOOM_STEPS, _zoom_steps, NULL);

    /* Save Map Cache Directory. */
    if(_map_dir_name)
        gconf_client_set_string(gconf_client,
            GCONF_KEY_MAP_DIR_NAME, _map_dir_name, NULL);

    /* Save Auto-Download. */
    gconf_client_set_bool(gconf_client,
            GCONF_KEY_AUTO_DOWNLOAD, _auto_download, NULL);

    /* Save Auto-Center Sensitivity. */
    gconf_client_set_int(gconf_client,
            GCONF_KEY_CENTER_SENSITIVITY, _center_ratio, NULL);

    /* Save Auto-Center Lead Amount. */
    gconf_client_set_int(gconf_client,
            GCONF_KEY_LEAD_AMOUNT, _lead_ratio, NULL);

    /* Save Draw Line Width. */
    gconf_client_set_int(gconf_client,
            GCONF_KEY_DRAW_LINE_WIDTH, _draw_line_width, NULL);

    /* Save Announce Advance Notice Ratio. */
    gconf_client_set_int(gconf_client,
            GCONF_KEY_ANNOUNCE_NOTICE, _announce_notice_ratio, NULL);

    /* Save Enable Voice flag. */
    gconf_client_set_bool(gconf_client,
            GCONF_KEY_ENABLE_VOICE, _enable_voice, NULL);

    /* Save Voice Synthesis Path flag. */
    gconf_client_set_string(gconf_client,
            GCONF_KEY_VOICE_SYNTH_PATH, _voice_synth_path, NULL);

    /* Save Keep On When Fullscreen flag. */
    gconf_client_set_bool(gconf_client,
            GCONF_KEY_ALWAYS_KEEP_ON, _always_keep_on, NULL);

    /* Save last saved latitude. */
    gconf_client_set_float(gconf_client,
            GCONF_KEY_LAT, _pos_lat, NULL);

    /* Save last saved longitude. */
    gconf_client_set_float(gconf_client,
            GCONF_KEY_LON, _pos_lon, NULL);

    /* Save last center point. */
    {
        gfloat center_lat, center_lon;
        unit2latlon(_center.unitx, _center.unity, center_lat, center_lon);

        /* Save last center latitude. */
        gconf_client_set_float(gconf_client,
                GCONF_KEY_CENTER_LAT, center_lat, NULL);

        /* Save last center longitude. */
        gconf_client_set_float(gconf_client,
                GCONF_KEY_CENTER_LON, center_lon, NULL);
    }

    /* Save last Zoom Level. */
    gconf_client_set_int(gconf_client,
            GCONF_KEY_ZOOM, _zoom, NULL);

    /* Save Route Directory. */
    if(_route_dir_uri)
        gconf_client_set_string(gconf_client,
                GCONF_KEY_ROUTEDIR, _route_dir_uri, NULL);

    /* Save Last Track File. */
    if(_track_file_uri)
        gconf_client_set_string(gconf_client,
                GCONF_KEY_TRACKFILE, _track_file_uri, NULL);

    /* Save Auto-Center Mode. */
    gconf_client_set_int(gconf_client,
            GCONF_KEY_AUTOCENTER_MODE, _center_mode, NULL);

    /* Save Show Tracks flag. */
    gconf_client_set_bool(gconf_client,
            GCONF_KEY_SHOWTRACKS, _show_tracks & TRACKS_MASK, NULL);

    /* Save Show Routes flag. */
    gconf_client_set_bool(gconf_client,
            GCONF_KEY_SHOWROUTES, _show_tracks & ROUTES_MASK, NULL);

    /* Save Show Velocity Vector flag. */
    gconf_client_set_bool(gconf_client,
            GCONF_KEY_SHOWVELVEC, _show_velvec, NULL);

    /* Save Enable GPS flag. */
    gconf_client_set_bool(gconf_client,
            GCONF_KEY_ENABLE_GPS, _enable_gps, NULL);

    /* Save Route Locations. */
    gconf_client_set_list(gconf_client,
            GCONF_KEY_ROUTE_LOCATIONS, GCONF_VALUE_STRING, _loc_list, NULL);

    g_object_unref(gconf_client);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

static void
force_min_visible_bars(HildonControlbar *control_bar, gint num_bars)
{
    GValue val;
    memset(&val, 0, sizeof(val));
    g_value_init(&val, G_TYPE_INT);
    g_value_set_int(&val, num_bars);
    g_object_set_property(G_OBJECT(control_bar), "minimum-visible-bars", &val);
}

typedef struct _ScanInfo ScanInfo;
struct _ScanInfo {
    GtkWidget *txt_rcvr_mac;
    GtkWidget *scan_banner;
};

/**
 * Scan for all bluetooth devices.  This method can take a few seconds,
 * during which the UI will freeze.
 */
static gboolean
scan_bluetooth(ScanInfo *scan_info)
{
    /* Do an hci_inquiry for our boy. */
    gchar buffer[18];
    inquiry_info ii;
    inquiry_info *pii = &ii;
    gint devid = hci_get_route(NULL);
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(hci_inquiry(devid, 4, 1, NULL, &pii, IREQ_CACHE_FLUSH) > 0)
    {
        ba2str(&ii.bdaddr, buffer);
        gtk_widget_destroy(scan_info->scan_banner);
        scan_info->scan_banner = NULL;

        gtk_entry_set_text(GTK_ENTRY(scan_info->txt_rcvr_mac), buffer);

        vprintf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
        return FALSE;
    }

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

/**
 * Bring up the Settings dialog.  Return TRUE if and only if the recever
 * information has changed (MAC or channel).
 */
static gboolean
settings_dialog()
{
    GtkWidget *dialog;
    GtkWidget *notebook;
    GtkWidget *table;
    GtkWidget *label;
    GtkWidget *txt_rcvr_mac;
    GtkWidget *num_rcvr_chan;
    GtkWidget *txt_map_uri_format;
    GtkWidget *txt_map_dir_name;
    GtkWidget *num_zoom_steps;
    GtkWidget *num_center_ratio;
    GtkWidget *num_lead_ratio;
    GtkWidget *num_announce_notice;
    GtkWidget *chk_enable_voice;
    GtkWidget *txt_voice_synth_path;
    GtkWidget *num_draw_line_width;
    GtkWidget *chk_always_keep_on;
    ScanInfo scan_info = {0, 0};
    gboolean rcvr_changed = FALSE;
    gint scan_sid = 0;
    printf("%s()\n", __PRETTY_FUNCTION__);

    dialog = gtk_dialog_new_with_buttons("Maemo Mapper Settings",
            GTK_WINDOW(_window), GTK_DIALOG_MODAL,
            GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
            GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
            NULL);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
            notebook = gtk_notebook_new(), TRUE, TRUE, 0);
    
    /* Receiver page. */
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
            table = gtk_table_new(2, 3, FALSE),
            label = gtk_label_new("GPS"));

    /* Receiver MAC Address. */
    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new("MAC"),
            0, 1, 0, 1, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(table),
            txt_rcvr_mac = gtk_entry_new_with_max_length(17),
            1, 2, 0, 1, GTK_EXPAND | GTK_FILL, 0, 2, 4);

    /* Receiver Channel. */
    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new("Channel"),
            0, 1, 1, 2, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(table),
            num_rcvr_chan = hildon_number_editor_new(0, 255),
            1, 2, 1, 2, GTK_EXPAND | GTK_FILL, 0, 2, 4);

    /* Note!. */
    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new(
                "Note: \"Channel\" refers to the device side!"),
            0, 2, 2, 3, GTK_FILL, 0, 2, 4);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
    gtk_misc_set_alignment(GTK_MISC(label), 0.5f, 0.5f);


    /* Maps page. */
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
            table = gtk_table_new(2, 3, FALSE),
            label = gtk_label_new("Maps"));

    /* Map download URI. */
    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new("URI Prefix"),
            0, 1, 0, 1, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(table),
            txt_map_uri_format = gtk_entry_new(),
            1, 2, 0, 1, GTK_EXPAND | GTK_FILL, 0, 2, 4);
    gtk_entry_set_width_chars(GTK_ENTRY(txt_map_uri_format), 30);

    /* Zoom Steps. */
    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new("Zoom Steps"),
            0, 1, 1, 2, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(table),
            num_zoom_steps = hildon_controlbar_new(),
            1, 2, 1, 2, GTK_EXPAND | GTK_FILL, 0, 2, 4);
    hildon_controlbar_set_range(HILDON_CONTROLBAR(num_zoom_steps), 1, 4);
    force_min_visible_bars(HILDON_CONTROLBAR(num_zoom_steps), 1);

    /* Map Directory. */
    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new("Cache Dir."),
            0, 1, 2, 3, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(table),
            txt_map_dir_name = gtk_entry_new(),
            1, 2, 2, 3, GTK_EXPAND | GTK_FILL, 0, 2, 4);
    gtk_entry_set_width_chars(GTK_ENTRY(txt_map_dir_name), 30);


    /* Auto-Center page. */
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
            table = gtk_table_new(2, 2, FALSE),
            label = gtk_label_new("Auto-Center"));

    /* Auto-Center Sensitivity. */
    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new("Sensitivity"),
            0, 1, 0, 1, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(table),
            num_center_ratio = hildon_controlbar_new(),
            1, 2, 0, 1, GTK_EXPAND | GTK_FILL, 0, 2, 4);
    hildon_controlbar_set_range(HILDON_CONTROLBAR(num_center_ratio), 1, 10);
    force_min_visible_bars(HILDON_CONTROLBAR(num_center_ratio), 1);

    /* Lead Amount. */
    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new("Lead Amount"),
            0, 1, 1, 2, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(table),
            num_lead_ratio = hildon_controlbar_new(),
            1, 2, 1, 2, GTK_EXPAND | GTK_FILL, 0, 2, 4);
    hildon_controlbar_set_range(HILDON_CONTROLBAR(num_lead_ratio), 1, 10);
    force_min_visible_bars(HILDON_CONTROLBAR(num_lead_ratio), 1);

    /* Announcement. */
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
            table = gtk_table_new(2, 3, FALSE),
            label = gtk_label_new("Announce"));

    /* Announcement Advance Notice. */
    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new("Advance Notice"),
            0, 1, 0, 1, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(table),
            num_announce_notice = hildon_controlbar_new(),
            1, 2, 0, 1, GTK_EXPAND | GTK_FILL, 0, 2, 4);
    hildon_controlbar_set_range(HILDON_CONTROLBAR(num_announce_notice), 1, 20);
    force_min_visible_bars(HILDON_CONTROLBAR(num_announce_notice), 1);

    /* Enable Voice. */
    gtk_table_attach(GTK_TABLE(table),
            chk_enable_voice = gtk_check_button_new_with_label(
                "Enable Voice Synthesis (requires flite)"),
            0, 2, 1, 2, GTK_EXPAND | GTK_FILL, 0, 2, 4);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chk_enable_voice),
            _enable_voice);

    /* Voice Synthesis Path. */
    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new("Voice Synth Path"),
            0, 1, 2, 3, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(table),
            txt_voice_synth_path = gtk_entry_new(),
            1, 2, 2, 3, GTK_EXPAND | GTK_FILL, 0, 2, 4);
    gtk_entry_set_width_chars(GTK_ENTRY(txt_voice_synth_path), 30);

    /* Misc. page. */
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
            table = gtk_table_new(2, 2, FALSE),
            label = gtk_label_new("Misc."));

    /* Line Width. */
    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new("Line Width"),
            0, 1, 0, 1, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(table),
            num_draw_line_width = hildon_controlbar_new(),
            1, 2, 0, 1, GTK_EXPAND | GTK_FILL, 0, 2, 4);
    hildon_controlbar_set_range(HILDON_CONTROLBAR(num_draw_line_width), 1, 20);
    force_min_visible_bars(HILDON_CONTROLBAR(num_draw_line_width), 1);

    /* Keep Display On Only When Fullscreen. */
    gtk_table_attach(GTK_TABLE(table),
            chk_always_keep_on = gtk_check_button_new_with_label(
                "Keep Display On Only in Fullscreen Mode"),
            0, 2, 1, 2, GTK_EXPAND | GTK_FILL, 0, 2, 4);

    /* Initialize fields. */
    if(_rcvr_mac)
        gtk_entry_set_text(GTK_ENTRY(txt_rcvr_mac), _rcvr_mac);
    else
    {
        scan_info.txt_rcvr_mac = txt_rcvr_mac;
        scan_info.scan_banner
            = hildon_banner_show_animation(dialog, NULL,
                "Scanning for bluetooth devices");
        scan_sid = gtk_idle_add((GSourceFunc)scan_bluetooth, &scan_info);
    }
    hildon_number_editor_set_value(HILDON_NUMBER_EDITOR(num_rcvr_chan),
            _rcvr_addr.rc_channel);
    if(_map_uri_format)
        gtk_entry_set_text(GTK_ENTRY(txt_map_uri_format), _map_uri_format);
    hildon_controlbar_set_value(HILDON_CONTROLBAR(num_zoom_steps), _zoom_steps);
    if(_map_dir_name)
        gtk_entry_set_text(GTK_ENTRY(txt_map_dir_name), _map_dir_name);
    hildon_controlbar_set_value(HILDON_CONTROLBAR(num_center_ratio),
            _center_ratio);
    hildon_controlbar_set_value(HILDON_CONTROLBAR(num_lead_ratio),
            _lead_ratio);
    hildon_controlbar_set_value(HILDON_CONTROLBAR(num_announce_notice),
            _announce_notice_ratio);
    gtk_entry_set_text(GTK_ENTRY(txt_voice_synth_path), _voice_synth_path);
    hildon_controlbar_set_value(HILDON_CONTROLBAR(num_draw_line_width),
            _draw_line_width);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chk_always_keep_on),
            !_always_keep_on);

    gtk_widget_show_all(dialog);

    while(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(dialog)))
    {
        if(!config_set_map_dir_name(gnome_vfs_expand_initial_tilde(
                gtk_entry_get_text(GTK_ENTRY(txt_map_dir_name)))))
        {
            popup_error("Could not create Map Cache directory.");
            continue;
        }

        /* Set _rcvr_mac if necessary. */
        if(!*gtk_entry_get_text(GTK_ENTRY(txt_rcvr_mac)))
        {
            /* User specified no rcvr mac - set _rcvr_mac to NULL. */
            if(_rcvr_mac)
            {
                g_free(_rcvr_mac);
                _rcvr_mac = NULL;
                rcvr_changed = TRUE;
            }
            if(_enable_gps)
            {
                gtk_check_menu_item_set_active(
                        GTK_CHECK_MENU_ITEM(_menu_enable_gps_item), FALSE);
                popup_error("No GPS Receiver MAC Provided.\n"
                        "GPS Disabled.");
                rcvr_changed = TRUE;
            }
        }
        else if(!_rcvr_mac || strcmp(_rcvr_mac,
                      gtk_entry_get_text(GTK_ENTRY(txt_rcvr_mac))))
        {
            /* User specified a new rcvr mac. */
            if(_rcvr_mac)
                g_free(_rcvr_mac);
            _rcvr_mac = g_strdup(gtk_entry_get_text(GTK_ENTRY(txt_rcvr_mac)));
            str2ba(_rcvr_mac, &_rcvr_addr.rc_bdaddr);
            rcvr_changed = TRUE;
        }

        if(_rcvr_addr.rc_channel != hildon_number_editor_get_value(
                    HILDON_NUMBER_EDITOR(num_rcvr_chan)))
        {
            _rcvr_addr.rc_channel = hildon_number_editor_get_value(
                    HILDON_NUMBER_EDITOR(num_rcvr_chan));
            rcvr_changed = TRUE;
        }

        if(_map_uri_format)
            g_free(_map_uri_format);
        if(strlen(gtk_entry_get_text(GTK_ENTRY(txt_map_uri_format))))
            _map_uri_format = g_strdup(gtk_entry_get_text(
                        GTK_ENTRY(txt_map_uri_format)));
        else
            _map_uri_format = NULL;

        _zoom_steps = hildon_controlbar_get_value(
                HILDON_CONTROLBAR(num_zoom_steps));

        _center_ratio = hildon_controlbar_get_value(
                HILDON_CONTROLBAR(num_center_ratio));

        _lead_ratio = hildon_controlbar_get_value(
                HILDON_CONTROLBAR(num_lead_ratio));

        _draw_line_width = hildon_controlbar_get_value(
                HILDON_CONTROLBAR(num_draw_line_width));

        _always_keep_on = !gtk_toggle_button_get_active(
                GTK_TOGGLE_BUTTON(chk_always_keep_on));

        _announce_notice_ratio = hildon_controlbar_get_value(
                HILDON_CONTROLBAR(num_announce_notice));

        _enable_voice = gtk_toggle_button_get_active(
                GTK_TOGGLE_BUTTON(chk_enable_voice));

        g_free(_voice_synth_path);
        _voice_synth_path = g_strdup(gtk_entry_get_text(
                    GTK_ENTRY(txt_voice_synth_path)));

        update_gcs();

        config_save();
        break;
    }

    if(scan_sid)
        g_source_remove(scan_sid);
    if(scan_info.scan_banner)
        gtk_widget_destroy(scan_info.scan_banner);

    gtk_widget_hide(dialog); /* Destroying causes a crash (!?!?!??!) */

    vprintf("%s(): return %d\n", __PRETTY_FUNCTION__, rcvr_changed);
    return rcvr_changed;
}

/**
 * Initialize all configuration from GCONF.  This should not be called more
 * than once during execution.
 */
static void
config_init()
{
    GConfValue *value;
    GConfClient *gconf_client = gconf_client_get_default();
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(!gconf_client)
    {
        fprintf(stderr, "Failed to initialize GConf.  Aborting.\n");
        exit(1);
    }

    /* Get Receiver MAC from GConf.  Default is scanned via hci_inquiry. */
    {
        _rcvr_mac = gconf_client_get_string(
                gconf_client, GCONF_KEY_RCVR_MAC, NULL);
        if(_rcvr_mac)
            str2ba(_rcvr_mac, &_rcvr_addr.rc_bdaddr);
    }

    /* Get Receiver Channel from GConf.  Default is 1. */
    _rcvr_addr.rc_family = AF_BLUETOOTH;
    _rcvr_addr.rc_channel = gconf_client_get_int(gconf_client,
            GCONF_KEY_RCVR_CHAN, NULL);
    if(_rcvr_addr.rc_channel < 1)
        _rcvr_addr.rc_channel = 1;

    /* Get Map Download URI Format.  Default is NULL. */
    _map_uri_format = gconf_client_get_string(gconf_client,
            GCONF_KEY_MAP_URI_FORMAT, NULL);

    /* Get Map Download Zoom Steps.  Default is 2. */
    _zoom_steps = gconf_client_get_int(gconf_client,
            GCONF_KEY_MAP_ZOOM_STEPS, NULL);
    if(!_zoom_steps)
        _zoom_steps = 2;

    /* Get Map Cache Directory.  Default is "~/apps/maemo-mapper". */
    {
        gchar *tmp = gconf_client_get_string(gconf_client,
                GCONF_KEY_MAP_DIR_NAME, NULL);
        if(!tmp)
            tmp = g_strdup("~/apps/maemo-mapper");
        if(!config_set_map_dir_name(gnome_vfs_expand_initial_tilde(tmp)))
        {
            popup_error("Could not create Map Cache directory.\n"
                    "Please set a valid Map Cache directory in the Settings"
                    " dialog box.");
        }
        g_free(tmp);
    }

    /* Get Auto-Download.  Default is FALSE. */
    _auto_download = gconf_client_get_bool(gconf_client,
            GCONF_KEY_AUTO_DOWNLOAD, NULL);

    /* Get Center Ratio - Default is 3. */
    _center_ratio = gconf_client_get_int(gconf_client,
            GCONF_KEY_CENTER_SENSITIVITY, NULL);
    if(!_center_ratio)
        _center_ratio = 7;

    /* Get Lead Ratio - Default is 5. */
    _lead_ratio = gconf_client_get_int(gconf_client,
            GCONF_KEY_LEAD_AMOUNT, NULL);
    if(!_lead_ratio)
        _lead_ratio = 5;

    /* Get Draw Line Width- Default is 5. */
    _draw_line_width = gconf_client_get_int(gconf_client,
            GCONF_KEY_DRAW_LINE_WIDTH, NULL);
    if(!_draw_line_width)
        _draw_line_width = 5;

    /* Get Announce Advance Notice - Default is 30. */
    value = gconf_client_get(gconf_client, GCONF_KEY_ANNOUNCE_NOTICE, NULL);
    if(value)
    {
        _announce_notice_ratio = gconf_value_get_int(value);
        gconf_value_free(value);
    }
    else
        _announce_notice_ratio = 6;

    /* Get Enable Voice flag.  Default is TRUE. */
    value = gconf_client_get(gconf_client, GCONF_KEY_ENABLE_VOICE, NULL);
    if(value)
    {
        _enable_voice = gconf_value_get_bool(value);
        gconf_value_free(value);
    }
    else
        _enable_voice = TRUE;

    /* Get Voice Synthesis Path.  Default is /usr/bin/flite. */
    _voice_synth_path = gconf_client_get_string(gconf_client,
            GCONF_KEY_VOICE_SYNTH_PATH, NULL);
    if(!_voice_synth_path)
        _voice_synth_path = g_strdup("/usr/bin/flite");

    /* Get Always Keep On flag.  Default is FALSE. */
    _always_keep_on = gconf_client_get_bool(gconf_client,
            GCONF_KEY_ALWAYS_KEEP_ON, NULL);

    /* Get last saved latitude.  Default is 0. */
    _pos_lat = gconf_client_get_float(gconf_client, GCONF_KEY_LAT, NULL);

    /* Get last saved longitude.  Default is somewhere in Midwest. */
    value = gconf_client_get(gconf_client, GCONF_KEY_LON, NULL);
    _pos_lon = gconf_client_get_float(gconf_client, GCONF_KEY_LON, NULL);

    /* Get last center point. */
    {
        gfloat center_lat, center_lon;

        /* Get last saved latitude.  Default is last saved latitude. */
        value = gconf_client_get(gconf_client, GCONF_KEY_CENTER_LAT, NULL);
        if(value)
        {
            center_lat = gconf_value_get_float(value);
            gconf_value_free(value);
        }
        else
            center_lat = _pos_lat;

        /* Get last saved longitude.  Default is last saved longitude. */
        value = gconf_client_get(gconf_client, GCONF_KEY_CENTER_LON, NULL);
        if(value)
        {
            center_lon = gconf_value_get_float(value);
            gconf_value_free(value);
        }
        else
            center_lon = _pos_lon;

        latlon2unit(center_lat, center_lon, _center.unitx, _center.unity);
    }

    /* Get last Zoom Level.  Default is 16. */
    value = gconf_client_get(gconf_client, GCONF_KEY_ZOOM, NULL);
    if(value)
    {
        _zoom = gconf_value_get_int(value);
        gconf_value_free(value);
    }
    else
        _zoom = 16;
    BOUND(_zoom, 0, MAX_ZOOM - 1);
    _world_size_tiles = unit2tile(WORLD_SIZE_UNITS);

    /* Speed and Heading are always initialized as 0. */
    _speed = 0.f;
    _heading = 0.f;

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

    /* Get Show Tracks flag.  Default is TRUE. */
    value = gconf_client_get(gconf_client, GCONF_KEY_SHOWTRACKS, NULL);
    if(value)
    {
        _show_tracks |= (gconf_value_get_bool(value) ? TRACKS_MASK : 0);
        gconf_value_free(value);
    }
    else
        _show_tracks |= TRACKS_MASK;

    /* Get Show Routes flag.  Default is TRUE. */
    value = gconf_client_get(gconf_client, GCONF_KEY_SHOWROUTES, NULL);
    if(value)
    {
        _show_tracks |= (gconf_value_get_bool(value) ? ROUTES_MASK : 0);
        gconf_value_free(value);
    }
    else
        _show_tracks |= ROUTES_MASK;

    /* Get Show Velocity Vector flag.  Default is TRUE. */
    value = gconf_client_get(gconf_client, GCONF_KEY_SHOWVELVEC, NULL);
    if(value)
    {
        _show_velvec = gconf_value_get_bool(value);
        gconf_value_free(value);
    }
    else
        _show_velvec = TRUE;

    /* Get Enable GPS flag.  Default is TRUE. */
    value = gconf_client_get(gconf_client, GCONF_KEY_ENABLE_GPS, NULL);
    if(value)
    {
        _enable_gps = gconf_value_get_bool(value);
        gconf_value_free(value);
    }
    else
        _enable_gps = TRUE;

    /* Initialize _conn_state based on _enable_gps. */
    _conn_state = (_enable_gps ? RCVR_DOWN : RCVR_OFF);

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

    g_object_unref(gconf_client);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

/**
 * Create the menu items needed for the drop down menu.
 */
static void
menu_init()
{
    /* Create needed handles. */
    GtkMenu *main_menu;
    GtkWidget *submenu;
    GtkWidget *menu_item;
    printf("%s()\n", __PRETTY_FUNCTION__);

    /* Get the menu of our view. */
    main_menu = GTK_MENU(gtk_menu_new());

    /* Create the menu items. */

    /* The "Routes" submenu. */
    gtk_menu_append(main_menu, menu_item
            = gtk_menu_item_new_with_label("Route"));
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item),
            submenu = gtk_menu_new());
    gtk_menu_append(submenu, _menu_route_open_item
            = gtk_menu_item_new_with_label("Open..."));
    gtk_menu_append(submenu, _menu_route_download_item
            = gtk_menu_item_new_with_label("Download..."));
    gtk_menu_append(submenu, _menu_route_save_item
            = gtk_menu_item_new_with_label("Save..."));
    gtk_menu_append(submenu, _menu_route_reset_item
        = gtk_menu_item_new_with_label("Reset"));
    gtk_menu_append(submenu, _menu_route_clear_item
        = gtk_menu_item_new_with_label("Clear"));

    /* The "Track" submenu. */
    gtk_menu_append(main_menu, menu_item
            = gtk_menu_item_new_with_label("Track"));
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item),
            submenu = gtk_menu_new());
    gtk_menu_append(submenu, _menu_track_open_item
            = gtk_menu_item_new_with_label("Open..."));
    gtk_menu_append(submenu, _menu_track_save_item
            = gtk_menu_item_new_with_label("Save..."));
    gtk_menu_append(submenu, _menu_track_mark_way_item
            = gtk_menu_item_new_with_label("Mark a Waypoint"));
    gtk_menu_append(submenu, _menu_track_clear_item
            = gtk_menu_item_new_with_label("Clear"));

    gtk_menu_append(main_menu, menu_item
            = gtk_menu_item_new_with_label("Maps"));
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item),
            submenu = gtk_menu_new());
    gtk_menu_append(submenu, _menu_maps_dlroute_item
            = gtk_menu_item_new_with_label("Download Along Route"));
    gtk_menu_append(submenu, _menu_maps_dlarea_item
            = gtk_menu_item_new_with_label("Download Area..."));
    gtk_menu_append(submenu, _menu_auto_download_item
            = gtk_check_menu_item_new_with_label("Auto-Download"));
    gtk_check_menu_item_set_active(
            GTK_CHECK_MENU_ITEM(_menu_auto_download_item), _auto_download);

    gtk_menu_append(main_menu, gtk_separator_menu_item_new());

    gtk_menu_append(main_menu, menu_item
            = gtk_menu_item_new_with_label("Show"));
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item),
            submenu = gtk_menu_new());
    gtk_menu_append(submenu, _menu_show_routes_item
            = gtk_check_menu_item_new_with_label("Route"));
    gtk_check_menu_item_set_active(
            GTK_CHECK_MENU_ITEM(_menu_show_routes_item),
            _show_tracks & ROUTES_MASK);
    gtk_menu_append(submenu, _menu_show_tracks_item
            = gtk_check_menu_item_new_with_label("Track"));
    gtk_check_menu_item_set_active(
            GTK_CHECK_MENU_ITEM(_menu_show_tracks_item),
            _show_tracks & TRACKS_MASK);
    gtk_menu_append(submenu, _menu_show_velvec_item
            = gtk_check_menu_item_new_with_label("Velocity Vector"));
    gtk_check_menu_item_set_active(
            GTK_CHECK_MENU_ITEM(_menu_show_velvec_item), _show_velvec);

    gtk_menu_append(main_menu, menu_item
            = gtk_menu_item_new_with_label("Auto-Center"));
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item),
            submenu = gtk_menu_new());
    gtk_menu_append(submenu, _menu_ac_latlon_item
            = gtk_radio_menu_item_new_with_label(NULL, "Lat/Lon"));
    gtk_check_menu_item_set_active(
            GTK_CHECK_MENU_ITEM(_menu_ac_latlon_item),
            _center_mode == CENTER_LATLON);
    gtk_menu_append(submenu, _menu_ac_lead_item
            = gtk_radio_menu_item_new_with_label_from_widget(
                GTK_RADIO_MENU_ITEM(_menu_ac_latlon_item), "Lead"));
    gtk_check_menu_item_set_active(
            GTK_CHECK_MENU_ITEM(_menu_ac_lead_item),
            _center_mode == CENTER_LEAD);
    gtk_menu_append(submenu, _menu_ac_none_item
            = gtk_radio_menu_item_new_with_label_from_widget(
                GTK_RADIO_MENU_ITEM(_menu_ac_latlon_item), "None"));
    gtk_check_menu_item_set_active(
            GTK_CHECK_MENU_ITEM(_menu_ac_none_item),
            _center_mode < 0);

    gtk_menu_append(main_menu, _menu_fullscreen_item
            = gtk_check_menu_item_new_with_label("Full Screen"));
    gtk_check_menu_item_set_active(
            GTK_CHECK_MENU_ITEM(_menu_fullscreen_item), _fullscreen);

    gtk_menu_append(main_menu, _menu_enable_gps_item
            = gtk_check_menu_item_new_with_label("Enable GPS"));
    gtk_check_menu_item_set_active(
            GTK_CHECK_MENU_ITEM(_menu_enable_gps_item), _enable_gps);

    gtk_menu_append(main_menu, gtk_separator_menu_item_new());

    gtk_menu_append(main_menu, _menu_settings_item
        = gtk_menu_item_new_with_label("Settings..."));

    gtk_menu_append(main_menu, gtk_separator_menu_item_new());

    gtk_menu_append(main_menu, _menu_close_item
        = gtk_menu_item_new_with_label("Close"));

    /* We need to show menu items. */
    gtk_widget_show_all(GTK_WIDGET(main_menu));

    hildon_window_set_menu(HILDON_WINDOW(_window), main_menu);

    /* Connect the signals. */
    g_signal_connect(G_OBJECT(_menu_route_open_item), "activate",
                      G_CALLBACK(menu_cb_route_open), NULL);
    g_signal_connect(G_OBJECT(_menu_route_download_item), "activate",
                      G_CALLBACK(menu_cb_route_download), NULL);
    g_signal_connect(G_OBJECT(_menu_route_save_item), "activate",
                      G_CALLBACK(menu_cb_route_save), NULL);
    g_signal_connect(G_OBJECT(_menu_route_reset_item), "activate",
                      G_CALLBACK(menu_cb_route_reset), NULL);
    g_signal_connect(G_OBJECT(_menu_route_clear_item), "activate",
                      G_CALLBACK(menu_cb_route_clear), NULL);
    g_signal_connect(G_OBJECT(_menu_track_open_item), "activate",
                      G_CALLBACK(menu_cb_track_open), NULL);
    g_signal_connect(G_OBJECT(_menu_track_save_item), "activate",
                      G_CALLBACK(menu_cb_track_save), NULL);
    g_signal_connect(G_OBJECT(_menu_track_mark_way_item), "activate",
                      G_CALLBACK(menu_cb_track_mark_way), NULL);
    g_signal_connect(G_OBJECT(_menu_track_clear_item), "activate",
                      G_CALLBACK(menu_cb_track_clear), NULL);
    g_signal_connect(G_OBJECT(_menu_show_tracks_item), "toggled",
                      G_CALLBACK(menu_cb_show_tracks), NULL);
    g_signal_connect(G_OBJECT(_menu_show_routes_item), "toggled",
                      G_CALLBACK(menu_cb_show_routes), NULL);
    g_signal_connect(G_OBJECT(_menu_show_velvec_item), "toggled",
                      G_CALLBACK(menu_cb_show_velvec), NULL);
    g_signal_connect(G_OBJECT(_menu_maps_dlroute_item), "activate",
                      G_CALLBACK(menu_cb_maps_dlroute), NULL);
    g_signal_connect(G_OBJECT(_menu_maps_dlarea_item), "activate",
                      G_CALLBACK(menu_cb_maps_dlarea), NULL);
    g_signal_connect(G_OBJECT(_menu_ac_latlon_item), "toggled",
                      G_CALLBACK(menu_cb_ac_latlon), NULL);
    g_signal_connect(G_OBJECT(_menu_ac_lead_item), "toggled",
                      G_CALLBACK(menu_cb_ac_lead), NULL);
    g_signal_connect(G_OBJECT(_menu_ac_none_item), "toggled",
                      G_CALLBACK(menu_cb_ac_none), NULL);
    g_signal_connect(G_OBJECT(_menu_fullscreen_item), "toggled",
                      G_CALLBACK(menu_cb_fullscreen), NULL);
    g_signal_connect(G_OBJECT(_menu_enable_gps_item), "toggled",
                      G_CALLBACK(menu_cb_enable_gps), NULL);
    g_signal_connect(G_OBJECT(_menu_auto_download_item), "toggled",
                      G_CALLBACK(menu_cb_auto_download), NULL);
    g_signal_connect(G_OBJECT(_menu_settings_item), "activate",
                      G_CALLBACK(menu_cb_settings), NULL);
    g_signal_connect(G_OBJECT(_menu_close_item), "activate",
                      G_CALLBACK(gtk_main_quit), NULL);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

/**
 * Call gtk_window_present() on Maemo Mapper.  This also checks the
 * configuration and brings up the Settings dialog if the GPS Receiver is
 * not set up, the first time it is called.
 */
static gboolean
window_present()
{
    static gint been_here = 0;
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(!been_here++)
    {
        /* Set connection state first, to avoid going into this if twice. */
        if(_rcvr_mac || !_enable_gps || settings_dialog())
        {
            gtk_widget_show_all(_window);

            /* Connect to receiver. */
            if(_enable_gps)
                rcvr_connect_now();
        }
        else
            gtk_main_quit();
    }
    gtk_window_present(GTK_WINDOW(_window));

    /* Re-enable any banners that might have been up. */
    set_conn_state(_conn_state);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return FALSE;
}

/**
 * Get the hash value of a ProgressUpdateInfo object.  This is trivial, since
 * the hash is generated and stored when the object is created.
 */
static guint
download_hashfunc(ProgressUpdateInfo *pui)
{
    vprintf("%s()\n", __PRETTY_FUNCTION__);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return pui->hash;
}

/**
 * Return whether or not two ProgressUpdateInfo objects are equal.  They
 * are equal if they are downloading the same tile.
 */
static gboolean
download_equalfunc(
        ProgressUpdateInfo *pui1, ProgressUpdateInfo *pui2)
{
    vprintf("%s()\n", __PRETTY_FUNCTION__);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return pui1->tilex == pui2->tilex && pui1->tiley == pui2->tiley
        && pui1->zoom == pui2->zoom;
}

/**
 * Draw the current mark (representing the current GPS location) onto
 * _map_widget.  This method does not queue the draw area.
 */
static void
map_draw_mark()
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(_enable_gps)
    {
        gdk_draw_arc(
                _map_widget->window,
                _conn_state == RCVR_FIXED ? _mark_current_gc : _mark_old_gc,
                FALSE, /* not filled. */
                _mark_x1 - _draw_line_width, _mark_y1 - _draw_line_width,
                2 * _draw_line_width, 2 * _draw_line_width,
                0, 360 * 64);
        gdk_draw_line(
                _map_widget->window,
                _conn_state == RCVR_FIXED
                    ? (_show_velvec ? _vel_current_gc : _mark_current_gc)
                    : _vel_old_gc,
                _mark_x1, _mark_y1, _mark_x2, _mark_y2);
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

/**
 * "Set" the mark, which translates the current GPS position into on-screen
 * units in preparation for drawing the mark with map_draw_mark().
 */
static void map_set_mark()
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    _mark_x1 = unit2x(_pos.unitx);
    _mark_y1 = unit2y(_pos.unity);
    _mark_x2 = _mark_x1 + (_show_velvec ? _vel_offsetx : 0);
    _mark_y2 = _mark_y1 + (_show_velvec ? _vel_offsety : 0);
    _mark_minx = MIN(_mark_x1, _mark_x2) - (2 * _draw_line_width);
    _mark_miny = MIN(_mark_y1, _mark_y2) - (2 * _draw_line_width);
    _mark_width = abs(_mark_x1 - _mark_x2) + (4 * _draw_line_width);
    _mark_height = abs(_mark_y1 - _mark_y2) + (4 * _draw_line_width);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

/**
 * Do an in-place scaling of a pixbuf's pixels at the given ratio from the
 * given source location.  It would have been nice if gdk_pixbuf provided
 * this method, but I guess it's not general-purpose enough.
 */
static void
map_pixbuf_scale_inplace(guchar *pixels, guint ratio_p2,
        guint src_x, guint src_y)
{
    guint dest_x = 0, dest_y = 0, dest_dim = TILE_SIZE_PIXELS;
    vprintf("%s(%d, %d, %d)\n", __PRETTY_FUNCTION__, ratio_p2, src_x, src_y);

    /* Sweep through the entire dest area, copying as necessary, but
     * DO NOT OVERWRITE THE SOURCE AREA.  We'll copy it afterward. */
    do {
        guint src_dim = dest_dim >> ratio_p2;
        guint src_endx = src_x - dest_x + src_dim;
        gint x, y;
        for(y = dest_dim - 1; y >= 0; y--)
        {
            guint src_offset_y, dest_offset_y;
            src_offset_y = (src_y + (y >> ratio_p2)) * TILE_PIXBUF_STRIDE;
            dest_offset_y = (dest_y + y) * TILE_PIXBUF_STRIDE;
            x = dest_dim - 1;
            if((unsigned)(dest_y + y - src_y) < src_dim
                    && (unsigned)(dest_x + x - src_x) < src_dim)
                x -= src_dim;
            for(; x >= 0; x--)
            {
                guint src_offset, dest_offset;
                src_offset = src_offset_y + (src_x + (x >> ratio_p2)) * 3;
                dest_offset = dest_offset_y + (dest_x + x) * 3;
                pixels[dest_offset + 0] = pixels[src_offset + 0];
                pixels[dest_offset + 1] = pixels[src_offset + 1];
                pixels[dest_offset + 2] = pixels[src_offset + 2];
                if((unsigned)(dest_y + y - src_y) < src_dim && x == src_endx)
                    x -= src_dim;
            }
        }
        /* Reuse src_dim and src_endx to store new src_x and src_y. */
        src_dim = src_x + ((src_x - dest_x) >> ratio_p2);
        src_endx = src_y + ((src_y - dest_y) >> ratio_p2);
        dest_x = src_x;
        dest_y = src_y;
        src_x = src_dim;
        src_y = src_endx;
    }
    while((dest_dim >>= ratio_p2) > 1);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

/**
 * Free a ProgressUpdateInfo data structure that was allocated during the
 * auto-map-download process.
 */
static void
progress_update_info_free(ProgressUpdateInfo *pui)
{
    vprintf("%s()\n", __PRETTY_FUNCTION__);

    g_hash_table_remove(_downloads_hash, pui);
    g_list_free(pui->src_list);
    g_list_free(pui->dest_list);
    g_free(pui);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

/**
 * Given the xyz coordinates of our map coordinate system, write the qrst
 * quadtree coordinates to buffer.
 */
static void
map_convert_coords_to_quadtree_string(int x, int y, int zoomlevel,gchar *buffer)
{
    static const gchar *const quadrant = "qrts";
    gchar *ptr = buffer;
    int n;
    *ptr++ = 't';
    for (n = 16 - zoomlevel; n >= 0; n--)
    {
        int xbit = (x >> n) & 1;
        int ybit = (y >> n) & 1;
        *ptr++ = quadrant[xbit + 2 * ybit];
    }
    *ptr++ = '\0';
}

/**
 * Construct the URL that we should fetch, based on the current URI format.
 * This method works differently depending on if a "%s" string is present in
 * the URI format, since that would indicate a quadtree-based map coordinate
 * system.
 */
static void
map_construct_url(gchar *buffer, guint tilex, guint tiley, guint zoom)
{
    if(strstr(_map_uri_format, "%s"))
    {
        /* This is a satellite-map URI. */
        gchar location[MAX_ZOOM + 2];
        map_convert_coords_to_quadtree_string(tilex, tiley, zoom - 1, location);
        sprintf(buffer, _map_uri_format, location);
    }
    else
        /* This is a street-map URI. */
        sprintf(buffer, _map_uri_format, tilex, tiley, zoom - 1);
}


/**
 * Initiate a download of the given xyz coordinates using the given buffer
 * as the URL.  If the map already exists on disk, or if we are already
 * downloading the map, then this method does nothing.
 */
static gboolean
map_initiate_download(gchar *buffer, guint tilex, guint tiley, guint zoom)
{
    GnomeVFSURI *src, *dest;
    GList *src_list = NULL, *dest_list = NULL;
    gint priority;
    ProgressUpdateInfo *pui;
    vprintf("%s(%s, %u, %u, %u)\n", __PRETTY_FUNCTION__,
            buffer, tilex, tiley, zoom);

    pui = g_new(ProgressUpdateInfo, 1);
    pui->hash = tilex + (tiley << 12) + (zoom << 24);
    if(g_hash_table_lookup(_downloads_hash, pui))
    {
        /* Already downloading - return FALSE. */
        g_free(pui);
        return FALSE;
    }
    dest = gnome_vfs_uri_new(buffer);
    if(gnome_vfs_uri_exists(dest))
    {
        /* Already downloaded - return FALSE. */
        gnome_vfs_uri_unref(dest);
        g_free(pui);
        return FALSE;
    }
    pui->tilex = tilex;
    pui->tiley = tiley;
    pui->zoom = zoom;
    pui->src_list = src_list;
    pui->dest_list = dest_list;

    /* Priority is based on proximity to _center.unitx - lower number means
     * higher priority, so the further we are, the higher the number. */
    priority = GNOME_VFS_PRIORITY_MIN
        + unit2ptile(abs(tile2punit(tilex, zoom) - _center.unitx)
                + abs(tile2punit(tiley, zoom) - _center.unity));
    BOUND(priority, GNOME_VFS_PRIORITY_MIN, GNOME_VFS_PRIORITY_MAX);

    map_construct_url(buffer, tilex, tiley, zoom);
    src = gnome_vfs_uri_new(buffer);
    src_list = g_list_prepend(src_list, src);
    dest_list = g_list_prepend(dest_list, dest);

    /* Make sure directory exists. */
    sprintf(buffer, "%s/%u", _map_dir_name, (pui->zoom - 1));
    gnome_vfs_make_directory(buffer, 0775);
    sprintf(buffer, "%s/%u/%u", _map_dir_name,
            (pui->zoom - 1), pui->tilex);
    gnome_vfs_make_directory(buffer, 0775);

    /* Initiate asynchronous download. */
    if(GNOME_VFS_OK != gnome_vfs_async_xfer(
                &pui->handle,
                src_list, dest_list,
                GNOME_VFS_XFER_USE_UNIQUE_NAMES, /* needed for dupe detect. */
                GNOME_VFS_XFER_ERROR_MODE_QUERY,
                GNOME_VFS_XFER_OVERWRITE_MODE_QUERY,
                priority,
                (GnomeVFSAsyncXferProgressCallback)map_download_cb_async,
                pui,
                (GnomeVFSXferProgressCallback)gtk_true,
                NULL))
    {
        progress_update_info_free(pui);
        return FALSE;
    }

    g_hash_table_insert(_downloads_hash, pui, pui);
    if(!_num_downloads++ && !_download_banner)
        _download_banner = hildon_banner_show_progress(
                _window, NULL, "Downloading maps");

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static void
map_render_tile(guint tilex, guint tiley, guint destx, guint desty,
        gboolean fast_fail) {
    gchar buffer[1024];
    GdkPixbuf *pixbuf = NULL;
    GError *error = NULL;
    guint zoff;

    if(tilex < _world_size_tiles && tiley < _world_size_tiles)
    {
        /* The tile is possible. */
        vprintf("%s(%u, %u, %u, %u)\n", __PRETTY_FUNCTION__,
                tilex, tiley, destx, desty);
        sprintf(buffer, "%s/%u/%u/%u.jpg",
            _map_dir_name, (_zoom - 1), tilex, tiley);
        pixbuf = gdk_pixbuf_new_from_file(buffer, &error);

        if(error)
        {
            pixbuf = NULL;
            error = NULL;
        }

        if(!pixbuf && !fast_fail && _auto_download && _map_uri_format && _zoom
                && !((_zoom - 1) % _zoom_steps))
        {
            map_initiate_download(buffer, tilex, tiley, _zoom);
            fast_fail = TRUE;
        }

        for(zoff = 1; !pixbuf && (_zoom + zoff) <= MAX_ZOOM; zoff += 1)
        {
            /* Attempt to blit a wider map. */
            sprintf(buffer, "%s/%u/%u/%u.jpg",
                    _map_dir_name, _zoom + zoff - 1,
                    (tilex >> zoff), (tiley >> zoff));
            pixbuf = gdk_pixbuf_new_from_file(buffer, &error);
            if(error)
            {
                pixbuf = NULL;
                error = NULL;
            }
            if(pixbuf)
            {
                map_pixbuf_scale_inplace(gdk_pixbuf_get_pixels(pixbuf), zoff,
                  (tilex - ((tilex >> zoff) << zoff)) << (TILE_SIZE_P2 - zoff),
                  (tiley - ((tiley >> zoff) << zoff)) << (TILE_SIZE_P2 - zoff));
            }
            else
            {
                if(_auto_download && _map_uri_format
                        && !((_zoom + zoff - 1) % _zoom_steps))
                {
                    if(!fast_fail)
                        map_initiate_download(buffer,
                                tilex >> zoff, tiley >> zoff, _zoom + zoff);
                    fast_fail = TRUE;
                }
            }
        }
    }
    if(!pixbuf)
        pixbuf = gdk_pixbuf_new(
                GDK_COLORSPACE_RGB,
                FALSE, /* no alpha. */
                8, /* 8 bits per sample. */
                TILE_SIZE_PIXELS, TILE_SIZE_PIXELS);
    if(pixbuf)
    {
        gdk_draw_pixbuf(
                _map_pixmap,
                _mark_current_gc,
                pixbuf,
                0, 0,
                destx, desty,
                TILE_SIZE_PIXELS, TILE_SIZE_PIXELS,
                GDK_RGB_DITHER_NONE, 0, 0);
        g_object_unref(pixbuf);
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

/**
 * Force a redraw of the entire _map_pixmap, including fetching the
 * background maps from disk and redrawing the tracks on top of them.
 */
void
map_force_redraw()
{
    guint new_x, new_y;
    printf("%s()\n", __PRETTY_FUNCTION__);

    for(new_y = 0; new_y < BUF_HEIGHT_TILES; ++new_y)
        for(new_x = 0; new_x < BUF_WIDTH_TILES; ++new_x)
        {
            map_render_tile(
                    _base_tilex + new_x,
                    _base_tiley + new_y,
                    new_x * TILE_SIZE_PIXELS,
                    new_y * TILE_SIZE_PIXELS,
                    FALSE);
        }
    map_render_tracks();
    MACRO_QUEUE_DRAW_AREA();

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

/**
 * Set the current zoom level.  If the given zoom level is the same as the
 * current zoom level, or if the new zoom is invalid
 * (not MIN_ZOOM <= new_zoom < MAX_ZOOM), then this method does nothing.
 */
void
map_set_zoom(guint new_zoom)
{
    printf("%s(%d)\n", __PRETTY_FUNCTION__, _zoom);

    /* Note that, since new_zoom is a guint and MIN_ZOOM is 0, this if
     * condition also checks for new_zoom >= MIN_ZOOM. */
    if(new_zoom > (MAX_ZOOM - 1) || new_zoom == _zoom)
        return;
    _zoom = new_zoom;
    _world_size_tiles = unit2tile(WORLD_SIZE_UNITS);

    /* If we're leading, update the center to reflect new zoom level. */
    MACRO_RECALC_CENTER(_center.unitx, _center.unity);

    /* Update center bounds to reflect new zoom level. */
    _min_center.unitx = pixel2unit(grid2pixel(_screen_grids_halfwidth));
    _min_center.unity = pixel2unit(grid2pixel(_screen_grids_halfheight));
    _max_center.unitx = WORLD_SIZE_UNITS-grid2unit(_screen_grids_halfwidth) - 1;
    _max_center.unity = WORLD_SIZE_UNITS-grid2unit(_screen_grids_halfheight)- 1;

    BOUND(_center.unitx, _min_center.unitx, _max_center.unitx);
    BOUND(_center.unity, _min_center.unity, _max_center.unity);

    _base_tilex = grid2tile((gint)pixel2grid(
                (gint)unit2pixel((gint)_center.unitx))
            - (gint)_screen_grids_halfwidth);
    _base_tiley = grid2tile(pixel2grid(
                unit2pixel(_center.unity))
            - _screen_grids_halfheight);

    /* New zoom level, so we can't reuse the old buffer's pixels. */


    /* Update state variables. */
    MACRO_RECALC_OFFSET();
    MACRO_RECALC_FOCUS_BASE();
    MACRO_RECALC_FOCUS_SIZE();

    map_set_mark();
    map_force_redraw();

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

/**
 * Center the view on the given unitx/unity.
 */
static void
map_center_unit(guint new_center_unitx, guint new_center_unity)
{
    gint new_base_tilex, new_base_tiley;
    guint new_x, new_y;
    guint j, k, base_new_x, base_old_x, old_x, old_y, iox, ioy;
    printf("%s(%d, %d)\n", __PRETTY_FUNCTION__,
            new_center_unitx, new_center_unity);

    /* Assure that _center.unitx/y are bounded. */
    BOUND(new_center_unitx, _min_center.unitx, _max_center.unitx);
    BOUND(new_center_unity, _min_center.unity, _max_center.unity);

    _prev_center.unitx  = _center.unitx;
    _prev_center.unity  = _center.unity;
    _center.unitx = new_center_unitx;
    _center.unity = new_center_unity;

    new_base_tilex = grid2tile((gint)pixel2grid(
                (gint)unit2pixel((gint)_center.unitx))
            - (gint)_screen_grids_halfwidth);
    new_base_tiley = grid2tile(pixel2grid(
                unit2pixel(_center.unity))
            - _screen_grids_halfheight);

    /* Same zoom level, so it's likely that we can reuse some of the old
     * buffer's pixels. */

    if(new_base_tilex != _base_tilex
            || new_base_tiley != _base_tiley)
    {
        /* If copying from old parts to new parts, we need to make sure we
         * don't overwrite the old parts when copying, so set up new_x,
         * new_y, old_x, old_y, iox, and ioy with that in mind. */
        if(new_base_tiley < _base_tiley)
        {
            /* New is lower than old - start at bottom and go up. */
            new_y = BUF_HEIGHT_TILES - 1;
            ioy = -1;
        }
        else
        {
            /* New is higher than old - start at top and go down. */
            new_y = 0;
            ioy = 1;
        }
        if(new_base_tilex < _base_tilex)
        {
            /* New is righter than old - start at right and go left. */
            base_new_x = BUF_WIDTH_TILES - 1;
            iox = -1;
        }
        else
        {
            /* New is lefter than old - start at left and go right. */
            base_new_x = 0;
            iox = 1;
        }

        /* Iterate over the y tile values. */
        old_y = new_y + new_base_tiley - _base_tiley;
        base_old_x = base_new_x + new_base_tilex - _base_tilex;
        _base_tilex = new_base_tilex;
        _base_tiley = new_base_tiley;
        for(j = 0; j < BUF_HEIGHT_TILES; ++j, new_y += ioy, old_y += ioy)
        {
            new_x = base_new_x;
            old_x = base_old_x;
            /* Iterate over the x tile values. */
            for(k = 0; k < BUF_WIDTH_TILES; ++k, new_x += iox, old_x += iox)
            {
                /* Can we get this grid block from the old buffer?. */
                if(old_x >= 0 && old_x < BUF_WIDTH_TILES
                        && old_y >= 0 && old_y < BUF_HEIGHT_TILES)
                {
                    /* Copy from old buffer to new buffer. */
                    gdk_draw_drawable(
                            _map_pixmap,
                            _mark_current_gc,
                            _map_pixmap,
                            old_x * TILE_SIZE_PIXELS,
                            old_y * TILE_SIZE_PIXELS,
                            new_x * TILE_SIZE_PIXELS,
                            new_y * TILE_SIZE_PIXELS,
                            TILE_SIZE_PIXELS, TILE_SIZE_PIXELS);
                }
                else
                {
                    map_render_tile(
                            new_base_tilex + new_x,
                            new_base_tiley + new_y,
                            new_x * TILE_SIZE_PIXELS,
                            new_y * TILE_SIZE_PIXELS,
                            FALSE);
                }
            }
        }
        map_render_tracks();
    }

    MACRO_RECALC_OFFSET();
    MACRO_RECALC_FOCUS_BASE();

    map_set_mark();
    MACRO_QUEUE_DRAW_AREA();

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

/**
 * Pan the view by the given number of units in the X and Y directions.
 */
void
map_pan(gint delta_unitx, gint delta_unity)
{
    printf("%s(%d, %d)\n", __PRETTY_FUNCTION__, delta_unitx, delta_unity);

    if(_center_mode > 0)
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(
                    _menu_ac_none_item), TRUE);
    map_center_unit(
            _center.unitx + delta_unitx,
            _center.unity + delta_unity);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

/**
 * Initiate a move of the mark from the old location to the current
 * location.  This function queues the draw area of the old mark (to force
 * drawing of the background map), then updates the mark, then queus the
 * draw area of the new mark.
 */
static void
map_move_mark()
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    /* Just queue the old and new draw areas. */
    gtk_widget_queue_draw_area(_map_widget,
            _mark_minx,
            _mark_miny,
            _mark_width,
            _mark_height);
    map_set_mark();
    gtk_widget_queue_draw_area(_map_widget,
            _mark_minx,
            _mark_miny,
            _mark_width,
            _mark_height);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

/**
 * Make sure the mark is up-to-date.  This function triggers a panning of
 * the view if the mark is appropriately close to the edge of the view.
 */
static void
refresh_mark()
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    guint new_center_unitx;
    guint new_center_unity;

    MACRO_RECALC_CENTER(new_center_unitx, new_center_unity);

    if((new_center_unitx - _focus.unitx) < _focus_unitwidth
            && (new_center_unity - _focus.unity) < _focus_unitheight)
        /* We're not changing the view - just move the mark. */
        map_move_mark();
    else
        map_center_unit(new_center_unitx, new_center_unity);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

/**
 * Handle a start tag in the parsing of a GPX file.
 */
#define MACRO_SET_UNKNOWN() { \
    data->prev_state = data->state; \
    data->state = UNKNOWN; \
    data->unknown_depth = 1; \
}
static void
gpx_start_element(SaxData *data, const xmlChar *name, const xmlChar **attrs)
{
    vprintf("%s(%s)\n", __PRETTY_FUNCTION__, name);

    switch(data->state)
    {
        case ERROR:
            break;
        case START:
            if(!strcmp((gchar*)name, "gpx"))
                data->state = INSIDE_GPX;
            else
                MACRO_SET_UNKNOWN();
            break;
        case INSIDE_GPX:
            if(!strcmp((gchar*)name, "trk"))
                data->state = INSIDE_TRACK;
            else
                MACRO_SET_UNKNOWN();
            break;
        case INSIDE_TRACK:
            if(!strcmp((gchar*)name, "trkseg"))
            {
                data->state = INSIDE_TRACK_SEGMENT;
                data->at_least_one_trkpt = FALSE;
            }
            else
                MACRO_SET_UNKNOWN();
            break;
        case INSIDE_TRACK_SEGMENT:
            if(!strcmp((gchar*)name, "trkpt"))
            {
                const xmlChar **curr_attr;
                gchar *error_check;
                gfloat lat = 0.f, lon = 0.f;
                gboolean has_lat, has_lon;
                has_lat = FALSE;
                has_lon = FALSE;
                for(curr_attr = attrs; *curr_attr != NULL; )
                {
                    const gchar *attr_name = *curr_attr++;
                    const gchar *attr_val = *curr_attr++;
                    if(!strcmp(attr_name, "lat"))
                    {
                        lat = g_ascii_strtod(attr_val, &error_check);
                        if(error_check != attr_val)
                            has_lat = TRUE;
                    }
                    else if(!strcmp(attr_name, "lon"))
                    {
                        lon = g_ascii_strtod(attr_val, &error_check);
                        if(error_check != attr_val)
                            has_lon = TRUE;
                    }
                }
                if(has_lat && has_lon)
                {
                    MACRO_TRACK_INCREMENT_TAIL(data->track);
                    latlon2unit(lat, lon, data->track.tail->unitx,
                            data->track.tail->unity);
                    data->state = INSIDE_TRACK_POINT;
                }
                else
                    data->state = ERROR;
            }
            else
                MACRO_SET_UNKNOWN();
            break;
        case INSIDE_TRACK_POINT:
            if(!strcmp((gchar*)name, "desc"))
                data->state = INSIDE_TRACK_POINT_DESC;
            else
                MACRO_SET_UNKNOWN();
            break;
        case UNKNOWN:
            data->unknown_depth++;
            break;
        default:
            ;
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

/**
 * Handle an end tag in the parsing of a GPX file.
 */
static void
gpx_end_element(SaxData *data, const xmlChar *name)
{
    vprintf("%s(%s)\n", __PRETTY_FUNCTION__, name);

    switch(data->state)
    {
        case ERROR:
            break;
        case START:
            data->state = ERROR;
            break;
        case INSIDE_GPX:
            if(!strcmp((gchar*)name, "gpx"))
                data->state = FINISH;
            else
                data->state = ERROR;
            break;
        case INSIDE_TRACK:
            if(!strcmp((gchar*)name, "trk"))
                data->state = INSIDE_GPX;
            else
                data->state = ERROR;
            break;
        case INSIDE_TRACK_SEGMENT:
            if(!strcmp((gchar*)name, "trkseg"))
            {
                if(data->at_least_one_trkpt)
                {
                    MACRO_TRACK_INCREMENT_TAIL(data->track);
                    data->track.tail->unitx = data->track.tail->unity = 0;
                }
                data->state = INSIDE_TRACK;
            }
            else
                data->state = ERROR;
            break;
        case INSIDE_TRACK_POINT:
            if(!strcmp((gchar*)name, "trkpt"))
            {
                data->state = INSIDE_TRACK_SEGMENT;
                data->at_least_one_trkpt = TRUE;
            }
            else
                data->state = ERROR;
            break;
        case INSIDE_TRACK_POINT_DESC:
            if(!strcmp((gchar*)name, "desc"))
            {
                MACRO_TRACK_INCREMENT_WTAIL(data->track);
                data->track.wtail->point = data->track.tail;
                data->track.wtail->desc = g_string_free(data->chars, FALSE);
                data->chars = g_string_new("");
                data->state = INSIDE_TRACK_POINT;
            }
            else
                data->state = ERROR;
            break;
        case UNKNOWN:
            if(!--data->unknown_depth)
                data->state = data->prev_state;
            else
                data->state = ERROR;
            break;
        default:
            ;
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

/**
 * Handle char data in the parsing of a GPX file.
 */
static void
gpx_chars(SaxData *data, const xmlChar *ch, int len)
{
    guint i;
    vprintf("%s()\n", __PRETTY_FUNCTION__);

    switch(data->state)
    {
        case ERROR:
            break;
        case INSIDE_TRACK_POINT_DESC:
            for (i = 0; i < len; i++)
                data->chars = g_string_append_c(data->chars, ch[i]);
            vprintf("%s\n", data->chars->str);
            break;
        default:
            ;
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

/**
 * Handle an entity in the parsing of a GPX file.  We don't do anything
 * special here.
 */
static xmlEntityPtr
gpx_get_entity(SaxData *data, const xmlChar *name)
{
    vprintf("%s()\n", __PRETTY_FUNCTION__);
    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return xmlGetPredefinedEntity(name);
}

/**
 * Handle an error in the parsing of a GPX file.
 */
static void
gpx_error(SaxData *data, const gchar *msg, ...)
{
    vprintf("%s()\n", __PRETTY_FUNCTION__);
    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    data->state = ERROR;
}

/**
 * Parse the given character buffer of the given size, replacing the given
 * Track's pointers with pointers to new arrays depending on the given
 * policy, adding extra_bins slots in the arrays for new data.
 *
 * policy_old should be negative to indicate that the existing data should
 * be prepended to the new GPX data, positive to indicate the opposite, and
 * zero to indicate that we should throw away the old data.
 *
 * When importing tracks, we *prepend* the GPX data and provide extra_bins.
 * When importing routes, we *append* in the case of regular routes, and we
 * *replace* in the case of automatic routing.  Routes get no extra bins.
 */
static gboolean
parse_gpx(gchar *buffer, gint size, Track *toreplace, gint policy_old,
        gint extra_bins)
{
    SaxData data;
    xmlSAXHandler sax_handler;
    printf("%s()\n", __PRETTY_FUNCTION__);

    MACRO_INIT_TRACK(data.track);
    data.state = START;
    data.chars = g_string_new("");

    memset(&sax_handler, 0, sizeof(sax_handler));
    sax_handler.characters = (charactersSAXFunc)gpx_chars;
    sax_handler.startElement = (startElementSAXFunc)gpx_start_element;
    sax_handler.endElement = (endElementSAXFunc)gpx_end_element;
    sax_handler.entityDecl = (entityDeclSAXFunc)gpx_get_entity;
    sax_handler.warning = (warningSAXFunc)gpx_error;
    sax_handler.error = (errorSAXFunc)gpx_error;
    sax_handler.fatalError = (fatalErrorSAXFunc)gpx_error;

    xmlSAXUserParseMemory(&sax_handler, &data, buffer, size);
    g_string_free(data.chars, TRUE);

    if(data.state != FINISH)
    {
        vprintf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
        return FALSE;
    }
    
    /* Successful parsing - replace given Track structure. */
    if(policy_old && toreplace->head)
    {
        TrackPoint *src_first;
        Track *src, *dest;

        if(policy_old > 0)
        {
            /* Append to current track. */
            src = &data.track;
            dest = toreplace;
        }
        else
        {
            /* Prepend to current track. */
            src = toreplace;
            dest = &data.track;
        }

        /* Find src_first non-zero point. */
        for(src_first = src->head - 1; src_first++ != src->tail; )
            if(src_first->unity != 0)
                break;

        /* Append track points from src to dest. */
        if(src->tail >= src_first)
        {
            WayPoint *curr;
            guint num_dest_points = dest->tail - dest->head + 1;
            guint num_src_points = src->tail - src_first + 1;

            /* Adjust dest->tail to be able to fit src track data
             * plus room for more track data. */
            track_resize(dest, num_dest_points + num_src_points + extra_bins);

            memcpy(dest->tail + 1, src_first,
                    num_src_points * sizeof(TrackPoint));

            dest->tail += num_src_points;

            /* Append waypoints from src to dest->. */
            track_wresize(dest, (dest->wtail - dest->whead)
                    + (src->wtail - src->whead) + 2 + extra_bins);
            for(curr = src->whead - 1; curr++ != src->wtail; )
            {
                (++(dest->wtail))->point = dest->head + num_dest_points
                    + (curr->point - src_first);
                dest->wtail->desc = curr->desc;
            }

        }

        /* Kill old track - don't use MACRO_CLEAR_TRACK(), because that
         * would free the string desc's that we moved to data.track. */
        g_free(src->head);
        g_free(src->whead);
        if(policy_old < 0)
            *toreplace = *dest;
    }
    else
    {
        MACRO_CLEAR_TRACK(*toreplace);
        /* Overwrite with data.track. */
        *toreplace = data.track;
        track_resize(toreplace,
                toreplace->tail - toreplace->head + 1 + extra_bins);
    }

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

/**
 * This is a multi-purpose function for allowing the user to select a file
 * for either reading or writing.  If chooser_action is
 * GTK_FILE_CHOOSER_ACTION_OPEN, then bytes_out and size_out must be
 * non-NULL.  If chooser_action is GTK_FILE_CHOOSER_ACTION_SAVE, then
 * handle_out must be non-NULL.  Either dir or file (or both) can be NULL.
 * This function returns TRUE if a file was successfully opened.
 */
static gboolean
open_file(gchar **bytes_out, GnomeVFSHandle **handle_out, gint *size_out,
        gchar **dir, gchar **file, GtkFileChooserAction chooser_action)
{
    GtkWidget *dialog;
    gboolean success = FALSE;
    printf("%s()\n", __PRETTY_FUNCTION__);

    dialog = hildon_file_chooser_dialog_new(GTK_WINDOW(_window),chooser_action);

    if(dir && *dir)
        gtk_file_chooser_set_current_folder_uri(
                GTK_FILE_CHOOSER(dialog), *dir);
    if(file && *file)
    {
        GValue val;
        gtk_file_chooser_set_uri(
                GTK_FILE_CHOOSER(dialog), *file);
        if(chooser_action == GTK_FILE_CHOOSER_ACTION_SAVE)
        {
            /* Work around a bug in HildonFileChooserDialog. */
            memset(&val, 0, sizeof(val));
            g_value_init(&val, G_TYPE_BOOLEAN);
            g_value_set_boolean(&val, FALSE);
            g_object_set_property(G_OBJECT(dialog), "autonaming", &val);
            gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog),
                    strrchr(*file, '/') + 1);
        }
    }

    gtk_widget_show_all(GTK_WIDGET(dialog));

    while(!success && gtk_dialog_run(GTK_DIALOG(dialog))==GTK_RESPONSE_OK)
    {
        gchar *file_uri_str;
        GnomeVFSResult vfs_result;

        /* Get the selected filename. */
        file_uri_str = gtk_file_chooser_get_uri(GTK_FILE_CHOOSER(dialog));

        if((chooser_action == GTK_FILE_CHOOSER_ACTION_OPEN
                && (GNOME_VFS_OK != (vfs_result = gnome_vfs_read_entire_file(
                        file_uri_str, size_out, bytes_out))))
                || (chooser_action == GTK_FILE_CHOOSER_ACTION_SAVE
                    && GNOME_VFS_OK != (vfs_result = gnome_vfs_create(
                            handle_out, file_uri_str,
                            GNOME_VFS_OPEN_WRITE, FALSE, 0664))))
        {
            gchar buffer[1024];
            sprintf(buffer, "Failed to open file for %s.\n%s",
                    chooser_action == GTK_FILE_CHOOSER_ACTION_OPEN
                    ? "reading" : "writing",
                    gnome_vfs_result_to_string(vfs_result));
            popup_error(buffer);
        }
        else
            success = TRUE;

        g_free(file_uri_str);
    }

    if(success)
    {
        /* Success!. */
        if(dir)
        {
            if(*dir)
                g_free(*dir);
            *dir = gtk_file_chooser_get_current_folder_uri(
                    GTK_FILE_CHOOSER(dialog));
        }

        /* If desired, save the file for later. */
        if(file)
        {
            if(*file)
                g_free(*file);
            *file = gtk_file_chooser_get_uri(GTK_FILE_CHOOSER(dialog));
        }
    }

    gtk_widget_destroy(dialog);

    vprintf("%s(): return %d\n", __PRETTY_FUNCTION__, success);
    return success;
}

/**
 * Refresh the view based on the fact that the route has been automatically
 * updated.
 */
static gboolean
auto_route_dl_idle_refresh()
{
    /* Make sure we're still supposed to do work. */
    vprintf("%s()\n", __PRETTY_FUNCTION__);

    if(!_autoroute_data.enabled)
        _autoroute_data.handle = NULL;
    else
    {
        if(parse_gpx(_autoroute_data.bytes, _autoroute_data.bytes_read,
                    &_route, 0, 0))
        {
            /* Find the nearest route point, if we're connected. */
            route_find_nearest_point();

            map_force_redraw();
        }
    }

    _autoroute_data.in_progress = FALSE;

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return FALSE;
}

/**
 * Read the data provided by the given handle as GPX data, updating the
 * auto-route with that data.
 */
static void
auto_route_dl_cb_read(GnomeVFSAsyncHandle *handle,
        GnomeVFSResult result,
        gpointer bytes,
        GnomeVFSFileSize bytes_requested,
        GnomeVFSFileSize bytes_read)
{
    vprintf("%s(%s)\n", __PRETTY_FUNCTION__,gnome_vfs_result_to_string(result));

    _autoroute_data.bytes_read += bytes_read;
    if(result == GNOME_VFS_OK)
    {
        /* Expand bytes and continue reading. */
        if(_autoroute_data.bytes_read * 2 > _autoroute_data.bytes_maxsize)
        {
            _autoroute_data.bytes = g_renew(gchar, _autoroute_data.bytes,
                    2 * _autoroute_data.bytes_read);
            _autoroute_data.bytes_maxsize = 2 * _autoroute_data.bytes_read;
        }
        gnome_vfs_async_read(_autoroute_data.handle,
                _autoroute_data.bytes + _autoroute_data.bytes_read,
                _autoroute_data.bytes_maxsize - _autoroute_data.bytes_read,
                (GnomeVFSAsyncReadCallback)auto_route_dl_cb_read, NULL);
    }
    else
        g_idle_add((GSourceFunc)auto_route_dl_idle_refresh, NULL);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

/**
 * Open the given handle in preparation for reading GPX data.
 */
static void
auto_route_dl_cb_open(GnomeVFSAsyncHandle *handle, GnomeVFSResult result)
{
    vprintf("%s()\n", __PRETTY_FUNCTION__);

    if(result == GNOME_VFS_OK)
        gnome_vfs_async_read(_autoroute_data.handle,
                _autoroute_data.bytes,
                _autoroute_data.bytes_maxsize,
                (GnomeVFSAsyncReadCallback)auto_route_dl_cb_read, NULL);
    else
    {
        _autoroute_data.in_progress = FALSE;
        _autoroute_data.handle = NULL;
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

/**
 * This function is periodically run to download updated auto-route data
 * from the route source.
 */
static gboolean
auto_route_dl_idle()
{
    gchar buffer[1024];
    vprintf("%s(%f, %f, %s)\n", __PRETTY_FUNCTION__,
            _pos_lat, _pos_lon, _autoroute_data.dest);

    sprintf(buffer,"http://www.gnuite.com/cgi-bin/gpx.cgi?saddr=%f,%f&daddr=%s",
            _pos_lat, _pos_lon, _autoroute_data.dest);
    printf("Downloading %s\n", buffer);
    _autoroute_data.bytes_read = 0;

    gnome_vfs_async_open(&_autoroute_data.handle,
            buffer, GNOME_VFS_OPEN_READ,
            GNOME_VFS_PRIORITY_DEFAULT,
            (GnomeVFSAsyncOpenCallback)auto_route_dl_cb_open, NULL);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return FALSE;
}

/**
 * Cancel the current auto-route.
 */
static void
cancel_autoroute()
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(_autoroute_data.enabled)
    {
        _autoroute_data.enabled = FALSE;
        if(_autoroute_data.dest)
        {
            g_free(_autoroute_data.dest);
            _autoroute_data.dest = NULL;
        }
        if(_autoroute_data.handle)
        {
            gnome_vfs_async_cancel(_autoroute_data.handle);
            _autoroute_data.handle = NULL;
        }
        if(_autoroute_data.bytes)
        {
            g_free(_autoroute_data.bytes);
            _autoroute_data.bytes = NULL;
        }
        _autoroute_data.in_progress = FALSE;
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

/**
 * Save state and destroy all non-UI elements created by this program in
 * preparation for exiting.
 */
static void
maemo_mapper_destroy(void)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    config_save();
    rcvr_disconnect();
    /* _program and widgets have already been destroyed. */

    MACRO_CLEAR_TRACK(_track);
    if(_route.head)
        MACRO_CLEAR_TRACK(_route);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

/**
 * Initialize everything required in preparation for calling gtk_main().
 */
static void
maemo_mapper_init(gint argc, gchar **argv)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    config_init();

    /* Initialize _program. */
    _program = HILDON_PROGRAM(hildon_program_get_instance());
    g_set_application_name("Maemo Mapper");

    /* Initialize _window. */
    _window = GTK_WIDGET(hildon_window_new());
    hildon_program_add_window(_program, HILDON_WINDOW(_window));

    /* Create and add widgets and supporting data. */
    _map_widget = gtk_drawing_area_new();
    gtk_container_add(GTK_CONTAINER(_window), _map_widget);

    gtk_widget_realize(_map_widget);

    _map_pixmap = gdk_pixmap_new(
                _map_widget->window,
                BUF_WIDTH_PIXELS, BUF_HEIGHT_PIXELS,
                -1); /* -1: use bit depth of widget->window. */

    /* Connect signals. */
    g_signal_connect(G_OBJECT(_window), "destroy",
            G_CALLBACK(gtk_main_quit), NULL);

    g_signal_connect(G_OBJECT(_window), "key_press_event",
            G_CALLBACK(window_cb_key_press), NULL);

    g_signal_connect(G_OBJECT(_map_widget), "configure_event",
            G_CALLBACK(map_cb_configure), NULL);

    g_signal_connect(G_OBJECT(_map_widget), "expose_event",
            G_CALLBACK(map_cb_expose), NULL);

    g_signal_connect(G_OBJECT(_map_widget), "button_release_event",
            G_CALLBACK(map_cb_button_release), NULL);

    gtk_widget_add_events(_map_widget,
            GDK_EXPOSURE_MASK
            | GDK_BUTTON_PRESS_MASK
            | GDK_BUTTON_RELEASE_MASK);

    osso_hw_set_event_cb(_osso, NULL, osso_cb_hw_state, NULL);

    gnome_vfs_async_set_job_limit(24);

    /* Initialize data. */

    memset(&_track, 0, sizeof(_track));
    memset(&_route, 0, sizeof(_route));
    _last_spoken_phrase = g_strdup("");

    /* Set up track array. */
    MACRO_INIT_TRACK(_track);

    _downloads_hash = g_hash_table_new((GHashFunc)download_hashfunc,
            (GEqualFunc)download_equalfunc);
    memset(&_autoroute_data, 0, sizeof(_autoroute_data));

    integerize_data();

    /* Initialize our line styles. */
    {
        GdkColor red = { 0, 0xffff, 0, 0 };
        GdkColor dark_red = { 0, 0xbfff, 0, 0 };
        GdkColor green = { 0, 0, 0xdfff, 0 };
        GdkColor dark_green = { 0, 0, 0xbfff, 0 };
        GdkColor darker_green = { 0, 0, 0x9fff, 0 };
        GdkColor blue = { 0, 0x5fff, 0x5fff, 0xffff };
        GdkColor dark_blue = { 0, 0, 0, 0xffff };
        GdkColor gray = { 0, 0x7fff, 0x7fff, 0x7fff };

        gdk_color_alloc(gtk_widget_get_colormap(_map_widget), &red);
        gdk_color_alloc(gtk_widget_get_colormap(_map_widget), &dark_red);
        gdk_color_alloc(gtk_widget_get_colormap(_map_widget), &green);
        gdk_color_alloc(gtk_widget_get_colormap(_map_widget), &dark_green);
        gdk_color_alloc(gtk_widget_get_colormap(_map_widget), &darker_green);
        gdk_color_alloc(gtk_widget_get_colormap(_map_widget), &blue);
        gdk_color_alloc(gtk_widget_get_colormap(_map_widget), &dark_blue);
        gdk_color_alloc(gtk_widget_get_colormap(_map_widget), &gray);

        /* _mark_current_gc is used to draw the mark when data is current. */
        _mark_current_gc = gdk_gc_new(_map_pixmap);
        gdk_gc_set_foreground(_mark_current_gc, &dark_blue);

        /* _mark_old_gc is used to draw the mark when data is old. */
        _mark_old_gc = gdk_gc_new(_map_pixmap);
        gdk_gc_copy(_mark_old_gc, _mark_current_gc);
        gdk_gc_set_foreground(_mark_old_gc, &gray);

        /* _vel_current_gc is used to draw the vel_current line. */
        _vel_current_gc = gdk_gc_new(_map_pixmap);
        gdk_gc_copy(_vel_current_gc, _mark_current_gc);
        gdk_gc_set_foreground(_vel_current_gc, &blue);

        /* _vel_current_gc is used to draw the vel mark when data is old. */
        _vel_old_gc = gdk_gc_new(_map_pixmap);
        gdk_gc_copy(_vel_old_gc, _mark_old_gc);

        /* _track_gc is used to draw the track line. */
        _track_gc = gdk_gc_new(_map_pixmap);
        gdk_gc_copy(_track_gc, _mark_current_gc);
        gdk_gc_set_foreground(_track_gc, &red);

        /* _track_way_gc is used to draw the track_way line. */
        _track_way_gc = gdk_gc_new(_map_pixmap);
        gdk_gc_copy(_track_way_gc, _mark_current_gc);
        gdk_gc_set_foreground(_track_way_gc, &dark_red);

        /* _route_gc is used to draw the route line. */
        _route_gc = gdk_gc_new(_map_pixmap);
        gdk_gc_copy(_route_gc, _mark_current_gc);
        gdk_gc_set_foreground(_route_gc, &green);

        /* _way_gc is used to draw the waypoint dots. */
        _route_way_gc = gdk_gc_new(_map_pixmap);
        gdk_gc_copy(_route_way_gc, _mark_current_gc);
        gdk_gc_set_foreground(_route_way_gc, &dark_green);

        /* _next_way_gc is used to draw the next_way labels. */
        _next_way_gc = gdk_gc_new(_map_pixmap);
        gdk_gc_copy(_next_way_gc, _mark_current_gc);
        gdk_gc_set_foreground(_next_way_gc, &darker_green);

        update_gcs();
    }

    menu_init();

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
            gchar buffer[1024];
            sprintf(buffer, "Failed to open file for reading.\n%s",
                    gnome_vfs_result_to_string(vfs_result));
            popup_error(buffer);
        }
        else
        {
            /* If auto is enabled, append the route, otherwise replace it. */
            if(parse_gpx(buffer, size, &_route, 0, 0))
                hildon_banner_show_information(_window, NULL, "Route Opened");
            else
                popup_error("Error parsing GPX file.");
            g_free(buffer);
        }
        g_free(file_uri);
    }

    gtk_idle_add((GSourceFunc)window_present, NULL);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

/****************************************************************************
 * ABOVE: ROUTINES **********************************************************
 ****************************************************************************/



/****************************************************************************
 * BELOW: MAIN **************************************************************
 ****************************************************************************/

gint
main(gint argc, gchar *argv[])
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    g_thread_init(NULL);

    /* Initialize _osso. */
    _osso = osso_initialize("maemo_mapper", VERSION, TRUE, NULL);
    if(!_osso)
    {
        fprintf(stderr, "osso_initialize failed.\n");
        return 1;
    }

    gtk_init(&argc, &argv);

    /* Init gconf. */
    g_type_init();
    gconf_init(argc, argv, NULL);

    gnome_vfs_init();

    maemo_mapper_init(argc, argv);

    if(OSSO_OK != osso_rpc_set_default_cb_f(_osso, dbus_cb_default, NULL))
    {
        fprintf(stderr, "osso_rpc_set_default_cb_f failed.\n");
        return 1;
    }

    gtk_main();

    maemo_mapper_destroy();

    osso_deinitialize(_osso);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return 0;
}

/****************************************************************************
 * ABOVE: MAIN **************************************************************
 ****************************************************************************/



/****************************************************************************
 * BELOW: CALLBACKS *********************************************************
 ****************************************************************************/

static gint
dbus_cb_default(const gchar *interface, const gchar *method,
        GArray *arguments, gpointer data, osso_rpc_t *retval)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(!strcmp(method, "top_application"))
        gtk_idle_add((GSourceFunc)window_present, NULL);

    retval->type = DBUS_TYPE_INVALID;

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return OSSO_OK;
}

static void
osso_cb_hw_state(osso_hw_state_t *state, gpointer data)
{
    static gboolean _must_save_data = FALSE;
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(state->system_inactivity_ind)
    {
        if(_must_save_data)
            _must_save_data = FALSE;
        else if(_conn_state > RCVR_OFF)
        {
            set_conn_state(RCVR_OFF);
            rcvr_disconnect();
            track_add(0, 0, FALSE);
            /* Pretend the autoroute is in progress to avoid download. */
            if(_autoroute_data.enabled)
                _autoroute_data.in_progress = TRUE;
        }
    }
    else if(state->save_unsaved_data_ind)
    {
        config_save();
        _must_save_data = TRUE;
    }
    else if(_conn_state == RCVR_OFF && _enable_gps)
    {
        set_conn_state(RCVR_DOWN);
        rcvr_connect_later();
        if(_autoroute_data.enabled)
            _autoroute_data.in_progress = TRUE;
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

static gboolean
window_cb_key_press(GtkWidget* widget, GdkEventKey *event)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    switch (event->keyval)
    {
        case GDK_Up:
            map_pan(0, -PAN_UNITS);
            return TRUE;

        case GDK_Down:
            map_pan(0, PAN_UNITS);
            return TRUE;

        case GDK_Left:
            map_pan(-PAN_UNITS, 0);
            return TRUE;

        case GDK_Right:
            map_pan(PAN_UNITS, 0);
            return TRUE;

        case GDK_Return:
            switch(_center_mode)
            {
                case CENTER_LATLON:
                case CENTER_WAS_LEAD:
                    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(
                                _menu_ac_lead_item), TRUE);
                    break;
                case CENTER_LEAD:
                case CENTER_WAS_LATLON:
                    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(
                                _menu_ac_latlon_item), TRUE);
                    break;
            }
            return TRUE;

        case GDK_F6: /* the fullscreen button. */
            gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(
                        _menu_fullscreen_item), !_fullscreen);
            return TRUE;

        case GDK_F7: /* the zoom-in button. */
            map_set_zoom(_zoom - 1);
            return TRUE;

        case GDK_F8: /* the zoom-out button. */
            map_set_zoom(_zoom + 1);
            return TRUE;

        case GDK_Escape:
            {
                switch(_show_tracks)
                {
                    case 0:
                        /* Nothing shown, nothing saved; just set both. */
                        _show_tracks = TRACKS_MASK | ROUTES_MASK;
                        break;
                    case TRACKS_MASK << 16:
                    case ROUTES_MASK << 16:
                    case (ROUTES_MASK | TRACKS_MASK) << 16:
                        /* Something was saved and nothing changed since.
                         * Restore saved. */
                        _show_tracks = _show_tracks >> 16;
                        break;
                    default:
                        /* There is no history, or they changed something since
                         * the last historical change. Save and clear. */
                        _show_tracks = _show_tracks << 16;
                }

                gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(
                            _menu_show_routes_item),
                        _show_tracks & ROUTES_MASK);

                gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(
                            _menu_show_tracks_item),
                        _show_tracks & TRACKS_MASK);
            }
            return TRUE;
    }
    return FALSE;
}

static gboolean
map_cb_configure(GtkWidget *widget, GdkEventConfigure *event)
{
    printf("%s(%d, %d)\n", __PRETTY_FUNCTION__,
            _map_widget->allocation.width, _map_widget->allocation.height);

    _screen_width_pixels = _map_widget->allocation.width;
    _screen_height_pixels = _map_widget->allocation.height;
    _screen_grids_halfwidth = pixel2grid(_screen_width_pixels) / 2;
    _screen_grids_halfheight = pixel2grid(_screen_height_pixels) / 2;

    MACRO_RECALC_FOCUS_BASE();
    MACRO_RECALC_FOCUS_SIZE();

    _min_center.unitx = pixel2unit(grid2pixel(_screen_grids_halfwidth));
    _min_center.unity = pixel2unit(grid2pixel(_screen_grids_halfheight));
    _max_center.unitx = WORLD_SIZE_UNITS-grid2unit(_screen_grids_halfwidth) - 1;
    _max_center.unity = WORLD_SIZE_UNITS-grid2unit(_screen_grids_halfheight)- 1;

    map_center_unit(_center.unitx, _center.unity);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
map_cb_expose(GtkWidget *widget, GdkEventExpose *event)
{
    printf("%s(%d, %d, %d, %d)\n", __PRETTY_FUNCTION__,
            event->area.x, event->area.y,
            event->area.width, event->area.height);

    gdk_draw_drawable(
            _map_widget->window,
            _mark_current_gc,
            _map_pixmap,
            event->area.x + _offsetx, event->area.y + _offsety,
            event->area.x, event->area.y,
            event->area.width, event->area.height);
    map_draw_mark();

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
map_cb_button_release(GtkWidget *widget, GdkEventButton *event)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

#ifdef DEBUG
    if(event->button != 1)
    {
        _pos.unitx = x2unit((gint)(event->x + 0.5));
        _pos.unity = y2unit((gint)(event->y + 0.5));
        unit2latlon(_pos.unitx, _pos.unity, _pos_lat, _pos_lon);
        _speed = 20.f;
        integerize_data();
        track_add(_pos.unitx, _pos.unity, FALSE);
        refresh_mark();
    }
    else
#endif
    {
        if(_center_mode > 0)
            gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(
                        _menu_ac_none_item), TRUE);
        map_center_unit(
                x2unit((gint)(event->x + 0.5)),
                y2unit((gint)(event->y + 0.5)));
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
channel_cb_error(GIOChannel *src, GIOCondition condition, gpointer data)
{
    printf("%s(%d)\n", __PRETTY_FUNCTION__, condition);

    /* An error has occurred - re-connect(). */
    rcvr_disconnect();
    track_add(0, 0, FALSE);

    if(_fd != -1)
        /* Attempt to reset the radio if user has sudo access. */
        system("/usr/bin/sudo -l | grep -q '/usr/sbin/hciconfig  *hci0  *reset'"
                " && sudo /usr/sbin/hciconfig hci0 reset");

    if(_conn_state > RCVR_OFF)
    {
        set_conn_state(RCVR_DOWN);
        rcvr_connect_now();
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return FALSE;
}

static gboolean
channel_cb_connect(GIOChannel *src, GIOCondition condition, gpointer data)
{
    int error, size = sizeof(error);
    printf("%s(%d)\n", __PRETTY_FUNCTION__, condition);

    if(*_rcvr_mac != '/'
            && (getsockopt(_fd, SOL_SOCKET, SO_ERROR, &error, &size) || error))
    {
        printf("%s(): Error connecting to receiver; retrying...\n",
                __PRETTY_FUNCTION__);
        /* Try again. */
        rcvr_disconnect();
        rcvr_connect_later();
    }
    else
    {
        printf("%s(): Connected to receiver!\n",
                __PRETTY_FUNCTION__);
        set_conn_state(RCVR_UP);
        _input_sid = g_io_add_watch_full(_channel, G_PRIORITY_HIGH_IDLE,
                G_IO_IN | G_IO_PRI, channel_cb_input, NULL, NULL);
    }

    _connect_sid = 0;
    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return FALSE;
}

#define MACRO_PARSE_INT(tofill, str) { \
    gchar *error_check; \
    (tofill) = strtol((str), &error_check, 10); \
    if(error_check == (str)) \
    { \
        fprintf(stderr, "Failed to parse string as int: %s\n", str); \
        hildon_banner_show_information(_window, NULL, \
                "Invalid NMEA input from receiver!"); \
        return; \
    } \
}
#define MACRO_PARSE_FLOAT(tofill, str) { \
    gchar *error_check; \
    (tofill) = g_ascii_strtod((str), &error_check); \
    if(error_check == (str)) \
    { \
        fprintf(stderr, "Failed to parse string as float: %s\n", str); \
        hildon_banner_show_information(_window, NULL, \
                "Invalid NMEA input from receiver!"); \
        return; \
    } \
}
static void
channel_parse_rmc(gchar *sentence)
{
    /* Recommended Minimum Navigation Information C
     *  1) UTC Time
     *  2) Status, V=Navigation receiver warning A=Valid
     *  3) Latitude
     *  4) N or S
     *  5) Longitude
     *  6) E or W
     *  7) Speed over ground, knots
     *  8) Track made good, degrees true
     *  9) Date, ddmmyy
     * 10) Magnetic Variation, degrees
     * 11) E or W
     * 12) FAA mode indicator (NMEA 2.3 and later)
     * 13) Checksum
     */
    gchar *token, *dpoint;
    gdouble tmpd;
    guint tmpi;
    gboolean newly_fixed = FALSE;
    vprintf("%s(): %s", __PRETTY_FUNCTION__, sentence);

#define DELIM ","

    /* Skip time. */
    token = strsep(&sentence, DELIM);

    token = strsep(&sentence, DELIM);
    /* Token is now Status. */
    if(*token != 'A')
    {
        /* Data is invalid - not enough satellites?. */
        if(_conn_state != RCVR_UP)
        {
            set_conn_state(RCVR_UP);
            track_add(0, 0, FALSE);
        }
    }
    else
    {
        /* Data is valid. */
        if(_conn_state != RCVR_FIXED)
        {
            newly_fixed = TRUE;
            set_conn_state(RCVR_FIXED);
        }
    }

    /* Parse the latitude. */
    token = strsep(&sentence, DELIM);
    if(*token)
    {
        dpoint = strchr(token, '.');
        MACRO_PARSE_FLOAT(tmpd, dpoint - 2);
        dpoint[-2] = '\0';
        MACRO_PARSE_INT(tmpi, token);
        _pos_lat = tmpi + (tmpd * (1.0 / 60.0));
    }

    /* Parse N or S. */
    token = strsep(&sentence, DELIM);
    if(*token && token[0] == 'S')
        _pos_lat = -_pos_lat;

    /* Parse the longitude. */
    token = strsep(&sentence, DELIM);
    if(*token)
    {
        dpoint = strchr(token, '.');
        MACRO_PARSE_FLOAT(tmpd, dpoint - 2);
        dpoint[-2] = '\0';
        MACRO_PARSE_INT(tmpi, token);
        _pos_lon = tmpi + (tmpd * (1.0 / 60.0));
    }

    /* Parse E or W. */
    token = strsep(&sentence, DELIM);
    if(*token && token[0] == 'W')
        _pos_lon = -_pos_lon;

    /* Parse speed over ground, knots. */
    token = strsep(&sentence, DELIM);
    if(*token)
        MACRO_PARSE_FLOAT(_speed, token);

    /* Parse heading, degrees from true north. */
    token = strsep(&sentence, DELIM);
    if(*token)
        MACRO_PARSE_FLOAT(_heading, token);

    /* Translate data into integers. */
    integerize_data();

    /* Add new data to track. */
    if(_conn_state == RCVR_FIXED)
        track_add(_pos.unitx, _pos.unity, newly_fixed);

    /* Move mark to new location. */
    refresh_mark();

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

static void
channel_parse_gsv(gchar *sentence)
{
    /* Must be GSV - Satellites in view
     *  1) total number of messages
     *  2) message number
     *  3) satellites in view
     *  4) satellite number
     *  5) elevation in degrees (0-90)
     *  6) azimuth in degrees to true north (0-359)
     *  7) SNR in dB (0-99)
     *  more satellite infos like 4)-7)
     *  n) checksum
     */
    gchar *token;
    guint msgcnt, nummsgs;
    static guint running_total = 0;
    static guint num_sats_used = 0;
    vprintf("%s(): %s", __PRETTY_FUNCTION__, sentence);

    /* Parse number of messages. */
    token = strsep(&sentence, DELIM);
    if(!sentence)
        return; /* because this is an invalid sentence. */
    MACRO_PARSE_INT(nummsgs, token);

    /* Parse message number. */
    token = strsep(&sentence, DELIM);
    if(!sentence)
        return; /* because this is an invalid sentence. */
    MACRO_PARSE_INT(msgcnt, token);

    /* Skip number of satellites in view. */
    token = strsep(&sentence, DELIM);

    /* Loop until there are no more satellites to parse. */
    while(sentence)
    {
        guint snr;
        /* Skip satellite number. */
        token = strsep(&sentence, DELIM);
        if(!sentence)
            break; /* because this is an invalid sentence. */
        /* Skip elevation in degrees (0-90). */
        token = strsep(&sentence, DELIM);
        if(!sentence)
            break; /* because this is an invalid sentence. */
        /* Skip azimuth in degrees to true north (0-359). */
        token = strsep(&sentence, DELIM);
        if(!sentence)
            break; /* because this is an invalid sentence. */

        /* Parse SNR. */
        token = strsep(&sentence, DELIM);
        if((snr = atoi(token)))
        {
            /* Snr is non-zero - add to total and count as used. */
            running_total += snr;
            num_sats_used++;
        }
    }

    if(msgcnt == nummsgs)
    {
        /*  This is the last message. Calculate signal strength. */
        if(num_sats_used)
        {
            gdouble fraction = running_total * sqrt(num_sats_used)
                / num_sats_used / 100.0;
            BOUND(fraction, 0.0, 1.0);
            running_total = 0;
            num_sats_used = 0;
            hildon_banner_set_fraction(HILDON_BANNER(_fix_banner), fraction);
        }

        /* Keep awake while they watch the progress bar. */
        KEEP_DISPLAY_ON();
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

static gboolean
channel_cb_input(GIOChannel *src, GIOCondition condition, gpointer data)
{
    gchar *sentence;
    vprintf("%s(%d)\n", __PRETTY_FUNCTION__, condition);
    
    while(G_IO_STATUS_NORMAL == g_io_channel_read_line(
                _channel,
                &sentence,
                NULL,
                NULL,
                NULL) && sentence)
    {
        gchar *sptr = sentence + 1;
        guint csum = 0;
        while(*sptr && *sptr != '*')
            csum ^= *sptr++;
        if (!*sptr || csum == strtol(sptr+1, NULL, 16))
        {
            if(*sptr)
                *sptr = '\0'; /* take checksum out of the sentence. */
            if(!strncmp(sentence + 3, "RMC", 3))
                channel_parse_rmc(sentence + 7);
            else if(_conn_state == RCVR_UP
                    && !strncmp(sentence + 3, "GSV", 3))
                channel_parse_gsv(sentence + 7);
        }
        else
        {
            printf("%s: Bad checksum in NMEA sentence:\n%s\n",
                    __PRETTY_FUNCTION__, sentence);
        }
        g_free(sentence);
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
gps_toggled_from(GtkToggleButton *chk_gps,
        GtkWidget *txt_from)
{
    gchar buffer[1024];
    printf("%s()\n", __PRETTY_FUNCTION__);

    sprintf(buffer, "%f, %f", _pos_lat, _pos_lon);
    gtk_widget_set_sensitive(txt_from, !gtk_toggle_button_get_active(chk_gps));
    gtk_entry_set_text(GTK_ENTRY(txt_from), buffer);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
gps_toggled_auto(GtkToggleButton *chk_gps,
        GtkWidget *chk_auto)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    gtk_widget_set_sensitive(chk_auto, gtk_toggle_button_get_active(chk_gps));

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_route_download(GtkAction *action)
{
    GtkWidget *dialog;
    GtkWidget *table;
    GtkWidget *label;
    GtkWidget *chk_gps;
    GtkWidget *chk_auto;
    GtkWidget *txt_from;
    GtkWidget *txt_to;
    GtkWidget *hbox;
    GtkEntryCompletion *from_comp;
    GtkEntryCompletion *to_comp;
    gchar *bytes = NULL;
    gint size;
    printf("%s()\n", __PRETTY_FUNCTION__);

    dialog = gtk_dialog_new_with_buttons("Download Route",
            GTK_WINDOW(_window), GTK_DIALOG_MODAL,
            GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
            GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
            NULL);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
            table = gtk_table_new(2, 4, FALSE), TRUE, TRUE, 0);

    from_comp = gtk_entry_completion_new();
    gtk_entry_completion_set_model(from_comp, GTK_TREE_MODEL(_loc_model));
    gtk_entry_completion_set_text_column(from_comp, 0);
    to_comp = gtk_entry_completion_new();
    gtk_entry_completion_set_model(to_comp, GTK_TREE_MODEL(_loc_model));
    gtk_entry_completion_set_text_column(to_comp, 0);

    /* Auto. */
    gtk_table_attach(GTK_TABLE(table),
            hbox = gtk_hbox_new(FALSE, 6),
            0, 2, 0, 1, 0, 0, 2, 4);
    gtk_box_pack_start(GTK_BOX(hbox),
            chk_gps = gtk_check_button_new_with_label(
                "Use GPS Location"),
            TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox),
            chk_auto = gtk_check_button_new_with_label(
                "Auto-Update"),
            TRUE, TRUE, 0);
    gtk_widget_set_sensitive(chk_auto, FALSE);

    /* Origin. */
    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new("Origin"),
            0, 1, 1, 2, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(table),
            txt_from = gtk_entry_new(),
            1, 2, 1, 2, GTK_EXPAND | GTK_FILL, 0, 2, 4);
    gtk_entry_set_completion(GTK_ENTRY(txt_from), from_comp);
    gtk_entry_set_width_chars(GTK_ENTRY(txt_from), 25);

    /* Destination. */
    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new("Destination"),
            0, 1, 2, 3, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(table),
            txt_to = gtk_entry_new(),
            1, 2, 2, 3, GTK_EXPAND | GTK_FILL, 0, 2, 4);
    gtk_entry_set_completion(GTK_ENTRY(txt_to), to_comp);
    gtk_entry_set_width_chars(GTK_ENTRY(txt_to), 25);

    g_signal_connect(G_OBJECT(chk_gps), "toggled",
                      G_CALLBACK(gps_toggled_from), txt_from);
    g_signal_connect(G_OBJECT(chk_gps), "toggled",
                      G_CALLBACK(gps_toggled_auto), chk_auto);

    /* Initialize fields. */
    gtk_entry_set_text(GTK_ENTRY(txt_from), "");
    gtk_entry_set_text(GTK_ENTRY(txt_to), "");

    gtk_widget_show_all(dialog);

    while(!bytes
            && GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(dialog)))
    {
        GnomeVFSResult vfs_result;
        gchar buffer[1024];
        const gchar *from, *to;
        gchar *from_escaped, *to_escaped;

        from = gtk_entry_get_text(GTK_ENTRY(txt_from));
        if(!strlen(from))
        {
            popup_error("Please specify a start location.");
            continue;
        }

        to = gtk_entry_get_text(GTK_ENTRY(txt_to));
        if(!strlen(to))
        {
            popup_error("Please specify an end location.");
            continue;
        }

        from_escaped = gnome_vfs_escape_string(from);
        to_escaped = gnome_vfs_escape_string(to);
        sprintf(buffer, "http://www.gnuite.com/cgi-bin/gpx.cgi?"
                "saddr=%s&daddr=%s",
                from_escaped, to_escaped);
        g_free(from_escaped);
        g_free(to_escaped);

        if(GNOME_VFS_OK != (vfs_result = gnome_vfs_read_entire_file(
                        buffer, &size, &bytes)))
        {
            gchar buffer[1024];
            sprintf(buffer, "Failed to connect to GPX Directions server.\n%s",
                    gnome_vfs_result_to_string(vfs_result));
            popup_error(buffer);
        }
        else if(strncmp(bytes, "<?xml", strlen("<?xml")))
        {
            /* Not an XML document - must be bad locations. */
            popup_error("Could not generate directions. Make sure your "
                    "locations are valid.");
            g_free(bytes);
            bytes = NULL;
        }
        else
        {
            /* If GPS is enabled, append the route, otherwise replace it. */
            if(parse_gpx(bytes, size, &_route,
                    (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(chk_gps))
                        ? 0 : 1), 0))
            {
                GtkTreeIter iter;

                /* Find the nearest route point, if we're connected. */
                route_find_nearest_point();

                /* Cancel any autoroute that might be occurring. */
                cancel_autoroute();

                map_force_redraw();

                if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(chk_auto)))
                {
                    /* Kick off a timeout to start the first update. */
                    _autoroute_data.dest = gnome_vfs_escape_string(to);
                    _autoroute_data.enabled = TRUE;
                    _autoroute_data.bytes_maxsize = 2 * size;
                    _autoroute_data.bytes = g_new(gchar, 2 * size);
                }

                /* Save Origin in Route Locations list if not from GPS. */
                if(!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(chk_gps))
                    && !g_slist_find_custom(_loc_list, from,
                                (GCompareFunc)strcmp))
                {
                    _loc_list = g_slist_prepend(_loc_list, g_strdup(from));
                    gtk_list_store_insert_with_values(_loc_model, &iter,
                            INT_MAX, 0, from, -1);
                }

                /* Save Destination in Route Locations list. */
                if(!g_slist_find_custom(_loc_list, to,
                            (GCompareFunc)strcmp))
                {
                    _loc_list = g_slist_prepend(_loc_list, g_strdup(to));
                    gtk_list_store_insert_with_values(_loc_model, &iter,
                            INT_MAX, 0, to, -1);
                }

                hildon_banner_show_information(_window, NULL,
                        "Route Downloaded");
            }
            else
                popup_error("Error parsing GPX file.");
            g_free(bytes);
        }
    }

    gtk_widget_destroy(dialog);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_route_open(GtkAction *action)
{
    gchar *buffer;
    gint size;
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(open_file(&buffer, NULL, &size, &_route_dir_uri, NULL,
                    GTK_FILE_CHOOSER_ACTION_OPEN))
    {
        /* If auto is enabled, append the route, otherwise replace it. */
        if(parse_gpx(buffer, size, &_route, _autoroute_data.enabled ? 0 : 1, 0))
        {
            cancel_autoroute();

            /* Find the nearest route point, if we're connected. */
            route_find_nearest_point();

            map_force_redraw();
            hildon_banner_show_information(_window, NULL,
                    "Route Opened");
        }
        else
            popup_error("Error parsing GPX file.");
        g_free(buffer);
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_route_reset(GtkAction *action)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    route_find_nearest_point();
    map_render_tracks();
    MACRO_QUEUE_DRAW_AREA();

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_route_clear(GtkAction *action)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    _next_way_dist_rough = -1; /* to avoid announcement attempts. */
    cancel_autoroute();
    MACRO_CLEAR_TRACK(_route);
    map_force_redraw();

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_track_open(GtkAction *action)
{
    gchar *buffer;
    gint size;
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(open_file(&buffer, NULL, &size, NULL, &_track_file_uri,
                    GTK_FILE_CHOOSER_ACTION_OPEN))
    {
        if(parse_gpx(buffer, size, &_track, -1, ARRAY_CHUNK_SIZE))
        {
            map_force_redraw();
            hildon_banner_show_information(_window, NULL, "Track Opened");
        }
        else
            popup_error("Error parsing GPX file.");
        g_free(buffer);
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return TRUE;
}

#define WRITE_STRING(string) { \
    GnomeVFSResult vfs_result; \
    GnomeVFSFileSize size; \
    if(GNOME_VFS_OK != (vfs_result = gnome_vfs_write( \
                    handle, (string), strlen((string)), &size))) \
    { \
        gchar buffer[1024]; \
        sprintf(buffer, "Error while writing to file:\n%s\n" \
                        "File is incomplete.", \
                gnome_vfs_result_to_string(vfs_result)); \
        popup_error(buffer); \
        return FALSE; \
    } \
}

static gboolean
write_gpx(GnomeVFSHandle *handle, Track *track)
{
    gboolean last_was_way = FALSE;
    printf("%s()\n", __PRETTY_FUNCTION__);

    /* Write the header. */
    WRITE_STRING("<?xml version=\"1.0\"?>\n"
                    "<gpx version=\"1.0\" creator=\"Maemo Mapper\" "
                    "xmlns=\"http://www.topografix.com/GPX/1/0\">\n"
                    "  <trk>\n"
                    "    <trkseg>\n");

    /* Iterate through the track and write the points to the file. */
    {
        TrackPoint *curr;
        WayPoint *wcurr;
        gboolean trkseg_break = FALSE;

        /* Find first non-zero point. */
        for(curr = track->head-1, wcurr = track->whead; curr++ != track->tail; )
            if(curr->unity != 0)
                break;
            else if(curr == wcurr->point)
                wcurr++;

        /* Curr points to first non-zero point. */
        for(curr--; curr++ != track->tail; )
        {
            gfloat lat, lon;
            if(curr->unity != 0)
            {
                gchar buffer[80];
                gchar strlat[80], strlon[80];
                if(trkseg_break)
                {
                    /* First trkpt of the segment - write trkseg header. */
                    WRITE_STRING("    </trkseg>\n");
                    /* Write trkseg header. */
                    WRITE_STRING("    <trkseg>\n");
                    trkseg_break = FALSE;
                }
                unit2latlon(curr->unitx, curr->unity, lat, lon);
                g_ascii_formatd(strlat, 80, "%.6f", lat);
                g_ascii_formatd(strlon, 80, "%.6f", lon);
                sprintf(buffer, "      <trkpt lat=\"%s\" lon=\"%s\"",
                        strlat, strlon);
                if(curr == wcurr->point)
                {
                    sprintf(buffer + strlen(buffer),
                            "><desc>%s</desc></trkpt>\n", wcurr++->desc);
                    last_was_way = TRUE;
                }
                else
                {
                    strcat(buffer, "/>\n");
                    last_was_way = FALSE;
                }
                WRITE_STRING(buffer);
            }
            else
                trkseg_break = TRUE;
        }
    }

    /* Write the footer. */
    WRITE_STRING("    </trkseg>\n  </trk>\n</gpx>\n");

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_track_save(GtkAction *action)
{
    GnomeVFSHandle *handle;
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(open_file(NULL, &handle, NULL, NULL, &_track_file_uri,
                    GTK_FILE_CHOOSER_ACTION_SAVE))
    {
        if(write_gpx(handle, &_track))
            hildon_banner_show_information(_window, NULL, "Track Saved");
        else
            popup_error("Error writing GPX file.");
        gnome_vfs_close(handle);
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_track_mark_way(GtkAction *action)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    MACRO_TRACK_INCREMENT_WTAIL(_track);
    if(_track.tail->unity != 0)
    {
        guint x1, y1;
        _track.wtail->point = _track.tail;
        _track.wtail->desc = g_strdup("Track Waypoint");
        
        /** Instead of calling map_render_tracks(), we'll just add the waypoint
         * ourselves. */
        x1 = unit2bufx(_track.tail->unitx);
        y1 = unit2bufy(_track.tail->unity);
        /* Make sure this circle will be visible. */
        if((x1 < BUF_WIDTH_PIXELS)
                && ((unsigned)y1 < BUF_HEIGHT_PIXELS))
            gdk_draw_arc(_map_pixmap, _track_way_gc,
                    FALSE, /* FALSE: not filled. */
                    x1 - _draw_line_width,
                    y1 - _draw_line_width,
                    2 * _draw_line_width,
                    2 * _draw_line_width,
                    0, /* start at 0 degrees. */
                    360 * 64);
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_route_save(GtkAction *action)
{
    GnomeVFSHandle *handle;
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(open_file(NULL, &handle, NULL, &_route_dir_uri, NULL,
                    GTK_FILE_CHOOSER_ACTION_SAVE))
    {
        if(write_gpx(handle, &_route))
            hildon_banner_show_information(_window, NULL, "Route Saved");
        else
            popup_error("Error writing GPX file.");
        gnome_vfs_close(handle);
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_track_clear(GtkAction *action)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    _track.tail = _track.head;
    _track.wtail = _track.whead;
    map_force_redraw();

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_show_tracks(GtkAction *action)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    _show_tracks ^= TRACKS_MASK;
    if(gtk_check_menu_item_get_active(
                GTK_CHECK_MENU_ITEM(_menu_show_tracks_item)))
    {
        _show_tracks |= TRACKS_MASK;
        map_render_tracks();
        MACRO_QUEUE_DRAW_AREA();
        hildon_banner_show_information(_window, NULL, "Tracks are now shown");
    }
    else
    {
        _show_tracks &= ~TRACKS_MASK;
        map_force_redraw();
        hildon_banner_show_information(_window, NULL, "Tracks are now hidden");
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_show_routes(GtkAction *action)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(gtk_check_menu_item_get_active(
                GTK_CHECK_MENU_ITEM(_menu_show_routes_item)))
    {
        _show_tracks |= ROUTES_MASK;
        map_render_tracks();
        MACRO_QUEUE_DRAW_AREA();
        hildon_banner_show_information(_window, NULL, "Routes are now shown");
    }
    else
    {
        _show_tracks &= ~ROUTES_MASK;
        map_force_redraw();
        hildon_banner_show_information(_window, NULL, "Routes are now hidden");
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_show_velvec(GtkAction *action)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    _show_velvec = gtk_check_menu_item_get_active(
            GTK_CHECK_MENU_ITEM(_menu_show_velvec_item));
    map_move_mark();

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_ac_lead(GtkAction *action)
{
    guint new_center_unitx, new_center_unity;
    printf("%s()\n", __PRETTY_FUNCTION__);

    _center_mode = CENTER_LEAD;
    hildon_banner_show_information(_window, NULL, "Auto-Center Mode: Lead");
    MACRO_RECALC_CENTER(new_center_unitx, new_center_unity);
    map_center_unit(new_center_unitx, new_center_unity);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_ac_latlon(GtkAction *action)
{
    guint new_center_unitx, new_center_unity;
    printf("%s()\n", __PRETTY_FUNCTION__);

    _center_mode = CENTER_LATLON;
    hildon_banner_show_information(_window, NULL, "Auto-Center Mode: Lat/Lon");
    MACRO_RECALC_CENTER(new_center_unitx, new_center_unity);
    map_center_unit(new_center_unitx, new_center_unity);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_ac_none(GtkAction *action)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    _center_mode = -_center_mode;
    hildon_banner_show_information(_window, NULL, "Auto-Center Off");

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_maps_dlroute(GtkAction *action)
{
    guint last_tilex, last_tiley;
    TrackPoint *curr;
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(!_route.head)
        return TRUE;

    last_tilex = 0;
    last_tiley = 0;
    for(curr = _route.head - 1; ++curr != _route.tail; )
    {
        if(curr->unity)
        {
            guint tilex = unit2tile(curr->unitx);
            guint tiley = unit2tile(curr->unity);
            if(tilex != last_tilex || tiley != last_tiley)
            {
                guint minx, miny, maxx, maxy, x, y;
                if(last_tiley != 0)
                {
                    minx = MIN(tilex, last_tilex) - 2;
                    miny = MIN(tiley, last_tiley) - 1;
                    maxx = MAX(tilex, last_tilex) + 2;
                    maxy = MAX(tiley, last_tiley) + 1;
                }
                else
                {
                    minx = tilex - 2;
                    miny = tiley - 1;
                    maxx = tilex + 2;
                    maxy = tiley + 1;
                }
                for(x = minx; x <= maxx; x++)
                    for(y = miny; y <= maxy; y++)
                    {
                        gchar buffer[1024];
                        sprintf(buffer, "%s/%u/%u/%u.jpg",
                            _map_dir_name, (_zoom - 1), x, y);
                        map_initiate_download(buffer, x, y, _zoom);
                    }
                last_tilex = tilex;
                last_tiley = tiley;
            }
        }
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return TRUE;
}

typedef struct _DlAreaInfo DlAreaInfo;
struct _DlAreaInfo {
    GtkWidget *notebook;
    GtkWidget *txt_topleft_lat;
    GtkWidget *txt_topleft_lon;
    GtkWidget *txt_botright_lat;
    GtkWidget *txt_botright_lon;
    GtkWidget *chk_zoom_levels[MAX_ZOOM];
};

static void dlarea_clear(GtkWidget *widget, DlAreaInfo *dlarea_info)
{
    guint i;
    printf("%s()\n", __PRETTY_FUNCTION__);
    if(gtk_notebook_get_current_page(GTK_NOTEBOOK(dlarea_info->notebook)))
        /* This is the second page (the "Zoom" page) - clear the checks. */
        for(i = 0; i < MAX_ZOOM; i++)
            gtk_toggle_button_set_active(
                    GTK_TOGGLE_BUTTON(dlarea_info->chk_zoom_levels[i]), FALSE);
    else
    {
        /* This is the first page (the "Area" page) - clear the text fields. */
        gtk_entry_set_text(GTK_ENTRY(dlarea_info->txt_topleft_lat), "");
        gtk_entry_set_text(GTK_ENTRY(dlarea_info->txt_topleft_lon), "");
        gtk_entry_set_text(GTK_ENTRY(dlarea_info->txt_botright_lat), "");
        gtk_entry_set_text(GTK_ENTRY(dlarea_info->txt_botright_lon), "");
    }
    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

static gboolean
menu_cb_maps_dlarea(GtkAction *action)
{
    GtkWidget *dialog;
    GtkWidget *table;
    GtkWidget *label;
    GtkWidget *button;
    GtkWidget *lbl_gps_lat;
    GtkWidget *lbl_gps_lon;
    GtkWidget *lbl_center_lat;
    GtkWidget *lbl_center_lon;
    DlAreaInfo dlarea_info;
    gchar buffer[80];
    gfloat lat, lon, prev_lat, prev_lon;
    guint i;
    printf("%s()\n", __PRETTY_FUNCTION__);

    dialog = gtk_dialog_new_with_buttons("Download Maps by Area",
            GTK_WINDOW(_window), GTK_DIALOG_MODAL,
            GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
            GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
            NULL);

    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area),
            button = gtk_button_new_with_label("Clear"));

    g_signal_connect(G_OBJECT(button), "clicked",
                      G_CALLBACK(dlarea_clear), &dlarea_info);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
            dlarea_info.notebook = gtk_notebook_new(), TRUE, TRUE, 0);
    
    gtk_notebook_append_page(GTK_NOTEBOOK(dlarea_info.notebook),
            table = gtk_table_new(2, 3, FALSE),
            label = gtk_label_new("Area"));

    /* Clear button and Label Columns. */
    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new("Latitude"),
            1, 2, 0, 1, GTK_FILL, 0, 4, 0);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new("Longitude"),
            2, 3, 0, 1, GTK_FILL, 0, 4, 0);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);

    /* GPS. */
    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new("GPS Location"),
            0, 1, 1, 2, GTK_FILL, 0, 4, 0);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(table),
            lbl_gps_lat = gtk_label_new(""),
            1, 2, 1, 2, GTK_FILL, 0, 4, 0);
    gtk_label_set_selectable(GTK_LABEL(lbl_gps_lat), TRUE);
    gtk_misc_set_alignment(GTK_MISC(lbl_gps_lat), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(table),
            lbl_gps_lon = gtk_label_new(""),
            2, 3, 1, 2, GTK_FILL, 0, 4, 0);
    gtk_label_set_selectable(GTK_LABEL(lbl_gps_lon), TRUE);
    gtk_misc_set_alignment(GTK_MISC(lbl_gps_lon), 1.f, 0.5f);

    /* Center. */
    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new("View Center"),
            0, 1, 2, 3, GTK_FILL, 0, 4, 0);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    unit2latlon(_center.unitx, _center.unity, lat, lon);
    unit2latlon(_prev_center.unitx, _prev_center.unity, prev_lat, prev_lon);
    gtk_table_attach(GTK_TABLE(table),
            lbl_center_lat = gtk_label_new(""),
            1, 2, 2, 3, GTK_FILL, 0, 4, 0);
    gtk_label_set_selectable(GTK_LABEL(lbl_center_lat), TRUE);
    gtk_misc_set_alignment(GTK_MISC(lbl_center_lat), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(table),
            lbl_center_lon = gtk_label_new(""),
            2, 3, 2, 3, GTK_FILL, 0, 4, 0);
    gtk_label_set_selectable(GTK_LABEL(lbl_center_lon), TRUE);
    gtk_misc_set_alignment(GTK_MISC(lbl_center_lon), 1.f, 0.5f);

    /* default values for Top Left and Bottom Right are defined by the
     * rectangle of the current and the previous Center */

    /* Top Left. */
    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new("Top-Left"),
            0, 1, 3, 4, GTK_FILL, 0, 4, 0);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(table),
            dlarea_info.txt_topleft_lat = gtk_entry_new(),
            1, 2, 3, 4, GTK_EXPAND | GTK_FILL, 0, 4, 0);
    sprintf(buffer, "%2.6f", MAX(lat, prev_lat));
    gtk_entry_set_alignment(GTK_ENTRY(dlarea_info.txt_topleft_lat), 1.f);
    gtk_table_attach(GTK_TABLE(table),
            dlarea_info.txt_topleft_lon = gtk_entry_new(),
            2, 3, 3, 4, GTK_EXPAND | GTK_FILL, 0, 4, 0);
    sprintf(buffer, "%2.6f", MIN(lon, prev_lon));
    gtk_entry_set_alignment(GTK_ENTRY(dlarea_info.txt_topleft_lon), 1.f);

    /* Bottom Right. */
    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new("Bottom-Right"),
            0, 1, 4, 5, GTK_FILL, 0, 4, 0);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(table),
            dlarea_info.txt_botright_lat = gtk_entry_new(),
            1, 2, 4, 5, GTK_EXPAND | GTK_FILL, 0, 4, 0);
    sprintf(buffer, "%2.6f", MIN(lat, prev_lat));
    gtk_entry_set_alignment(GTK_ENTRY(dlarea_info.txt_botright_lat), 1.f);
    gtk_table_attach(GTK_TABLE(table),
            dlarea_info.txt_botright_lon = gtk_entry_new(),
            2, 3, 4, 5, GTK_EXPAND | GTK_FILL, 0, 4, 0);
    sprintf(buffer, "%2.6f", MAX(lon, prev_lon));
    gtk_entry_set_alignment(GTK_ENTRY(dlarea_info.txt_botright_lon), 1.f);


    gtk_notebook_append_page(GTK_NOTEBOOK(dlarea_info.notebook),
            table = gtk_table_new(5, 5, FALSE),
            label = gtk_label_new("Zoom"));
    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new(
                "Zoom Levels to Download: (0 -> most detail)"),
            0, 5, 0, 1, GTK_FILL, 0, 4, 0);
    gtk_misc_set_alignment(GTK_MISC(label), 0.f, 0.5f);
    for(i = 0; i < MAX_ZOOM; i++)
    {
        sprintf(buffer, "%d", i);
        gtk_table_attach(GTK_TABLE(table),
                dlarea_info.chk_zoom_levels[i]
                        = gtk_check_button_new_with_label(buffer),
                i % 5, i % 5 + 1, i / 5 + 1, i / 5 + 2,
                GTK_EXPAND | GTK_FILL, 0, 4, 0);
    }

    /* Initialize fields. */
    sprintf(buffer, "%f", _pos_lat);
    gtk_label_set_text(GTK_LABEL(lbl_gps_lat), buffer);
    sprintf(buffer, "%f", _pos_lon);
    gtk_label_set_text(GTK_LABEL(lbl_gps_lon), buffer);
    sprintf(buffer, "%f", lat);
    gtk_label_set_text(GTK_LABEL(lbl_center_lat), buffer);
    sprintf(buffer, "%f", lon);
    gtk_label_set_text(GTK_LABEL(lbl_center_lon), buffer);

    /* Zoom check boxes. */
    gtk_entry_set_text(GTK_ENTRY(dlarea_info.txt_topleft_lat), buffer);
    gtk_entry_set_text(GTK_ENTRY(dlarea_info.txt_topleft_lon), buffer);
    gtk_entry_set_text(GTK_ENTRY(dlarea_info.txt_botright_lat), buffer);
    gtk_entry_set_text(GTK_ENTRY(dlarea_info.txt_botright_lon), buffer);
    for(i = 0; i < MAX_ZOOM; i++)
        gtk_toggle_button_set_active(
            GTK_TOGGLE_BUTTON(dlarea_info.chk_zoom_levels[_zoom - 1]), FALSE);
    gtk_toggle_button_set_active(
            GTK_TOGGLE_BUTTON(dlarea_info.chk_zoom_levels[_zoom - 1]), TRUE);

    gtk_widget_show_all(dialog);

    while(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(dialog)))
    {
        const gchar *text;
        gchar *error_check;
        gfloat start_lat, start_lon, end_lat, end_lon;
        guint start_unitx, start_unity, end_unitx, end_unity;
        guint num_maps = 0;
        GtkWidget *confirm;

        text = gtk_entry_get_text(GTK_ENTRY(dlarea_info.txt_topleft_lat));
        start_lat = strtof(text, &error_check);
        if(text == error_check) {
            popup_error("Invalid Top-Left Latitude");
            continue;
        }

        text = gtk_entry_get_text(GTK_ENTRY(dlarea_info.txt_topleft_lon));
        start_lon = strtof(text, &error_check);
        if(text == error_check) {
            popup_error("Invalid Top-Left Longitude");
            continue;
        }

        text = gtk_entry_get_text(GTK_ENTRY(dlarea_info.txt_botright_lat));
        end_lat = strtof(text, &error_check);
        if(text == error_check) {
            popup_error("Invalid Bottom-Right Latitude");
            continue;
        }

        text = gtk_entry_get_text(GTK_ENTRY(dlarea_info.txt_botright_lon));
        end_lon = strtof(text, &error_check);
        if(text == error_check) {
            popup_error("Invalid Bottom-Right Longitude");
            continue;
        }

        latlon2unit(start_lat, start_lon, start_unitx, start_unity);
        latlon2unit(end_lat, end_lon, end_unitx, end_unity);

        /* Swap if they specified flipped lats or lons. */
        if(start_unitx > end_unitx)
        {
            guint swap = start_unitx;
            start_unitx = end_unitx;
            end_unitx = swap;
        }
        if(start_unity > end_unity)
        {
            guint swap = start_unity;
            start_unity = end_unity;
            end_unity = swap;
        }

        /* First, get the number of maps to download. */
        for(i = 0; i < MAX_ZOOM; i++)
        {
            if(gtk_toggle_button_get_active(
                        GTK_TOGGLE_BUTTON(dlarea_info.chk_zoom_levels[i])))
            {
                guint start_tilex, start_tiley, end_tilex, end_tiley;
                start_tilex = unit2ztile(start_unitx, i + 1);
                start_tiley = unit2ztile(start_unity, i + 1);
                end_tilex = unit2ztile(end_unitx, i + 1);
                end_tiley = unit2ztile(end_unity, i + 1);
                num_maps += (end_tilex - start_tilex + 1)
                    * (end_tiley - start_tiley + 1);
            }
        }
        text = gtk_entry_get_text(GTK_ENTRY(dlarea_info.txt_topleft_lat));

        sprintf(buffer, "Confirm download of %d maps\n(up to about %.2f MB)\n",
                num_maps,
                num_maps * (strstr(_map_uri_format, "%s") ? 18e-3 : 6e-3));
        text = gtk_entry_get_text(GTK_ENTRY(dlarea_info.txt_topleft_lat));
        confirm = hildon_note_new_confirmation(GTK_WINDOW(dialog), buffer);
        text = gtk_entry_get_text(GTK_ENTRY(dlarea_info.txt_topleft_lat));

        if(GTK_RESPONSE_OK == gtk_dialog_run(GTK_DIALOG(confirm)))
        {
            for(i = 0; i < MAX_ZOOM; i++)
            {
                if(gtk_toggle_button_get_active(
                            GTK_TOGGLE_BUTTON(dlarea_info.chk_zoom_levels[i])))
                {
                    guint start_tilex, start_tiley, end_tilex, end_tiley;
                    guint tilex, tiley;
                    start_tilex = unit2ztile(start_unitx, i + 1);
                    start_tiley = unit2ztile(start_unity, i + 1);
                    end_tilex = unit2ztile(end_unitx, i + 1);
                    end_tiley = unit2ztile(end_unity, i + 1);
                    for(tiley = start_tiley; tiley <= end_tiley; tiley++)
                        for(tilex = start_tilex; tilex <= end_tilex; tilex++)
                        {
                            gchar buffer[1024];
                            sprintf(buffer, "%s/%u/%u/%u.jpg",
                                _map_dir_name, i, tilex, tiley);
                            map_initiate_download(buffer, tilex, tiley, i + 1);
                        }
                }
            }
            gtk_widget_destroy(confirm);
            break;
        }
        gtk_widget_destroy(confirm);
    }

    gtk_widget_hide(dialog); /* Destroying causes a crash (!?!?!??!) */

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_fullscreen(GtkAction *action)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if((_fullscreen = gtk_check_menu_item_get_active(
                GTK_CHECK_MENU_ITEM(_menu_fullscreen_item))))
        gtk_window_fullscreen(GTK_WINDOW(_window));
    else
        gtk_window_unfullscreen(GTK_WINDOW(_window));

    gtk_idle_add((GSourceFunc)window_present, NULL);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_enable_gps(GtkAction *action)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if((_enable_gps = gtk_check_menu_item_get_active(
                GTK_CHECK_MENU_ITEM(_menu_enable_gps_item))))
    {
        if(_rcvr_mac)
        {
            set_conn_state(RCVR_DOWN);
            rcvr_connect_now();
        }
        else
        {
            popup_error("Cannot enable GPS until a GPS Receiver MAC "
                    "is set in the Settings dialog box.");
            gtk_check_menu_item_set_active(
                    GTK_CHECK_MENU_ITEM(_menu_enable_gps_item), FALSE);
        }
    }
    else
    {
        if(_conn_state > RCVR_OFF)
            set_conn_state(RCVR_OFF);
        rcvr_disconnect();
        track_add(0, 0, FALSE);
    }
    map_move_mark();

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_auto_download(GtkAction *action)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if((_auto_download = gtk_check_menu_item_get_active(
            GTK_CHECK_MENU_ITEM(_menu_auto_download_item))))
        map_force_redraw();

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_settings(GtkAction *action)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(settings_dialog())
    {
        /* Settings have changed - reconnect to receiver. */
        if(_enable_gps)
        {
            set_conn_state(RCVR_DOWN);
            rcvr_disconnect();
            rcvr_connect_now();
        }
    }
    MACRO_RECALC_FOCUS_BASE();
    MACRO_RECALC_FOCUS_SIZE();
    map_force_redraw();

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
map_download_idle_refresh(ProgressUpdateInfo *pui)
{
    vprintf("%s(%p)\n", __PRETTY_FUNCTION__, pui);

    if(pui)
    {
        gint zoom_diff = pui->zoom - _zoom;
        /* Only refresh at same or "lower" (more detailed) zoom level. */
        if(zoom_diff >= 0)
        {
            /* If zoom has changed since we first put in the request for this
             * tile, then we may have to update more than one tile. */
            guint tilex, tiley, tilex_end, tiley_end;
            for(tilex = pui->tilex << zoom_diff,
                    tilex_end = tilex + (1 << zoom_diff);
                    tilex < tilex_end; tilex++)
                for(tiley = pui->tiley<<zoom_diff,
                        tiley_end = tiley + (1 << zoom_diff);
                        tiley < tiley_end; tiley++)
                {
                    if((tilex - _base_tilex) < 4 && (tiley - _base_tiley) < 3)
                    {
                        map_render_tile(
                                tilex, tiley,
                                ((tilex - _base_tilex) << TILE_SIZE_P2),
                                ((tiley - _base_tiley) << TILE_SIZE_P2),
                                TRUE);
                        map_render_tracks();
                        gtk_widget_queue_draw_area(
                                _map_widget,
                                ((tilex-_base_tilex)<<TILE_SIZE_P2) - _offsetx,
                                ((tiley-_base_tiley)<<TILE_SIZE_P2) - _offsety,
                                TILE_SIZE_PIXELS, TILE_SIZE_PIXELS);
                    }
                }
        }
        progress_update_info_free(pui);
    }
    if(++_curr_download == _num_downloads)
    {
        gtk_widget_destroy(_download_banner);
        _download_banner = NULL;
        _num_downloads = _curr_download = 0;
    }
    else
        hildon_banner_set_fraction(HILDON_BANNER(_download_banner),
                _curr_download / (double)_num_downloads);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return FALSE;
}

static gint
map_download_cb_async(GnomeVFSAsyncHandle *handle,
        GnomeVFSXferProgressInfo *info, ProgressUpdateInfo*pui)
{
    vprintf("%s(%p, %d, %d, %d, %s)\n", __PRETTY_FUNCTION__, pui->handle,
            info->status, info->vfs_status, info->phase,
            info->target_name ? info->target_name + 7 : info->target_name);

    if(info->phase == GNOME_VFS_XFER_PHASE_COMPLETED)
        g_idle_add_full(G_PRIORITY_HIGH_IDLE,
                (GSourceFunc)map_download_idle_refresh, pui, NULL);

    vprintf("%s(): return %d\n", __PRETTY_FUNCTION__,
            info->status == GNOME_VFS_XFER_PROGRESS_STATUS_OK);
    return (info->status == GNOME_VFS_XFER_PROGRESS_STATUS_OK);
}

/****************************************************************************
 * ABOVE: CALLBACKS *********************************************************
 ****************************************************************************/

