/*
 * This file is part of maemo-mapper
 *
 * Copyright (C) 2006 John Costigan.
 *
 * POI and GPS-Info code originally written by Cezary Jackiewicz.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */


#define _GNU_SOURCE

#define _(String) gettext(String)

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
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <fcntl.h>
#include <gdk/gdkkeysyms.h>
#include <libosso.h>
#include <osso-helplib.h>
#include <osso-ic-dbus.h>
#include <osso-ic.h>
#include <dbus/dbus-glib.h>
#include <bt-dbus.h>
#include <hildon-widgets/hildon-program.h>
#include <hildon-widgets/hildon-controlbar.h>
#include <hildon-widgets/hildon-note.h>
#include <hildon-widgets/hildon-color-button.h>
#include <hildon-widgets/hildon-file-chooser-dialog.h>
#include <hildon-widgets/hildon-number-editor.h>
#include <hildon-widgets/hildon-banner.h>
#include <hildon-widgets/hildon-system-sound.h>
#include <libgnomevfs/gnome-vfs.h>
#include <curl/multi.h>
#include <gconf/gconf-client.h>
#include <libxml/parser.h>

#include <libintl.h>
#include <locale.h>

#include <sqlite.h>

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
#define MAX_ZOOM 16

#define TILE_SIZE_PIXELS (256)
#define TILE_SIZE_P2 (8)
#define BUF_WIDTH_TILES (4)
#define BUF_HEIGHT_TILES (3)
#define BUF_WIDTH_PIXELS (1024)
#define BUF_HEIGHT_PIXELS (768)

#define ARRAY_CHUNK_SIZE (1024)

#define BUFFER_SIZE (2048)

#define WORLD_SIZE_UNITS (2 << (MAX_ZOOM + TILE_SIZE_P2))

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
#define pixel2zunit(pixel, zoom) ((pixel) << (zoom))

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

/* Pans are done two "grids" at a time, or 64 pixels. */
#define PAN_UNITS (grid2unit(2))

#define INITIAL_DOWNLOAD_RETRIES (3)

#define GCONF_KEY_PREFIX "/apps/maemo/maemo-mapper"
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
#define GCONF_KEY_VOICE_SPEED GCONF_KEY_PREFIX"/voice_speed"
#define GCONF_KEY_VOICE_PITCH GCONF_KEY_PREFIX"/voice_pitch"
#define GCONF_KEY_ALWAYS_KEEP_ON GCONF_KEY_PREFIX"/always_keep_on"
#define GCONF_KEY_UNITS GCONF_KEY_PREFIX"/units"
#define GCONF_KEY_ESCAPE_KEY GCONF_KEY_PREFIX"/escape_key"
#define GCONF_KEY_SPEED_LIMIT_ON GCONF_KEY_PREFIX"/speed_limit_on"
#define GCONF_KEY_SPEED_LIMIT GCONF_KEY_PREFIX"/speed_limit"
#define GCONF_KEY_SPEED_LOCATION GCONF_KEY_PREFIX"/speed_location"
#define GCONF_KEY_INFO_FONT_SIZE GCONF_KEY_PREFIX"/info_font_size"

#define GCONF_KEY_POI_DB GCONF_KEY_PREFIX"/poi_db"
#define GCONF_KEY_POI_ZOOM GCONF_KEY_PREFIX"/poi_zoom"

#define GCONF_KEY_COLOR_MARK GCONF_KEY_PREFIX"/color_mark"
#define GCONF_KEY_COLOR_MARK_VELOCITY GCONF_KEY_PREFIX"/color_mark_velocity"
#define GCONF_KEY_COLOR_MARK_OLD GCONF_KEY_PREFIX"/color_mark_old"
#define GCONF_KEY_COLOR_TRACK GCONF_KEY_PREFIX"/color_track"
#define GCONF_KEY_COLOR_TRACK_BREAK GCONF_KEY_PREFIX"/color_track_break"
#define GCONF_KEY_COLOR_ROUTE GCONF_KEY_PREFIX"/color_route"
#define GCONF_KEY_COLOR_ROUTE_WAY GCONF_KEY_PREFIX"/color_route_way"
#define GCONF_KEY_COLOR_ROUTE_NEXTWAY GCONF_KEY_PREFIX"/color_route_nextway"
#define GCONF_KEY_COLOR_POI GCONF_KEY_PREFIX"/color_poi"

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
#define GCONF_KEY_REPOSITORIES GCONF_KEY_PREFIX"/repositories"
#define GCONF_KEY_CURRREPO GCONF_KEY_PREFIX"/curr_repo"
#define GCONF_KEY_GPS_INFO GCONF_KEY_PREFIX"/gps_info"
#define GCONF_KEY_ROUTE_DL_RADIUS GCONF_KEY_PREFIX"/route_dl_radius"
#define GCONF_KEY_DEG_FORMAT GCONF_KEY_PREFIX"/deg_format"

#define GCONF_KEY_DISCONNECT_ON_COVER \
  "/system/osso/connectivity/IAP/disconnect_on_cover"

#define GCONF_KEY_HTTP_PROXY_PREFIX "/system/http_proxy"
#define GCONF_KEY_HTTP_PROXY_ON GCONF_KEY_HTTP_PROXY_PREFIX"/use_http_proxy"
#define GCONF_KEY_HTTP_PROXY_HOST GCONF_KEY_HTTP_PROXY_PREFIX"/host"
#define GCONF_KEY_HTTP_PROXY_PORT GCONF_KEY_HTTP_PROXY_PREFIX"/port"

#define CONFIG_DIR_NAME "~/.maemo-mapper/"
#define CONFIG_FILE_ROUTE "route.gpx"
#define CONFIG_FILE_TRACK "track.gpx"

#define XML_DATE_FORMAT "%FT%T"

#define XML_TRKSEG_HEADER ( \
  "<?xml version=\"1.0\"?>\n" \
  "<gpx version=\"1.0\" creator=\"Maemo Mapper\" " \
      "xmlns=\"http://www.topografix.com/GPX/1/0\">\n" \
  "  <trk>\n" \
  "    <trkseg>\n" \
)

#define XML_TRKSEG_FOOTER ( \
  "    </trkseg>\n" \
  "  </trk>\n" \
  "</gpx>\n" \
)

#define HELP_ID_PREFIX "help_maemomapper_"
#define HELP_ID_INTRO HELP_ID_PREFIX"intro"

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
  _max_center.unitx = WORLD_SIZE_UNITS-grid2unit(_screen_grids_halfwidth) - 1;\
  _max_center.unity = WORLD_SIZE_UNITS-grid2unit(_screen_grids_halfheight)- 1;\
}

#define MACRO_INIT_TRACK(track) { \
    (track).head = (track).tail = g_new(TrackPoint, ARRAY_CHUNK_SIZE); \
    *((track).tail) = _track_null; \
    (track).cap = (track).head + ARRAY_CHUNK_SIZE; \
}

#define MACRO_CLEAR_TRACK(track) if((track).head) { \
    g_free((track).head); \
    (track).head = (track).tail = (track).cap = NULL; \
}

#define MACRO_INIT_ROUTE(route) { \
    (route).head = (route).tail = g_new(Point, ARRAY_CHUNK_SIZE); \
    *((route).tail) = _pos_null; \
    (route).cap = (route).head + ARRAY_CHUNK_SIZE; \
    (route).whead = g_new(WayPoint, ARRAY_CHUNK_SIZE); \
    (route).wtail = (route).whead - 1; \
    (route).wcap = (route).whead + ARRAY_CHUNK_SIZE; \
}

#define MACRO_CLEAR_ROUTE(route) if((route).head) { \
    WayPoint *curr; \
    g_free((route).head); \
    (route).head = (route).tail = (route).cap = NULL; \
    for(curr = (route).whead - 1; curr++ != (route).wtail; ) \
        g_free(curr->desc); \
    g_free((route).whead); \
    (route).whead = (route).wtail = (route).wcap = NULL; \
}

#define DISTANCE_SQUARED(a, b) \
    ((((gint)((b).unitx) - (a).unitx) * ((gint)((b).unitx) - (a).unitx)) \
     + (((gint)((b).unity) - (a).unity) * ((gint)((b).unity) - (a).unity)))

#define DISTANCE_ROUGH(a, b) \
    (abs((gint)((b).unitx) - (a).unitx) \
     + abs((gint)((b).unity) - (a).unity))

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

#define MACRO_TRACK_INCREMENT_TAIL(track) { \
    if(++(track).tail == (track).cap) \
        track_resize(&(track), (track).cap - (track).head + ARRAY_CHUNK_SIZE);\
}

#define MACRO_ROUTE_INCREMENT_TAIL(route) { \
    if(++(route).tail == (route).cap) \
        route_resize(&(route), (route).cap - (route).head + ARRAY_CHUNK_SIZE);\
}

#define MACRO_ROUTE_INCREMENT_WTAIL(route) { \
    if(++(route).wtail == (route).wcap) \
        route_wresize(&(route), \
                (route).wcap - (route).whead + ARRAY_CHUNK_SIZE); \
}

#define TRACKS_MASK 0x00000001
#define ROUTES_MASK 0x00000002

#define g_timeout_add(I, F, D) g_timeout_add_full(G_PRIORITY_DEFAULT_IDLE, \
          (I), (F), (D), NULL)

#define MACRO_CURL_EASY_INIT(C) { \
    C = curl_easy_init(); \
    curl_easy_setopt(C, CURLOPT_NOPROGRESS, 1); \
    curl_easy_setopt(C, CURLOPT_FOLLOWLOCATION, 1); \
    curl_easy_setopt(C, CURLOPT_FAILONERROR, 1); \
    curl_easy_setopt(C, CURLOPT_USERAGENT, \
            "Mozilla/5.0 (X11; U; Linux i686; en-US; " \
            "rv:1.8.0.4) Gecko/20060701 Firefox/1.5.0.4"); \
    curl_easy_setopt(C, CURLOPT_TIMEOUT, 30); \
    curl_easy_setopt(C, CURLOPT_CONNECTTIMEOUT, 10); \
    if(_iap_http_proxy_host) \
    { \
        curl_easy_setopt(C, CURLOPT_PROXY, _iap_http_proxy_host); \
        if(_iap_http_proxy_port) \
            curl_easy_setopt(C, CURLOPT_PROXYPORT, _iap_http_proxy_port); \
    } \
}

#define MACRO_BANNER_SHOW_INFO(A, S) { \
    gchar *my_macro_buffer = g_strdup_printf("<span size='%s'>%s</span>", \
            INFO_FONT_TEXT[_info_font_size], (S)); \
    hildon_banner_show_information_with_markup(A, NULL, my_macro_buffer); \
    g_free(my_macro_buffer); \
}

/****************************************************************************
 * ABOVE: DEFINES ***********************************************************
 ****************************************************************************/

/****************************************************************************
 * BELOW: MARSHALLERS *******************************************************
 ****************************************************************************/


#ifndef __g_cclosure_user_marshal_MARSHAL_H__
#define __g_cclosure_user_marshal_MARSHAL_H__

#include	<glib-object.h>

G_BEGIN_DECLS

#ifdef G_ENABLE_DEBUG
#define g_marshal_value_peek_boolean(v)  g_value_get_boolean (v)
#define g_marshal_value_peek_char(v)     g_value_get_char (v)
#define g_marshal_value_peek_uchar(v)    g_value_get_uchar (v)
#define g_marshal_value_peek_int(v)      g_value_get_int (v)
#define g_marshal_value_peek_uint(v)     g_value_get_uint (v)
#define g_marshal_value_peek_long(v)     g_value_get_long (v)
#define g_marshal_value_peek_ulong(v)    g_value_get_ulong (v)
#define g_marshal_value_peek_int64(v)    g_value_get_int64 (v)
#define g_marshal_value_peek_uint64(v)   g_value_get_uint64 (v)
#define g_marshal_value_peek_enum(v)     g_value_get_enum (v)
#define g_marshal_value_peek_flags(v)    g_value_get_flags (v)
#define g_marshal_value_peek_float(v)    g_value_get_float (v)
#define g_marshal_value_peek_double(v)   g_value_get_double (v)
#define g_marshal_value_peek_string(v)   (char*) g_value_get_string (v)
#define g_marshal_value_peek_param(v)    g_value_get_param (v)
#define g_marshal_value_peek_boxed(v)    g_value_get_boxed (v)
#define g_marshal_value_peek_pointer(v)  g_value_get_pointer (v)
#define g_marshal_value_peek_object(v)   g_value_get_object (v)
#else /* !G_ENABLE_DEBUG */
/* WARNING: This code accesses GValues directly, which is UNSUPPORTED API.
 *          Do not access GValues directly in your code. Instead, use the
 *          g_value_get_*() functions
 */
#define g_marshal_value_peek_boolean(v)  (v)->data[0].v_int
#define g_marshal_value_peek_char(v)     (v)->data[0].v_int
#define g_marshal_value_peek_uchar(v)    (v)->data[0].v_uint
#define g_marshal_value_peek_int(v)      (v)->data[0].v_int
#define g_marshal_value_peek_uint(v)     (v)->data[0].v_uint
#define g_marshal_value_peek_long(v)     (v)->data[0].v_long
#define g_marshal_value_peek_ulong(v)    (v)->data[0].v_ulong
#define g_marshal_value_peek_int64(v)    (v)->data[0].v_int64
#define g_marshal_value_peek_uint64(v)   (v)->data[0].v_uint64
#define g_marshal_value_peek_enum(v)     (v)->data[0].v_long
#define g_marshal_value_peek_flags(v)    (v)->data[0].v_ulong
#define g_marshal_value_peek_float(v)    (v)->data[0].v_float
#define g_marshal_value_peek_double(v)   (v)->data[0].v_double
#define g_marshal_value_peek_string(v)   (v)->data[0].v_pointer
#define g_marshal_value_peek_param(v)    (v)->data[0].v_pointer
#define g_marshal_value_peek_boxed(v)    (v)->data[0].v_pointer
#define g_marshal_value_peek_pointer(v)  (v)->data[0].v_pointer
#define g_marshal_value_peek_object(v)   (v)->data[0].v_pointer
#endif /* !G_ENABLE_DEBUG */


/* VOID:STRING,STRING,POINTER,UCHAR,UINT (marshal.list:1) */
extern void g_cclosure_user_marshal_VOID__STRING_STRING_POINTER_UCHAR_UINT (GClosure     *closure,
                                                                            GValue       *return_value,
                                                                            guint         n_param_values,
                                                                            const GValue *param_values,
                                                                            gpointer      invocation_hint,
                                                                            gpointer      marshal_data);
void
g_cclosure_user_marshal_VOID__STRING_STRING_POINTER_UCHAR_UINT (GClosure     *closure,
                                                                GValue       *return_value,
                                                                guint         n_param_values,
                                                                const GValue *param_values,
                                                                gpointer      invocation_hint,
                                                                gpointer      marshal_data)
{
  typedef void (*GMarshalFunc_VOID__STRING_STRING_POINTER_UCHAR_UINT) (gpointer     data1,
                                                                       gpointer     arg_1,
                                                                       gpointer     arg_2,
                                                                       gpointer     arg_3,
                                                                       guchar       arg_4,
                                                                       guint        arg_5,
                                                                       gpointer     data2);
  register GMarshalFunc_VOID__STRING_STRING_POINTER_UCHAR_UINT callback;
  register GCClosure *cc = (GCClosure*) closure;
  register gpointer data1, data2;

  g_return_if_fail (n_param_values == 6);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
      data1 = g_value_peek_pointer (param_values + 0);
      data2 = closure->data;
    }
  callback = (GMarshalFunc_VOID__STRING_STRING_POINTER_UCHAR_UINT) (marshal_data ? marshal_data : cc->callback);

  callback (data1,
            g_marshal_value_peek_string (param_values + 1),
            g_marshal_value_peek_string (param_values + 2),
            g_marshal_value_peek_pointer (param_values + 3),
            g_marshal_value_peek_uchar (param_values + 4),
            g_marshal_value_peek_uint (param_values + 5),
            data2);
}

G_END_DECLS

#endif /* __g_cclosure_user_marshal_MARSHAL_H__ */


/****************************************************************************
 * ABOVE: MARSHALLERS *******************************************************
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
    INSIDE_PATH,
    INSIDE_PATH_SEGMENT,
    INSIDE_PATH_POINT,
    INSIDE_PATH_POINT_ELE,
    INSIDE_PATH_POINT_TIME,
    INSIDE_PATH_POINT_DESC,
    FINISH,
    UNKNOWN,
    ERROR,
} SaxState;

/** POI dialog action **/
typedef enum
{
    ACTION_ADD_POI,
    ACTION_EDIT_POI,
} POIAction;

/** Category list **/
typedef enum
{
    CAT_ID,
    CAT_ENABLED,
    CAT_LABEL,
    CAT_DESCRIPTION,
    CAT_NUM_COLUMNS
} CategoryList;

/** POI list **/
typedef enum
{
    POI_INDEX,
    POI_LATITUDE,
    POI_LONGITUDE,
    POI_LABEL,
    POI_CATEGORY,
    POI_NUM_COLUMNS
} POIList;

/** This enum defines the possible units we can use. */
typedef enum
{
    UNITS_KM,
    UNITS_MI,
    UNITS_NM,
    UNITS_ENUM_COUNT
} UnitType;
gchar *UNITS_TEXT[UNITS_ENUM_COUNT];

/* UNITS_CONVERTS, when multiplied, converts from NM. */
#define EARTH_RADIUS (3440.06479f)
gfloat UNITS_CONVERT[] =
{
    1.85200,
    1.15077945,
    1.f,
};

/** This enum defines the possible font sizes. */
typedef enum
{
    INFO_FONT_XXSMALL,
    INFO_FONT_XSMALL,
    INFO_FONT_SMALL,
    INFO_FONT_MEDIUM,
    INFO_FONT_LARGE,
    INFO_FONT_XLARGE,
    INFO_FONT_XXLARGE,
    INFO_FONT_ENUM_COUNT
} InfoFontSize;
gchar *INFO_FONT_TEXT[INFO_FONT_ENUM_COUNT];

typedef enum
{
    ESCAPE_KEY_TOGGLE_TRACKS,
    ESCAPE_KEY_CHANGE_REPO,
    ESCAPE_KEY_RESET_BLUETOOTH,
    ESCAPE_KEY_TOGGLE_GPS,
    ESCAPE_KEY_TOGGLE_GPSINFO,
    ESCAPE_KEY_TOGGLE_SPEEDLIMIT,
    ESCAPE_KEY_ENUM_COUNT
} EscapeKeyAction;
gchar *ESCAPE_KEY_TEXT[ESCAPE_KEY_ENUM_COUNT];

typedef enum
{
    DDPDDDDD,
    DD_MMPMMM,
    DD_MM_SSPS,
    DEG_FORMAT_ENUM_COUNT
} Degree_format;
gchar *DEG_FORMAT_TEXT[DEG_FORMAT_ENUM_COUNT];

typedef enum
{
    SPEED_LOCATION_TOP_LEFT,
    SPEED_LOCATION_TOP_RIGHT,
    SPEED_LOCATION_BOTTOM_RIGHT,
    SPEED_LOCATION_BOTTOM_LEFT,
    SPEED_LOCATION_ENUM_COUNT
} SpeedLocation;
gchar *SPEED_LOCATION_TEXT[SPEED_LOCATION_ENUM_COUNT];

/** A general definition of a point in the Maemo Mapper unit system. */
typedef struct _Point Point;
struct _Point {
    guint unitx;
    guint unity;
};

/** A TrackPoint, which is a Point with a time. */
typedef struct _TrackPoint TrackPoint;
struct _TrackPoint {
    Point point;
    time_t time;
    gfloat altitude;
};

/** A WayPoint, which is a Point with a description. */
typedef struct _WayPoint WayPoint;
struct _WayPoint {
    Point *point;
    gchar *desc;
};

/** A Track is a set of timed TrackPoints. */
typedef struct _Track Track;
struct _Track {
    TrackPoint *head; /* points to first element in array; NULL if empty. */
    TrackPoint *tail; /* points to last element in array. */
    TrackPoint *cap; /* points after last slot in array. */
};

/** A Route is a set of untimed Points and WayPoints. */
typedef struct _Route Route;
struct _Route {
    Point *head; /* points to first element in array; NULL if empty. */
    Point *tail; /* points to last element in array. */
    Point *cap; /* points after last slot in array. */
    WayPoint *whead; /* points to first element in array; NULL if empty. */
    WayPoint *wtail; /* points to last element in array. */
    WayPoint *wcap; /* points after last slot in array. */
};

typedef enum _PathType PathType;
enum _PathType {
    TRACK,
    ROUTE,
};

typedef struct _Path Path;
struct _Path {
    union {
        Track track;
        Route route;
    } path;
    PathType path_type;
};

/** Data used during the SAX parsing operation. */
typedef struct _SaxData SaxData;
struct _SaxData {
    Path path;
    SaxState state;
    SaxState prev_state;
    guint unknown_depth;
    gboolean at_least_one_trkpt;
    GString *chars;
};

/** Data used during action: add or edit category/poi **/
typedef struct _DeletePOI DeletePOI;
struct _DeletePOI {
    GtkWidget *dialog;
    gchar *txt_label;
    guint id;
};

/** Data regarding a map repository. */
typedef struct _RepoData RepoData;
struct _RepoData {
    gchar *name;
    gchar *url;
    gchar *cache_dir;
    guint dl_zoom_steps;
    guint view_zoom_steps;
    GtkWidget *menu_item;
};

/** GPS Data and Satellite **/
typedef struct _GpsData GpsData;
struct _GpsData {
    guint fix;
    guint fixquality;
    struct tm timeloc;    /* local time */
    gfloat latitude;
    gfloat longitude;
    gchar slatitude[15];
    gchar slongitude[15];
    gfloat speed;    /* in knots */
    gfloat maxspeed;    /* in knots */
    gfloat altitude; /* in meters */
    gfloat heading;
    gfloat hdop;
    gfloat pdop;
    gfloat vdop;
    guint satinview;
    guint satinuse;
    guint satforfix[12];
};

typedef struct _GpsSatelliteData GpsSatelliteData;
struct _GpsSatelliteData {
    guint prn;
    guint elevation;
    guint azimuth;
    guint snr;
};

/** Data used during the asynchronous progress update phase of automatic map
 * downloading. */
typedef struct _ProgressUpdateInfo ProgressUpdateInfo;
struct _ProgressUpdateInfo
{
    gchar *src_str;
    gchar *dest_str;
    RepoData *repo;
    guint tilex, tiley, zoom; /* for refresh. */
    gint retries; /* if equal to zero, it means we're DELETING maps. */
    guint priority;
    FILE *file;
};

typedef struct _RouteDownloadData RouteDownloadData;
struct _RouteDownloadData {
    gchar *bytes;
    guint bytes_read;
};

/** Data used during the asynchronous automatic route downloading operation. */
typedef struct _AutoRouteDownloadData AutoRouteDownloadData;
struct _AutoRouteDownloadData {
    gboolean enabled;
    gboolean in_progress;
    gchar *dest;
    CURL *curl_easy;
    gchar *src_str;
    RouteDownloadData rdl_data;
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

static GtkWidget *_gps_widget = NULL;
static GtkWidget *_text_lat = NULL;
static GtkWidget *_text_lon = NULL;
static GtkWidget *_text_speed = NULL;
static GtkWidget *_text_alt = NULL;
static GtkWidget *_sat_panel = NULL;
static GtkWidget *_text_time = NULL;
static GtkWidget *_heading_panel = NULL;

static GtkWidget *_sat_details_panel = NULL;
static GtkWidget *_sdi_lat = NULL;
static GtkWidget *_sdi_lon = NULL;
static GtkWidget *_sdi_spd = NULL;
static GtkWidget *_sdi_alt = NULL;
static GtkWidget *_sdi_hea = NULL;
static GtkWidget *_sdi_tim = NULL;
static GtkWidget *_sdi_vie = NULL;
static GtkWidget *_sdi_use = NULL;
static GtkWidget *_sdi_fix = NULL;
static GtkWidget *_sdi_fqu = NULL;
static GtkWidget *_sdi_msp = NULL;

/** The file descriptor of our connection with the GPS receiver. */
static gint _fd = -1;
#define GPS_READ_BUF_SIZE 256
static gchar _gps_read_buf[GPS_READ_BUF_SIZE];
static gchar *_gps_read_buf_curr = _gps_read_buf;
static gchar *_gps_read_buf_last = _gps_read_buf + GPS_READ_BUF_SIZE - 1;
static DBusGProxy *_rfcomm_req_proxy = NULL;

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
/** The Source ID of the CURL Multi Download timeout. */
static guint _curl_sid = 0;

/** GPS data. */
static Point _pos = {0, 0};
static const Point _pos_null = {0, 0};
static const TrackPoint _track_null = { { 0, 0 }, FP_NAN};
static gint _vel_offsetx = 0;
static gint _vel_offsety = 0;

static GpsData _gps;
static GpsSatelliteData _gps_sat[12];
gboolean _satdetails_on = FALSE;

/** The current connection state. */
static ConnState _conn_state = RCVR_OFF;

/** VARIABLES PERTAINING TO THE INTERNET CONNECTION. */
static gboolean _iap_connected = FALSE;
static gboolean _iap_connecting = FALSE;
static gchar *_iap_http_proxy_host = NULL;
static gint _iap_http_proxy_port = 0;

/** VARIABLES FOR MAINTAINING STATE OF THE CURRENT VIEW. */

/** The "zoom" level defines the resolution of a pixel, from 0 to MAX_ZOOM.
 * Each pixel in the current view is exactly (1 << _zoom) "units" wide. */
static guint _zoom = 3; /* zoom level, from 0 to MAX_ZOOM. */
static Point _center = {-1, -1}; /* current center location, X. */

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
static Point _focus = {-1, -1};
static guint _focus_unitwidth = 0;
static guint _focus_unitheight = 0;
static Point _min_center = {-1, -1};
static Point _max_center = {-1, -1};
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
static Route _route;

/** Data for tracking waypoints for the purpose of announcement. */
/* _near_point is the route point to which we are closest. */
static Point *_near_point = NULL;
static guint _near_point_dist_rough = -1;
/* _next_way is what we currently interpret to be the next waypoint. */
static WayPoint *_next_way = NULL;
static guint _next_way_dist_rough = -1;
static gchar *_last_spoken_phrase = NULL;
/* _next_wpt is the route point immediately following _next_way. */
static Point *_next_wpt = NULL;
static guint _next_wpt_dist_rough = -1;

static WayPoint *_visible_way_first = NULL;
static WayPoint *_visible_way_last = NULL;

static guint _key_zoom_new = 0;
static guint _key_zoom_timeout_sid = 0;

/** THE GdkGC OBJECTS USED FOR DRAWING. */
static GdkGC *_gc_mark = NULL;
static GdkGC *_gc_mark_velocity = NULL;
static GdkGC *_gc_mark_old = NULL;
static GdkGC *_gc_track = NULL;
static GdkGC *_gc_track_break = NULL;
static GdkGC *_gc_route = NULL;
static GdkGC *_gc_route_way = NULL;
static GdkGC *_gc_route_nextway = NULL;
static GdkGC *_gc_poi = NULL;
static GdkColor _color_mark = {0, 0, 0, 0};
static GdkColor _color_mark_velocity = {0, 0, 0, 0};
static GdkColor _color_mark_old = {0, 0, 0, 0};
static GdkColor _color_track = {0, 0, 0, 0};
static GdkColor _color_track_break = {0, 0, 0, 0};
static GdkColor _color_route = {0, 0, 0, 0};
static GdkColor _color_route_way = {0, 0, 0, 0};
static GdkColor _color_route_nextway = {0, 0, 0, 0};
static GdkColor _color_poi = {0, 0, 0, 0};
static GdkColor DEFAULT_COLOR_MARK = {0, 0x0000, 0x0000, 0xc000};
static GdkColor DEFAULT_COLOR_MARK_VELOCITY = {0, 0x6000, 0x6000, 0xf800};
static GdkColor DEFAULT_COLOR_MARK_OLD = {0, 0x8000, 0x8000, 0x8000};
static GdkColor DEFAULT_COLOR_TRACK = {0, 0xe000, 0x0000, 0x0000};
static GdkColor DEFAULT_COLOR_TRACK_BREAK = {0, 0xa000, 0x0000, 0x0000};
static GdkColor DEFAULT_COLOR_ROUTE = {0, 0x0000, 0xa000, 0x0000};
static GdkColor DEFAULT_COLOR_ROUTE_WAY = {0, 0x0000, 0x8000, 0x0000};
static GdkColor DEFAULT_COLOR_ROUTE_NEXTWAY = {0, 0x0000, 0x6000, 0x0000};
static GdkColor DEFAULT_COLOR_POI = {0, 0xa000, 0x0000, 0xa000};

/** MAIN MENU ITEMS. */
static GtkWidget *_menu_route_download_item = NULL;
static GtkWidget *_menu_route_open_item = NULL;
static GtkWidget *_menu_route_save_item = NULL;
static GtkWidget *_menu_route_reset_item = NULL;
static GtkWidget *_menu_route_clear_item = NULL;
static GtkWidget *_menu_track_open_item = NULL;
static GtkWidget *_menu_track_save_item = NULL;
static GtkWidget *_menu_track_mark_way_item = NULL;
static GtkWidget *_menu_track_clear_item = NULL;
static GtkWidget *_menu_maps_submenu = NULL;
static GtkWidget *_menu_maps_mapman_item = NULL;
static GtkWidget *_menu_maps_repoman_item = NULL;
static GtkWidget *_menu_auto_download_item = NULL;
static GtkWidget *_menu_show_tracks_item = NULL;
static GtkWidget *_menu_show_routes_item = NULL;
static GtkWidget *_menu_show_velvec_item = NULL;
static GtkWidget *_menu_ac_latlon_item = NULL;
static GtkWidget *_menu_ac_lead_item = NULL;
static GtkWidget *_menu_ac_none_item = NULL;
static GtkWidget *_menu_fullscreen_item = NULL;
static GtkWidget *_menu_enable_gps_item = NULL;
static GtkWidget *_menu_gps_show_info_item = NULL;
static GtkWidget *_menu_gps_details_item = NULL;
static GtkWidget *_menu_gps_reset_item = NULL;
static GtkWidget *_menu_settings_item = NULL;
static GtkWidget *_menu_poi_item = NULL;
static GtkWidget *_menu_help_item = NULL;
static GtkWidget *_menu_close_item = NULL;

/** CONTEXT MENU ITEMS. */
static guint _cmenu_position_x;
static guint _cmenu_position_y;

static GtkWidget *_cmenu_loc_show_latlon_item = NULL;
static GtkWidget *_cmenu_loc_clip_latlon_item = NULL;
static GtkWidget *_cmenu_loc_route_to_item = NULL;
static GtkWidget *_cmenu_loc_distance_to_item = NULL;
static GtkWidget *_cmenu_add_poi = NULL;
static GtkWidget *_cmenu_edit_poi = NULL;

static GtkWidget *_cmenu_way_show_latlon_item = NULL;
static GtkWidget *_cmenu_way_show_desc_item = NULL;
static GtkWidget *_cmenu_way_clip_latlon_item = NULL;
static GtkWidget *_cmenu_way_clip_desc_item = NULL;
static GtkWidget *_cmenu_way_route_to_item = NULL;
static GtkWidget *_cmenu_way_distance_to_item = NULL;
static GtkWidget *_cmenu_way_delete_item = NULL;

/** BANNERS. */
GtkWidget *_connect_banner = NULL;
GtkWidget *_fix_banner = NULL;
GtkWidget *_download_banner = NULL;

/** DOWNLOAD PROGRESS. */
static CURLM *_curl_multi = NULL;
static GQueue *_curl_easy_queue = NULL;
static guint _num_downloads = 0;
static guint _curr_download = 0;
static GTree *_pui_tree = NULL;
static GTree *_downloading_tree = NULL;
static GHashTable *_pui_by_easy = NULL;

/** CONFIGURATION INFORMATION. */
static gchar *_rcvr_mac = NULL;
static gchar *_route_dir_uri = NULL;
static gchar *_track_file_uri = NULL;
static CenterMode _center_mode = CENTER_LEAD;
static gboolean _fullscreen = FALSE;
static gboolean _enable_gps = FALSE;
static gboolean _gps_info = FALSE;
static guint _route_dl_radius = 4;
static gint _show_tracks = 0;
static gboolean _show_velvec = TRUE;
static gboolean _auto_download = FALSE;
static guint _lead_ratio = 5;
static guint _center_ratio = 7;
static guint _draw_line_width = 5;
static guint _announce_notice_ratio = 8;
static gboolean _enable_voice = TRUE;
static gboolean _always_keep_on = FALSE;
static gdouble _voice_speed = 1.0;
static gint _voice_pitch = 0;
static GSList *_loc_list;
static GtkListStore *_loc_model;
static UnitType _units = UNITS_KM;
static EscapeKeyAction _escape_key = ESCAPE_KEY_TOGGLE_TRACKS;
static guint _degformat = DDPDDDDD;
static gboolean _speed_limit_on = FALSE;
static guint _speed_limit = 100;
static gboolean _speed_excess = FALSE;
static SpeedLocation _speed_location = SPEED_LOCATION_TOP_RIGHT;
static InfoFontSize _info_font_size = INFO_FONT_MEDIUM;

static gchar *_config_dir_uri;

static GList *_repo_list = NULL;
static RepoData *_curr_repo = NULL;

/** POI */
static gchar *_poi_db = NULL;
static sqlite *_db = NULL;
static guint _poi_zoom = 6;
static gboolean _dbconn = FALSE;

/** The singleton auto-route-download data. */
static AutoRouteDownloadData _autoroute_data;

static gchar XML_TZONE[7];
static gint _gmtoffset = 0;

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
window_cb_key_release(GtkWidget* widget, GdkEventKey *event);

static gboolean
map_cb_configure(GtkWidget *widget, GdkEventConfigure *event);
static gboolean
map_cb_expose(GtkWidget *widget, GdkEventExpose *event);
static gboolean
map_cb_button_press(GtkWidget *widget, GdkEventButton *event);
static gboolean
map_cb_button_release(GtkWidget *widget, GdkEventButton *event);
static gboolean
heading_panel_expose(GtkWidget *widget, GdkEventExpose *event);
static gboolean
sat_panel_expose(GtkWidget *widget, GdkEventExpose *event);
static gboolean
sat_details_panel_expose(GtkWidget *widget, GdkEventExpose *event);

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
menu_cb_category(GtkAction *action);


static gboolean
menu_cb_maps_select(GtkAction *action, gpointer new_repo);
static gboolean
menu_cb_mapman(GtkAction *action);
static gboolean
menu_cb_maps_repoman(GtkAction *action);
static gboolean
menu_cb_auto_download(GtkAction *action);

static gboolean
menu_cb_ac_latlon(GtkAction *action);
static gboolean
menu_cb_ac_lead(GtkAction *action);
static gboolean
menu_cb_ac_none(GtkAction *action);


static gboolean
cmenu_cb_loc_show_latlon(GtkAction *action);
static gboolean
cmenu_cb_loc_clip_latlon(GtkAction *action);
static gboolean
cmenu_cb_loc_route_to(GtkAction *action);
static gboolean
cmenu_cb_loc_distance_to(GtkAction *action);
static gboolean
cmenu_cb_add_poi(GtkAction *action);
static gboolean
cmenu_cb_edit_poi(GtkAction *action);

static gboolean
cmenu_cb_way_show_latlon(GtkAction *action);
static gboolean
cmenu_cb_way_show_desc(GtkAction *action);
static gboolean
cmenu_cb_way_clip_latlon(GtkAction *action);
static gboolean
cmenu_cb_way_clip_desc(GtkAction *action);
static gboolean
cmenu_cb_way_route_to(GtkAction *action);
static gboolean
cmenu_cb_way_distance_to(GtkAction *action);
static gboolean
cmenu_cb_way_delete(GtkAction *action);


static gboolean
menu_cb_fullscreen(GtkAction *action);
static gboolean
menu_cb_enable_gps(GtkAction *action);
static gboolean
menu_cb_gps_show_info(GtkAction *action);
static gboolean
menu_cb_gps_details(GtkAction *action);
static gboolean
menu_cb_gps_reset(GtkAction *action);

static gboolean
menu_cb_settings(GtkAction *action);

static gboolean
menu_cb_help(GtkAction *action);

static gboolean
curl_download_timeout();

static gboolean
auto_route_dl_idle();

/****************************************************************************
 * ABOVE: CALLBACK DECLARATIONS *********************************************
 ****************************************************************************/

/**
 * Pop up a modal dialog box with simple error information in it.
 */
static void
popup_error(GtkWidget *window, const gchar *error)
{
    GtkWidget *dialog;
    printf("%s(\"%s\")\n", __PRETTY_FUNCTION__, error);

    dialog = hildon_note_new_information(GTK_WINDOW(window), error);

    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

static void
track_resize(Track *track, guint size)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(track->head + size != track->cap)
    {
        TrackPoint *old_head = track->head;
        track->head = g_renew(TrackPoint, old_head, size);
        track->cap = track->head + size;
        if(track->head != old_head)
            track->tail = track->head + (track->tail - old_head);
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

static void
route_resize(Route *route, guint size)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(route->head + size != route->cap)
    {
        Point *old_head = route->head;
        WayPoint *curr;
        route->head = g_renew(Point, old_head, size);
        route->cap = route->head + size;
        if(route->head != old_head)
        {
            route->tail = route->head + (route->tail - old_head);

            /* Adjust all of the waypoints. */
            for(curr = route->whead - 1; curr++ != route->wtail; )
                curr->point = route->head + (curr->point - old_head);
        }
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

static void
route_wresize(Route *route, guint wsize)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(route->whead + wsize != route->wcap)
    {
        WayPoint *old_whead = route->whead;
        route->whead = g_renew(WayPoint, old_whead, wsize);
        route->wtail = route->whead + (route->wtail - old_whead);
        route->wcap = route->whead + wsize;
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}


/****************************************************************************
 * BELOW: FILE-HANDLING ROUTINES *********************************************
 ****************************************************************************/

#define WRITE_STRING(string) { \
    GnomeVFSResult vfs_result; \
    GnomeVFSFileSize size; \
    if(GNOME_VFS_OK != (vfs_result = gnome_vfs_write( \
                    handle, (string), strlen((string)), &size))) \
    { \
        gchar buffer[BUFFER_SIZE]; \
        snprintf(buffer, sizeof(buffer), \
                "%s:\n%s\n%s", _("Error while writing to file"), \
                _("File is incomplete."), \
                gnome_vfs_result_to_string(vfs_result)); \
        popup_error(_window, buffer); \
        return FALSE; \
    } \
}

static gboolean
write_track_gpx(GnomeVFSHandle *handle)
{
    TrackPoint *curr;
    gboolean trkseg_break = FALSE;
    printf("%s()\n", __PRETTY_FUNCTION__);

    /* Find first non-zero point. */
    for(curr = _track.head-1; curr++ != _track.tail; )
        if(curr->point.unity)
            break;

    /* Write the header. */
    WRITE_STRING(XML_TRKSEG_HEADER);

    /* Curr points to first non-zero point. */
    for(curr--; curr++ != _track.tail; )
    {
        gfloat lat, lon;
        if(curr->point.unity)
        {
            gchar buffer[80];
            gchar strlat[80], strlon[80];
            if(trkseg_break)
            {
                /* First trkpt of the segment - write trkseg header. */
                WRITE_STRING("    </trkseg>\n"
                /* Write trkseg header. */
                             "    <trkseg>\n");
                trkseg_break = FALSE;
            }
            unit2latlon(curr->point.unitx, curr->point.unity, lat, lon);
            g_ascii_formatd(strlat, 80, "%.06f", lat);
            g_ascii_formatd(strlon, 80, "%.06f", lon);
            snprintf(buffer, sizeof(buffer),
                    "      <trkpt lat=\"%s\" lon=\"%s\">\n",
                    strlat, strlon);
            WRITE_STRING(buffer);

            /* write the elevation */
            if(!isnan(curr->altitude))
            {
                WRITE_STRING("        <ele>");
                {
                    g_ascii_formatd(buffer, 80, "%.2f", curr->altitude);
                    WRITE_STRING(buffer);
                }
                WRITE_STRING("</ele>\n");
            }

            /* write the time */
            if(curr->time)
            {
                WRITE_STRING("        <time>");
                {
                    struct tm time;
                    localtime_r(&curr->time, &time);
                    strftime(buffer, 80, XML_DATE_FORMAT, &time);
                    WRITE_STRING(buffer);
                    WRITE_STRING(XML_TZONE);
                }
                WRITE_STRING("</time>\n");
            }
            WRITE_STRING("      </trkpt>\n");
        }
        else
            trkseg_break = TRUE;
    }

    /* Write the footer. */
    WRITE_STRING(XML_TRKSEG_FOOTER);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
write_route_gpx(GnomeVFSHandle *handle)
{
    Point *curr = NULL;
    WayPoint *wcurr = NULL;
    gboolean trkseg_break = FALSE;
    printf("%s()\n", __PRETTY_FUNCTION__);

    /* Find first non-zero point. */
    if(_route.head)
        for(curr = _route.head-1, wcurr = _route.whead; curr++ != _route.tail;)
        {
            if(curr->unity)
                break;
            else if(wcurr && curr == wcurr->point)
                wcurr++;
        }

    /* Write the header. */
    WRITE_STRING(XML_TRKSEG_HEADER);

    /* Curr points to first non-zero point. */
    if(_route.head)
        for(curr--; curr++ != _route.tail; )
        {
            gfloat lat, lon;
            if(curr->unity)
            {
                gchar buffer[BUFFER_SIZE];
                gchar strlat[80], strlon[80];
                if(trkseg_break)
                {
                    /* First trkpt of the segment - write trkseg header. */
                    WRITE_STRING("    </trkseg>\n"
                                 "    <trkseg>\n");
                    trkseg_break = FALSE;
                }
                unit2latlon(curr->unitx, curr->unity, lat, lon);
                g_ascii_formatd(strlat, 80, "%.06f", lat);
                g_ascii_formatd(strlon, 80, "%.06f", lon);
                snprintf(buffer, sizeof(buffer),
                        "      <trkpt lat=\"%s\" lon=\"%s\"",
                        strlat, strlon);
                if(wcurr && curr == wcurr->point)
                    snprintf(buffer + strlen(buffer),
                            BUFFER_SIZE - strlen(buffer),
                            "><desc>%s</desc></trkpt>\n", wcurr++->desc);
                else
                    strcat(buffer, "/>\n");
                WRITE_STRING(buffer);
            }
            else
                trkseg_break = TRUE;
        }

    /* Write the footer. */
    WRITE_STRING(XML_TRKSEG_FOOTER);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
speed_excess(void)
{
    printf("%s()\n", __PRETTY_FUNCTION__);
    if(!_speed_excess)
        return FALSE;

    hildon_play_system_sound(
        "/usr/share/sounds/ui-information_note.wav");

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static void
speed_limit(void)
{
    PangoContext *context = NULL;
    PangoLayout *layout = NULL;
    PangoFontDescription *fontdesc = NULL;
    GdkColor color;
    GdkGC *gc;
    gfloat cur_speed;
    gchar *buffer;
    static guint x = 0, y = 0, width = 0, height = 0;
    printf("%s()\n", __PRETTY_FUNCTION__);

    cur_speed = _gps.speed * UNITS_CONVERT[_units];

    context = gtk_widget_get_pango_context(_map_widget);
    layout = pango_layout_new(context);
    fontdesc =  pango_font_description_new();

    pango_font_description_set_size(fontdesc, 64 * PANGO_SCALE);
    pango_layout_set_font_description (layout, fontdesc);

    if(cur_speed > _speed_limit)
    {
        color.red = 0xffff;
        if(!_speed_excess)
        {
            _speed_excess = TRUE;
            g_timeout_add(5000, (GSourceFunc)speed_excess, NULL);
        }
    }
    else
    {
        color.red = 0;
        _speed_excess = FALSE;
    }

    color.green = 0;
    color.blue = 0;
    gc = gdk_gc_new (_map_widget->window);
    gdk_gc_set_rgb_fg_color (gc, &color);

    buffer = g_strdup_printf("%0.0f", cur_speed);
    pango_layout_set_text(layout, buffer, strlen(buffer));

    gtk_widget_queue_draw_area ( _map_widget,
        x - 5,
        y - 5,
        width + 5,
        height + 5);
    gdk_window_process_all_updates();

    pango_layout_get_pixel_size(layout, &width, &height);
    switch (_speed_location)
    {
        case SPEED_LOCATION_TOP_RIGHT:
            x = _map_widget->allocation.width - 10 - width;
            y = 5;
            break;
        case SPEED_LOCATION_BOTTOM_RIGHT:
            x = _map_widget->allocation.width - 10 - width;
            y = _map_widget->allocation.height - 10 - height;
            break;
        case SPEED_LOCATION_BOTTOM_LEFT:
            x = 10;
            y = _map_widget->allocation.height - 10 - height;
            break;
        default:
            x = 10;
            y = 10;
            break;
    }

    gdk_draw_layout(_map_widget->window,
        gc,
        x, y,
        layout);
    g_free(buffer);

    pango_font_description_free (fontdesc);
    g_object_unref (layout);
    g_object_unref (gc);

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
                data->state = INSIDE_PATH;
            else
                MACRO_SET_UNKNOWN();
            break;
        case INSIDE_PATH:
            if(!strcmp((gchar*)name, "trkseg"))
            {
                data->state = INSIDE_PATH_SEGMENT;
                data->at_least_one_trkpt = FALSE;
            }
            else
                MACRO_SET_UNKNOWN();
            break;
        case INSIDE_PATH_SEGMENT:
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
                    if(data->path.path_type == TRACK)
                    {
                        MACRO_TRACK_INCREMENT_TAIL(data->path.path.track);
                        latlon2unit(lat, lon,
                                data->path.path.track.tail->point.unitx,
                                data->path.path.track.tail->point.unity);
                        data->path.path.track.tail->time = 0;
                        data->path.path.track.tail->altitude = NAN;
                    }
                    else
                    {
                        MACRO_ROUTE_INCREMENT_TAIL(data->path.path.route);
                        latlon2unit(lat, lon,
                                data->path.path.route.tail->unitx,
                                data->path.path.route.tail->unity);
                    }
                    data->state = INSIDE_PATH_POINT;
                }
                else
                    data->state = ERROR;
            }
            else
                MACRO_SET_UNKNOWN();
            break;
        case INSIDE_PATH_POINT:
            /* only parse time/ele for tracks */
            if(data->path.path_type == TRACK)
            {
                if(!strcmp((gchar*)name, "time"))
                    data->state = INSIDE_PATH_POINT_TIME;
                else if(!strcmp((gchar*)name, "ele"))
                    data->state = INSIDE_PATH_POINT_ELE;
            }
            /* only parse description for routes */
            else if(data->path.path_type == ROUTE
                    && !strcmp((gchar*)name, "desc"))
                data->state = INSIDE_PATH_POINT_DESC;

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
        case INSIDE_PATH:
            if(!strcmp((gchar*)name, "trk"))
                data->state = INSIDE_GPX;
            else
                data->state = ERROR;
            break;
        case INSIDE_PATH_SEGMENT:
            if(!strcmp((gchar*)name, "trkseg"))
            {
                if(data->at_least_one_trkpt)
                {
                    if(data->path.path_type == TRACK)
                    {
                        MACRO_TRACK_INCREMENT_TAIL(data->path.path.track);
                        *data->path.path.track.tail = _track_null;
                    }
                    else
                    {
                        MACRO_ROUTE_INCREMENT_TAIL(data->path.path.route);
                        *data->path.path.route.tail = _pos_null;
                    }
                }
                data->state = INSIDE_PATH;
            }
            else
                data->state = ERROR;
            break;
        case INSIDE_PATH_POINT:
            if(!strcmp((gchar*)name, "trkpt"))
            {
                data->state = INSIDE_PATH_SEGMENT;
                data->at_least_one_trkpt = TRUE;
            }
            else
                data->state = ERROR;
            break;
        case INSIDE_PATH_POINT_ELE:
            /* only parse time for tracks */
            if(!strcmp((gchar*)name, "ele"))
            {
                gchar *error_check;
                data->path.path.track.tail->altitude
                    = g_ascii_strtod(data->chars->str, &error_check);
                if(error_check == data->chars->str)
                    data->path.path.track.tail->altitude = NAN;
                printf("str: %s\n", data->chars->str);
                printf("alt: %f\n", data->path.path.track.tail->altitude);
                data->state = INSIDE_PATH_POINT;
                g_string_free(data->chars, TRUE);
                data->chars = g_string_new("");
            }
            else
                data->state = ERROR;
            break;
        case INSIDE_PATH_POINT_TIME:
            /* only parse time for tracks */
            if(!strcmp((gchar*)name, "time"))
            {
                struct tm time;
                gchar *ptr;

                if(NULL == (ptr = strptime(data->chars->str,
                            XML_DATE_FORMAT, &time)))
                    /* Failed to parse dateTime format. */
                    data->state = ERROR;
                else
                {
                    /* Parse was successful. Now we have to parse timezone.
                     * From here on, if there is an error, I just assume local
                     * timezone.  Yes, this is not proper XML, but I don't
                     * care. */
                    gchar *error_check;

                    /* First, set time in "local" time zone. */
                    data->path.path.track.tail->time = (mktime(&time));

                    /* Now, skip inconsequential characters */
                    while(*ptr && *ptr != 'Z' && *ptr != '-' && *ptr != '+')
                        ptr++;

                    /* Check if we ran to the end of the string. */
                    if(*ptr)
                    {
                        /* Next character is either 'Z', '-', or '+' */
                        if(*ptr == 'Z')
                            /* Zulu (UTC) time. Undo the local time zone's
                             * offset. */
                            data->path.path.track.tail->time += time.tm_gmtoff;
                        else
                        {
                            /* Not Zulu (UTC). Must parse hours and minutes. */
                            gint offhours = strtol(ptr, &error_check, 10);
                            if(error_check != ptr
                                    && *(ptr = error_check) == ':')
                            {
                                /* Parse of hours worked. Check minutes. */
                                gint offmins = strtol(ptr + 1,
                                        &error_check, 10);
                                if(error_check != (ptr + 1))
                                {
                                    /* Parse of minutes worked. Calculate. */
                                    data->path.path.track.tail->time
                                        += (time.tm_gmtoff
                                                - (offhours * 60 * 60
                                                    + offmins * 60));
                                }
                            }
                        }
                    }
                    /* Successfully parsed dateTime. */
                    data->state = INSIDE_PATH_POINT;
                }

                g_string_free(data->chars, TRUE);
                data->chars = g_string_new("");
            }
            else
                data->state = ERROR;
            break;
        case INSIDE_PATH_POINT_DESC:
            /* only parse description for routes */
            if(!strcmp((gchar*)name, "desc"))
            {
                MACRO_ROUTE_INCREMENT_WTAIL(data->path.path.route);
                data->path.path.route.wtail->point= data->path.path.route.tail;
                data->path.path.route.wtail->desc
                    = g_string_free(data->chars, FALSE);
                data->chars = g_string_new("");
                data->state = INSIDE_PATH_POINT;
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
        case UNKNOWN:
            break;
        case INSIDE_PATH_POINT_ELE:
        case INSIDE_PATH_POINT_TIME:
        case INSIDE_PATH_POINT_DESC:
            for(i = 0; i < len; i++)
                data->chars = g_string_append_c(data->chars, ch[i]);
            vprintf("%s\n", data->chars->str);
            break;
        default:
            break;
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
parse_track_gpx(gchar *buffer, gint size, gint policy_old)
{
    SaxData data;
    xmlSAXHandler sax_handler;
    printf("%s()\n", __PRETTY_FUNCTION__);

    data.path.path_type = TRACK;
    MACRO_INIT_TRACK(data.path.path.track);
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
    if(policy_old && _track.head)
    {
        TrackPoint *src_first;
        Track *src, *dest;

        if(policy_old > 0)
        {
            /* Append to current track. */
            src = &data.path.path.track;
            dest = &_track;
        }
        else
        {
            /* Prepend to current track. */
            src = &_track;
            dest = &data.path.path.track;
        }

        /* Find src_first non-zero point. */
        for(src_first = src->head - 1; src_first++ != src->tail; )
            if(src_first->point.unity)
                break;

        /* Append track points from src to dest. */
        if(src->tail >= src_first)
        {
            guint num_dest_points = dest->tail - dest->head + 1;
            guint num_src_points = src->tail - src_first + 1;

            /* Adjust dest->tail to be able to fit src track data
             * plus room for more track data. */
            track_resize(dest,
                    num_dest_points + num_src_points + ARRAY_CHUNK_SIZE);

            memcpy(dest->tail + 1, src_first,
                    num_src_points * sizeof(TrackPoint));

            dest->tail += num_src_points;
        }

        MACRO_CLEAR_TRACK(*src);
        if(policy_old < 0)
            _track = *dest;
    }
    else
    {
        MACRO_CLEAR_TRACK(_track);
        /* Overwrite with data.track. */
        _track = data.path.path.track;
        track_resize(&_track,
                _track.tail - _track.head + 1 + ARRAY_CHUNK_SIZE);
    }

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
parse_route_gpx(gchar *buffer, gint size, gint policy_old)
{
    SaxData data;
    xmlSAXHandler sax_handler;
    printf("%s()\n", __PRETTY_FUNCTION__);

    data.path.path_type = ROUTE;
    MACRO_INIT_ROUTE(data.path.path.route);
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

    if(policy_old && _route.head)
    {
        Point *src_first;
        Route *src, *dest;

        if(policy_old > 0)
        {
            /* Append to current route. */
            src = &data.path.path.route;
            dest = &_route;
        }
        else
        {
            /* Prepend to current route. */
            src = &_route;
            dest = &data.path.path.route;
        }

        /* Find src_first non-zero point. */
        for(src_first = src->head - 1; src_first++ != src->tail; )
            if(src_first->unity)
                break;

        /* Append route points from src to dest. */
        if(src->tail >= src_first)
        {
            WayPoint *curr;
            guint num_dest_points = dest->tail - dest->head + 1;
            guint num_src_points = src->tail - src_first + 1;

            /* Adjust dest->tail to be able to fit src route data
             * plus room for more route data. */
            route_resize(dest, num_dest_points + num_src_points);

            memcpy(dest->tail + 1, src_first,
                    num_src_points * sizeof(Point));

            dest->tail += num_src_points;

            /* Append waypoints from src to dest->. */
            route_wresize(dest, (dest->wtail - dest->whead)
                    + (src->wtail - src->whead) + 2);
            for(curr = src->whead - 1; curr++ != src->wtail; )
            {
                (++(dest->wtail))->point = dest->head + num_dest_points
                    + (curr->point - src_first);
                dest->wtail->desc = curr->desc;
            }

        }

        /* Kill old route - don't use MACRO_CLEAR_ROUTE(), because that
         * would free the string desc's that we just moved to data.route. */
        g_free(src->head);
        g_free(src->whead);
        if(policy_old < 0)
            _route = *dest;
    }
    else
    {
        MACRO_CLEAR_ROUTE(_route);
        /* Overwrite with data.route. */
        _route = data.path.path.route;
        route_resize(&_route, _route.tail - _route.head + 1);
        route_wresize(&_route, _route.wtail - _route.whead + 1);
    }

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

/****************************************************************************
 * ABOVE: FILE-HANDLING ROUTINES *********************************************
 ****************************************************************************/


/****************************************************************************
 * BELOW: ROUTINES **********************************************************
 ****************************************************************************/

static void
deg_format(gfloat coor, gchar scoor[15])
{
    gint deg;
    gfloat min, sec;
    printf("%s()\n", __PRETTY_FUNCTION__);

    switch(_degformat)
    {
        case DD_MMPMMM:
            sprintf(scoor, "%d\u00b0%06.3f'",
                    (int)coor, (coor - (int)coor)*60.0);
            break;
        case DD_MM_SSPS:
            deg = (int)coor;
            min = (coor - (int)coor)*60.0;
            sec = (min - (int)min)*60.0;
            sprintf(scoor, "%d\u00b0%02d'%04.1f\"", deg, (int) min, sec);
            break;
        default:
            sprintf(scoor, "%.5f\u00b0", coor);
            break;
    }
    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

static gboolean
gps_display_details(void)
{
    gchar *buffer, strtime[15];
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(_gps.fix < 2)
    {
        /* no fix no fun */
        gtk_label_set_label(GTK_LABEL(_sdi_lat), " --- ");
        gtk_label_set_label(GTK_LABEL(_sdi_lon), " --- ");
        gtk_label_set_label(GTK_LABEL(_sdi_spd), " --- ");
        gtk_label_set_label(GTK_LABEL(_sdi_alt), " --- ");
        gtk_label_set_label(GTK_LABEL(_sdi_hea), " --- ");
        gtk_label_set_label(GTK_LABEL(_sdi_tim), " --:--:-- ");
    }
    else
    {
        gfloat speed = _gps.speed * UNITS_CONVERT[_units];

        /* latitude */
        gtk_label_set_label(GTK_LABEL(_sdi_lat), _gps.slatitude);

        /* longitude */
        gtk_label_set_label(GTK_LABEL(_sdi_lon), _gps.slongitude);

        /* speed */
        switch(_units)
        {
            case UNITS_MI:
                buffer = g_strdup_printf("%.1f mph", speed);
                break;
            case UNITS_NM:
                buffer = g_strdup_printf("%.1f kn", speed);
                break;
            default:
                buffer = g_strdup_printf("%.1f km/h", speed);
                break;
        }
        gtk_label_set_label(GTK_LABEL(_sdi_spd), buffer);
        g_free(buffer);

        /* altitude */
        switch(_units)
        {
            case UNITS_MI:
            case UNITS_NM:
                buffer = g_strdup_printf("%.1f ft",
                        _gps.altitude * 3.2808399f);
                break;
            default:
                buffer = g_strdup_printf("%.1f m", _gps.altitude);
                break;
        }
        gtk_label_set_label(GTK_LABEL(_sdi_alt), buffer);
        g_free(buffer);

        /* heading */
        buffer = g_strdup_printf("%0.0f\u00b0", _gps.heading);
        gtk_label_set_label(GTK_LABEL(_sdi_hea), buffer);
        g_free(buffer);

        /* local time */
        strftime(strtime, 15, "%X", &_gps.timeloc);
        gtk_label_set_label(GTK_LABEL(_sdi_tim), strtime);
    }

    /* Sat in view */
    buffer = g_strdup_printf("%d", _gps.satinview);
    gtk_label_set_label(GTK_LABEL(_sdi_vie), buffer);
    g_free(buffer);

    /* Sat in use */
    buffer = g_strdup_printf("%d", _gps.satinuse);
    gtk_label_set_label(GTK_LABEL(_sdi_use), buffer);
    g_free(buffer);

    /* fix */
    switch(_gps.fix)
    {
        case 2:
        case 3: buffer = g_strdup_printf("%dD fix", _gps.fix); break;
        default: buffer = g_strdup_printf("nofix"); break;
    }
    gtk_label_set_label(GTK_LABEL(_sdi_fix), buffer);
    g_free(buffer);

    if(_gps.fix == 1)
        buffer = g_strdup("none");
    else
    {
        switch (_gps.fixquality)
        {
            case 1 : buffer = g_strdup_printf(_("SPS")); break;
            case 2 : buffer = g_strdup_printf(_("DGPS")); break;
            case 3 : buffer = g_strdup_printf(_("PPS")); break;
            case 4 : buffer = g_strdup_printf(_("Real Time Kinematic")); break;
            case 5 : buffer = g_strdup_printf(_("Float RTK")); break;
            case 6 : buffer = g_strdup_printf(_("Estimated")); break;
            case 7 : buffer = g_strdup_printf(_("Manual)")); break;
            case 8 : buffer = g_strdup_printf(_("Simulation")); break;
            default : buffer = g_strdup_printf(_("none")); break;
        }
    }
    gtk_label_set_label(GTK_LABEL(_sdi_fqu), buffer);
    g_free(buffer);

    /* max speed */
    {
        gfloat maxspeed = _gps.maxspeed * UNITS_CONVERT[_units];

        /* speed */
        switch(_units)
        {
            case UNITS_MI:
                buffer = g_strdup_printf("%.1f mph", maxspeed);
                break;
            case UNITS_NM:
                buffer = g_strdup_printf("%.1f kn", maxspeed);
                break;
            default:
                buffer = g_strdup_printf("%.1f km/h", maxspeed);
                break;
        }
        gtk_label_set_label(GTK_LABEL(_sdi_msp), buffer);
        g_free(buffer);
    }

    /* refresh sat panel */
    gtk_widget_queue_draw_area(GTK_WIDGET(_sat_details_panel),
        0, 0,
        _sat_details_panel->allocation.width,
        _sat_details_panel->allocation.height);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static void
gps_display_data(void)
{
    gchar *buffer, strtime[15];
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(_gps.fix < 2)
    {
        /* no fix no fun */
        gtk_label_set_label(GTK_LABEL(_text_lat), " --- ");
        gtk_label_set_label(GTK_LABEL(_text_lon), " --- ");
        gtk_label_set_label(GTK_LABEL(_text_speed), " --- ");
        gtk_label_set_label(GTK_LABEL(_text_alt), " --- ");
        gtk_label_set_label(GTK_LABEL(_text_time), " --:--:-- ");
    }
    else
    {
        gfloat speed = _gps.speed * UNITS_CONVERT[_units];

        /* latitude */
        gtk_label_set_label(GTK_LABEL(_text_lat), _gps.slatitude);

        /* longitude */
        gtk_label_set_label(GTK_LABEL(_text_lon), _gps.slongitude);

        /* speed */
        switch(_units)
        {
            case UNITS_MI:
                buffer = g_strdup_printf("Spd: %.1f mph", speed);
                break;
            case UNITS_NM:
                buffer = g_strdup_printf("Spd: %.1f kn", speed);
                break;
            default:
                buffer = g_strdup_printf("Spd: %.1f km/h", speed);
                break;
        }
        gtk_label_set_label(GTK_LABEL(_text_speed), buffer);
        g_free(buffer);

        /* altitude */
        switch(_units)
        {
            case UNITS_MI:
            case UNITS_NM:
                buffer = g_strdup_printf("Alt: %.1f ft",
                        _gps.altitude * 3.2808399f);
                break;
            default:
                buffer = g_strdup_printf("Alt: %.1f m", _gps.altitude);
        }
        gtk_label_set_label(GTK_LABEL(_text_alt), buffer);
        g_free(buffer);

        /* local time */
        strftime(strtime, 15, "%X", &_gps.timeloc);
        gtk_label_set_label(GTK_LABEL(_text_time), strtime);
    }

    /* refresh sat panel */
    gtk_widget_queue_draw_area(GTK_WIDGET(_sat_panel),
        0, 0,
        _sat_panel->allocation.width,
        _sat_panel->allocation.height);

    /* refresh heading panel*/
    gtk_widget_queue_draw_area(GTK_WIDGET(_heading_panel),
        0, 0,
        _heading_panel->allocation.width,
        _heading_panel->allocation.height);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return;
}

static void
gps_hide_text(void)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    /* Clear gps data */
    _gps.fix = 1;
    _gps.satinuse = 0;
    _gps.satinview = 0;

    if(_gps_info)
        gps_display_data();

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

static void
gps_show_info(void)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(_gps_info && _enable_gps)
        gtk_widget_show_all(GTK_WIDGET(_gps_widget));
    else
    {
        gps_hide_text();
        gtk_widget_hide_all(GTK_WIDGET(_gps_widget));
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

static void
draw_sat_info(GtkWidget *widget, guint x0, guint y0,
        guint width, guint height, gboolean showsnr)
{
    PangoContext *context = NULL;
    PangoLayout *layout = NULL;
    PangoFontDescription *fontdesc = NULL;
    GdkColor color;
    GdkGC *gc1, *gc2, *gc;
    guint step, i, j, snr_height, bymargin, xoffset, yoffset;
    guint x, y, x1, y1;
    gchar *tmp = NULL;
    printf("%s()\n", __PRETTY_FUNCTION__);

    xoffset = x0;
    yoffset = y0;
    /* Bootom margin - 12% */
    bymargin = height * 0.88f;

    /* Bottom line */
    gdk_draw_line(widget->window,
        widget->style->fg_gc[GTK_WIDGET_STATE (widget)],
        xoffset + 5, yoffset + bymargin,
        xoffset + width - 10 - 2, yoffset + bymargin);
    gdk_draw_line(widget->window,
        widget->style->fg_gc[GTK_WIDGET_STATE (widget)],
        xoffset + 5, yoffset + bymargin - 1,
        xoffset + width - 10 - 2, yoffset + bymargin - 1);

    context = gtk_widget_get_pango_context(widget);
    layout = pango_layout_new(context);
    fontdesc =  pango_font_description_new();

    pango_font_description_set_family(fontdesc,"Sans Serif");

    if(_gps.satinview > 0 )
    {
        pango_font_description_set_size(fontdesc, 8*PANGO_SCALE);

        pango_layout_set_font_description (layout, fontdesc);
        pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);

        /* Left margin - 5pix, Right margin - 5pix */
        step = (width - 10) / _gps.satinview;

        color.red = 0;
        color.green = 0;
        color.blue = 0;
        gc1 = gdk_gc_new (widget->window);
        gdk_gc_set_rgb_fg_color (gc1, &color);

        color.red = 0;
        color.green = 0;
        color.blue = 0xffff;
        gc2 = gdk_gc_new (widget->window);
        gdk_gc_set_rgb_fg_color (gc2, &color);

        for(i = 0; i < _gps.satinview; i++)
        {
            /* Sat used or not */
            gc = gc1;
            for(j = 0; j < _gps.satinuse ; j++)
            {
                if(_gps.satforfix[j] == _gps_sat[i].prn)
                {
                    gc = gc2;
                    break;
                }
            }

            x = 5 + i * step;
            snr_height = _gps_sat[i].snr * height * 0.78f / 100;
            y = height * 0.1f + (height * 0.78f - snr_height);

            /* draw sat rectangle... */
            gdk_draw_rectangle(widget->window,
                    gc,
                    TRUE,
                    xoffset + x,
                    yoffset + y,
                    step - 2,
                    snr_height);

            if(showsnr && _gps_sat[i].snr > 0)
            {
                /* ...snr.. */
                tmp = g_strdup_printf("%02d", _gps_sat[i].snr);
                pango_layout_set_text(layout, tmp, 2);
                pango_layout_get_pixel_size(layout, &x1, &y1);
                gdk_draw_layout(widget->window,
                    widget->style->fg_gc[GTK_STATE_NORMAL],
                    xoffset + x + ((step - 2) - x1)/2,
                    yoffset + y - 15,
                    layout);
                g_free(tmp);
            }

            /* ...and sat number */
            tmp = g_strdup_printf("%02d", _gps_sat[i].prn);
            pango_layout_set_text(layout, tmp, 2);
            pango_layout_get_pixel_size(layout, &x1, &y1);
            gdk_draw_layout(widget->window,
                widget->style->fg_gc[GTK_STATE_NORMAL],
                xoffset + x + ((step - 2) - x1)/2 ,
                yoffset + bymargin + 1,
                layout);
            g_free(tmp);
        }
        g_object_unref (gc1);
        g_object_unref (gc2);
    }

    pango_font_description_free (fontdesc);
    g_object_unref (layout);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return;
}

static void
draw_sat_details(GtkWidget *widget, guint x0, guint y0,
        guint width, guint height)
{
    guint i, j, x, y, size, halfsize, xoffset, yoffset;
    guint x1, y1;
    gfloat tmp;
    GdkColor color;
    GdkGC *gc1, *gc2, *gc3, *gc;
    PangoContext *context = NULL;
    PangoLayout *layout = NULL;
    PangoFontDescription *fontdesc = NULL;
    gchar *buffer = NULL;
    printf("%s()\n", __PRETTY_FUNCTION__);

    size = MIN(width, height);
    halfsize = size/2;
    if(width > height)
    {
        xoffset = x0 + (width - height - 10) / 2;
        yoffset = y0 + 5;
    }
    else
    {
        xoffset = x0 + 5;
        yoffset = y0 + (height - width - 10) / 2;
    }

    context = gtk_widget_get_pango_context(widget);
    layout = pango_layout_new(context);
    fontdesc =  pango_font_description_new();

    pango_font_description_set_family(fontdesc,"Sans Serif");
    pango_font_description_set_size(fontdesc, 10*PANGO_SCALE);
    pango_layout_set_font_description (layout, fontdesc);
    pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);

    /* 90 */
    gdk_draw_arc(widget->window,
            widget->style->fg_gc[GTK_WIDGET_STATE(widget)],
            FALSE,
            xoffset + 2, yoffset + 2, size - 4, size - 4,
            0, 64 * 360);

    /* 60 */
    gdk_draw_arc(widget->window,
            widget->style->fg_gc[GTK_WIDGET_STATE(widget)],
            FALSE,
            xoffset + size/6, yoffset + size/6,
            size/6*4, size/6*4,
            0, 64 * 360);

    /* 30 */
    gdk_draw_arc(widget->window,
            widget->style->fg_gc[GTK_WIDGET_STATE(widget)],
            FALSE,
            xoffset + size/6*2, yoffset + size/6*2,
            size/6*2, size/6*2,
            0, 64 * 360);

    guint line[12] = {0,30,60,90,120,150,180,210,240,270,300,330};

    for(i = 0; i < 6; i++)
    {
        /* line */
        tmp = (line[i] * (1.f / 180.f)) * PI;
        gdk_draw_line(widget->window,
            widget->style->fg_gc[GTK_WIDGET_STATE (widget)],
            xoffset + halfsize + (halfsize -2) * sinf(tmp),
            yoffset + halfsize - (halfsize -2) * cosf(tmp),
            xoffset + halfsize - (halfsize -2) * sinf(tmp),
            yoffset + halfsize + (halfsize -2) * cosf(tmp));
    }

    for(i = 0; i < 12; i++)
    {
        tmp = (line[i] * (1.f / 180.f)) * PI;
        /* azimuth */
        if(line[i] == 0)
            buffer = g_strdup_printf("N");
        else
            buffer = g_strdup_printf("%d\u00b0", line[i]);
        pango_layout_set_text(layout, buffer, strlen(buffer));
        pango_layout_get_pixel_size(layout, &x, &y);
        gdk_draw_layout(widget->window,
            widget->style->fg_gc[GTK_STATE_NORMAL],
            (xoffset + halfsize + (halfsize - size/12) * sinf(tmp)) - x/2,
            (yoffset + halfsize - (halfsize - size/12) * cosf(tmp)) - y/2,
            layout);
        g_free(buffer);
    }

    /* elevation 30 */
    tmp = (30 * (1.f / 180.f)) * PI;
    buffer = g_strdup_printf("30\u00b0");
    pango_layout_set_text(layout, buffer, strlen(buffer));
    pango_layout_get_pixel_size(layout, &x, &y);
    gdk_draw_layout(widget->window,
        widget->style->fg_gc[GTK_STATE_NORMAL],
        (xoffset + halfsize + size/6*2 * sinf(tmp)) - x/2,
        (yoffset + halfsize - size/6*2 * cosf(tmp)) - y/2,
        layout);
    g_free(buffer);

    /* elevation 60 */
    tmp = (30 * (1.f / 180.f)) * PI;
    buffer = g_strdup_printf("60\u00b0");
    pango_layout_set_text(layout, buffer, strlen(buffer));
    pango_layout_get_pixel_size(layout, &x, &y);
    gdk_draw_layout(widget->window,
        widget->style->fg_gc[GTK_STATE_NORMAL],
        (xoffset + halfsize + size/6 * sinf(tmp)) - x/2,
        (yoffset + halfsize - size/6 * cosf(tmp)) - y/2,
        layout);
    g_free(buffer);

    color.red = 0;
    color.green = 0;
    color.blue = 0;
    gc1 = gdk_gc_new (widget->window);
    gdk_gc_set_rgb_fg_color (gc1, &color);

    color.red = 0;
    color.green = 0;
    color.blue = 0xffff;
    gc2 = gdk_gc_new (widget->window);
    gdk_gc_set_rgb_fg_color (gc2, &color);

    color.red = 0xffff;
    color.green = 0xffff;
    color.blue = 0xffff;
    gc3 = gdk_gc_new (widget->window);
    gdk_gc_set_rgb_fg_color (gc3, &color);

    for(i = 0; i < _gps.satinview; i++)
    {
        /* Sat used or not */
        gc = gc1;
        for(j = 0; j < _gps.satinuse ; j++)
        {
            if(_gps.satforfix[j] == _gps_sat[i].prn)
            {
                gc = gc2;
                break;
            }
        }

        tmp = (_gps_sat[i].azimuth * (1.f / 180.f)) * PI;
        x = xoffset + halfsize
            + (90 - _gps_sat[i].elevation)*halfsize/90 * sinf(tmp);
        y = yoffset + halfsize
            - (90 - _gps_sat[i].elevation)*halfsize/90 * cosf(tmp);

        gdk_draw_arc (widget->window,
            gc, TRUE,
            x - 10, y - 10, 20, 20,
            0, 64 * 360);

        buffer = g_strdup_printf("%02d", _gps_sat[i].prn);
        pango_layout_set_text(layout, buffer, strlen(buffer));
        pango_layout_get_pixel_size(layout, &x1, &y1);
        gdk_draw_layout(widget->window,
            gc3,
            x - x1/2,
            y - y1/2,
            layout);
        g_free(buffer);
    }
    g_object_unref (gc1);
    g_object_unref (gc2);
    g_object_unref (gc3);

    pango_font_description_free (fontdesc);
    g_object_unref (layout);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return;
}


static gboolean
sat_details_panel_expose(GtkWidget *widget, GdkEventExpose *event)
{
    guint width, height, x, y;
    PangoContext *context = NULL;
    PangoLayout *layout = NULL;
    PangoFontDescription *fontdesc = NULL;
    gchar *buffer = NULL;
    printf("%s()\n", __PRETTY_FUNCTION__);

    width = widget->allocation.width;
    height = widget->allocation.height * 0.9;

    draw_sat_info(widget, 0, 0, width/2, height, TRUE);
    draw_sat_details(widget, width/2, 0, width/2, height);

    context = gtk_widget_get_pango_context(widget);
    layout = pango_layout_new(context);
    fontdesc =  pango_font_description_new();

    pango_font_description_set_family(fontdesc,"Sans Serif");
    pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);
    pango_font_description_set_size(fontdesc, 14*PANGO_SCALE);
    pango_layout_set_font_description (layout, fontdesc);

    buffer = g_strdup_printf(
        "%s: %d; %s: %d",
        _("Satellites in view"), _gps.satinview,
        _("in use"), _gps.satinuse);
    pango_layout_set_text(layout, buffer, strlen(buffer));
    pango_layout_get_pixel_size(layout, &x, &y);
    gdk_draw_layout(widget->window,
        widget->style->fg_gc[GTK_STATE_NORMAL],
        10,
        height*0.9 + 10,
        layout);
    g_free(buffer);

    buffer = g_strdup_printf("HDOP: %.01f", _gps.hdop);
    pango_layout_set_text(layout, buffer, strlen(buffer));
    pango_layout_get_pixel_size(layout, &x, &y);
    gdk_draw_layout(widget->window,
        widget->style->fg_gc[GTK_STATE_NORMAL],
        (width/8) - x/2,
        (height/6) - y/2,
        layout);
    g_free(buffer);
    buffer = g_strdup_printf("PDOP: %.01f", _gps.pdop);
    pango_layout_set_text(layout, buffer, strlen(buffer));
    pango_layout_get_pixel_size(layout, &x, &y);
    gdk_draw_layout(widget->window,
        widget->style->fg_gc[GTK_STATE_NORMAL],
        (width/8) - x/2,
        (height/6) - y/2 + 20,
        layout);
    g_free(buffer);
    buffer = g_strdup_printf("VDOP: %.01f", _gps.vdop);
    pango_layout_set_text(layout, buffer, strlen(buffer));
    pango_layout_get_pixel_size(layout, &x, &y);
    gdk_draw_layout(widget->window,
        widget->style->fg_gc[GTK_STATE_NORMAL],
        (width/8) - x/2,
        (height/6) - y/2 + 40,
        layout);
    g_free(buffer);

    pango_font_description_free (fontdesc);
    g_object_unref (layout);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static void
gps_details(void)
{
    GtkWidget *dialog;
    GtkWidget *table;
    GtkWidget *label;
    GtkWidget *notebook;
    printf("%s()\n", __PRETTY_FUNCTION__);

    dialog = gtk_dialog_new_with_buttons(_("GPS Details"),
            GTK_WINDOW(_window), GTK_DIALOG_MODAL,
            GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
            NULL);

    gtk_window_set_default_size(GTK_WINDOW(dialog), 600, 300);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
            notebook = gtk_notebook_new(), TRUE, TRUE, 0);

    /* textual info */
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
            table = gtk_table_new(4, 6, FALSE),
            label = gtk_label_new(_("GPS Information")));

    _sat_details_panel = gtk_drawing_area_new ();
    gtk_widget_set_size_request (_sat_details_panel, 300, 300);
    /* sat details info */
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
            _sat_details_panel,
            label = gtk_label_new(_("Satellites details")));
    g_signal_connect (G_OBJECT (_sat_details_panel), "expose_event",
                        G_CALLBACK (sat_details_panel_expose), NULL);

    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new(_("Latitude")),
            0, 1, 0, 1, GTK_EXPAND | GTK_FILL, 0, 20, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(table),
            _sdi_lat = gtk_label_new(" --- "),
            1, 2, 0, 1, GTK_EXPAND | GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(_sdi_lat), 0.f, 0.5f);

    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new(_("Longitude")),
            0, 1, 1, 2, GTK_EXPAND | GTK_FILL, 0, 20, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(table),
            _sdi_lon = gtk_label_new(" --- "),
            1, 2, 1, 2, GTK_EXPAND | GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(_sdi_lon), 0.f, 0.5f);

    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new(_("Speed")),
            0, 1, 2, 3, GTK_EXPAND | GTK_FILL, 0, 20, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(table),
            _sdi_spd = gtk_label_new(" --- "),
            1, 2, 2, 3, GTK_EXPAND | GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(_sdi_spd), 0.f, 0.5f);

    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new(_("Altitude")),
            0, 1, 3, 4, GTK_EXPAND | GTK_FILL, 0, 20, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(table),
            _sdi_alt = gtk_label_new(" --- "),
            1, 2, 3, 4, GTK_EXPAND | GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(_sdi_alt), 0.f, 0.5f);

    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new(_("Heading")),
            0, 1, 4, 5, GTK_EXPAND | GTK_FILL, 0, 20, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(table),
            _sdi_hea = gtk_label_new(" --- "),
            1, 2, 4, 5, GTK_EXPAND | GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(_sdi_hea), 0.f, 0.5f);

    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new(_("Local time")),
            0, 1, 5, 6, GTK_EXPAND | GTK_FILL, 0, 20, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(table),
            _sdi_tim = gtk_label_new(" --:--:-- "),
            1, 2, 5, 6, GTK_EXPAND | GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(_sdi_tim), 0.f, 0.5f);

    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new(_("Sat in view")),
            2, 3, 0, 1, GTK_EXPAND | GTK_FILL, 0, 20, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(table),
            _sdi_vie = gtk_label_new("0"),
            3, 4, 0, 1, GTK_EXPAND | GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(_sdi_vie), 0.f, 0.5f);

    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new(_("Sat in use")),
            2, 3, 1, 2, GTK_EXPAND | GTK_FILL, 0, 20, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(table),
            _sdi_use = gtk_label_new("0"),
            3, 4, 1, 2, GTK_EXPAND | GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(_sdi_use), 0.f, 0.5f);

    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new(_("Fix")),
            2, 3, 2, 3, GTK_EXPAND | GTK_FILL, 0, 20, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(table),
            _sdi_fix = gtk_label_new(_("nofix")),
            3, 4, 2, 3, GTK_EXPAND | GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(_sdi_fix), 0.f, 0.5f);

    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new(_("Fix Quality")),
            2, 3, 3, 4, GTK_EXPAND | GTK_FILL, 0, 20, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(table),
            _sdi_fqu = gtk_label_new(_("none")),
            3, 4, 3, 4, GTK_EXPAND | GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(_sdi_fix), 0.f, 0.5f);

    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new(_("Max speed")),
            2, 3, 5, 6, GTK_EXPAND | GTK_FILL, 0, 20, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(table),
            _sdi_msp = gtk_label_new(" --- "),
            3, 4, 5, 6, GTK_EXPAND | GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(_sdi_msp), 0.f, 0.5f);

    gtk_widget_show_all(dialog);
    _satdetails_on = TRUE;
    gps_display_details();
    while(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(dialog)))
    {
        _satdetails_on = FALSE;
        break;
    }
    gtk_widget_destroy(dialog);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

static gfloat
calculate_distance(gfloat lat1, gfloat lon1, gfloat lat2, gfloat lon2)
{
    gfloat dlat, dlon, slat, slon, a;

    /* Convert to radians. */
    lat1 *= (PI / 180.f);
    lon1 *= (PI / 180.f);
    lat2 *= (PI / 180.f);
    lon2 *= (PI / 180.f);

    dlat = lat2 - lat1;
    dlon = lon2 - lon1;

    slat = sinf(dlat / 2.f);
    slon = sinf(dlon / 2.f);
    a = (slat * slat) + (cosf(lat1) * cosf(lat2) * slon * slon);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return ((2.f * atan2f(sqrtf(a), sqrtf(1.f - a))) * EARTH_RADIUS)
        * UNITS_CONVERT[_units];
}

static void
db_connect()
{
    gchar buffer[100];
    gchar *perror;
    gchar **pszResult;
    guint nRow, nColumn;
    printf("%s()\n", __PRETTY_FUNCTION__);

    _dbconn = FALSE;

    if(!_poi_db)
        return;

    if(NULL == (_db = sqlite_open(_poi_db, 0666, &perror)))
    {
        gchar buffer2[200];
        snprintf(buffer2, sizeof(buffer2),
                "%s: %s", _("Problem with POI database"), perror);
        popup_error(_window, buffer2);
        g_free(perror);
        return;
    }

    if(SQLITE_OK != sqlite_get_table(_db, "select label from poi limit 1",
        &pszResult, &nRow, &nColumn, NULL))
    {
        if(SQLITE_OK != sqlite_exec(_db,
                /* Create the necessary tables... */
                "create table poi (poi_id integer PRIMARY KEY, lat real, "
                "lon real, label text, desc text, cat_id integer);"
                "create table category (cat_id integer PRIMARY KEY,"
                "label text, desc text, enabled integer);"
                /* Add some default categories... */
                "insert into category (label, desc, enabled) "
                "values ('Fuel',"
                  "'Stations for purchasing fuel for vehicles.', 1);"
                "insert into category (label, desc, enabled) "
                "values ('Residence', "
                  "'Houses, apartments, or other residences of import.', 1);"
                "insert into category (label, desc, enabled) "
                "values ('Dining', "
                  "'Places to eat or drink.', 1);"
                "insert into category (label, desc, enabled) "
                "values ('Shopping/Services', "
                  "'Places to shop or acquire services.', 1);"
                "insert into category (label, desc, enabled) "
                "values ('Recreation', "
                  "'Indoor or Outdoor places to have fun.', 1);"
                "insert into category (label, desc, enabled) "
                "values ('Transportation', "
                  "'Bus stops, airports, train stations, etc.', 1);"
                "insert into category (label, desc, enabled) "
                "values ('Lodging', "
                  "'Places to stay temporarily or for the night.', 1);"
                "insert into category (label, desc, enabled) "
                "values ('School', "
                  "'Elementary schools, college campuses, etc.', 1);"
                "insert into category (label, desc, enabled) "
                "values ('Business', "
                  "'General places of business.', 1);"
                "insert into category (label, desc, enabled) "
                "values ('Landmark', "
                  "'General landmarks.', 1);"
                "insert into category (label, desc, enabled) "
                "values ('Other', "
                  "'Miscellaneous category for everything else.', 1);",
                NULL,
                NULL,
                &perror)
                && (SQLITE_OK != sqlite_get_table(_db,
                            "select label from poi limit 1",
                            &pszResult, &nRow, &nColumn, NULL)))
        {
            snprintf(buffer, sizeof(buffer), "%s:\n%s",
                    _("Failed to open or create database"), _poi_db);
            popup_error(_window, buffer);
            sqlite_close(_db);
            return;
        }
    }
    else
        sqlite_free_table(pszResult);

    _dbconn = TRUE;
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
                        _window, NULL, _("Searching for GPS receiver"));
            break;
        case RCVR_UP:
            if(_connect_banner)
            {
                gtk_widget_destroy(_connect_banner);
                _connect_banner = NULL;
            }
            if(!_fix_banner)
                _fix_banner = hildon_banner_show_progress(
                        _window, NULL, _("Establishing GPS fix"));
            break;
        default: ; /* to quell warning. */
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

/**
 * Updates _near_point, _next_way, and _next_wpt.  If quick is FALSE (as
 * it is when this function is called from route_find_nearest_point), then
 * the entire list (starting from _near_point) is searched.  Otherwise, we
 * stop searching when we find a point that is farther away.
 */
static gboolean
route_update_nears(gboolean quick)
{
    gboolean ret = FALSE;
    Point *curr, *near;
    WayPoint *wcurr, *wnext;
    guint near_dist_rough;
    printf("%s(%d)\n", __PRETTY_FUNCTION__, quick);

    /* If we have waypoints (_next_way != NULL), then determine the "next
     * waypoint", which is defined as the waypoint after the nearest point,
     * UNLESS we've passed that waypoint, in which case the waypoint after
     * that waypoint becomes the "next" waypoint. */
    if(_next_way)
    {
        /* First, set near_dist_rough with the new distance from _near_point.*/
        near = _near_point;
        near_dist_rough = DISTANCE_ROUGH(_pos, *near);

        /* Now, search _route for a closer point.  If quick is TRUE, then we'll
         * only search forward, only as long as we keep finding closer points.
         */
        for(curr = _near_point; curr++ != _route.tail; )
        {
            if(curr->unity)
            {
                guint dist_rough = DISTANCE_ROUGH(_pos, *curr);
                if(dist_rough <= near_dist_rough)
                {
                    near = curr;
                    near_dist_rough = dist_rough;
                }
                else if(quick)
                    break;
            }
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
             * _next_wpt, and if the former is increasing and the latter is
             * decreasing, then we have passed the waypoint, and thus we
             * should skip it.  Note that if there is no _next_wpt, then
             * there is no next waypoint, so we do not skip it in that case. */
                || (wcurr->point == near && quick
                    && (_next_wpt
                     && (DISTANCE_ROUGH(_pos, *near) > _next_way_dist_rough
                      && DISTANCE_ROUGH(_pos, *_next_wpt)
                                                     < _next_wpt_dist_rough))))
                wnext = wcurr + 1;
            else
                break;
        }

        if(wnext == _route.wtail && (wnext->point < near
                || (wnext->point == near && quick
                    && (!_next_wpt
                     || (DISTANCE_ROUGH(_pos, *near) > _next_way_dist_rough
                      &&DISTANCE_ROUGH(_pos, *_next_wpt)
                                                 < _next_wpt_dist_rough)))))
        {
            _next_way = NULL;
            _next_wpt = NULL;
            _next_way_dist_rough = -1;
            _next_wpt_dist_rough = -1;
            ret = TRUE;
        }
        /* Only update _next_way (and consequently _next_wpt) if _next_way is
         * different, and record that fact for return. */
        else
        {
            if(!quick || _next_way != wnext)
            {
                _next_way = wnext;
                _next_wpt = wnext->point;
                if(_next_wpt == _route.tail)
                    _next_wpt = NULL;
                else
                {
                    while(!(++_next_wpt)->unity)
                    {
                        if(_next_wpt == _route.tail)
                        {
                            _next_wpt = NULL;
                            break;
                        }
                    }
                }
                ret = TRUE;
            }
            _next_way_dist_rough = DISTANCE_ROUGH(_pos, *wnext->point);
            if(_next_wpt)
                _next_wpt_dist_rough = DISTANCE_ROUGH(_pos, *_next_wpt);
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
    while(!_near_point->unity && _near_point != _route.tail)
        _near_point++;

    /* Initialize _next_way. */
    if(_route.wtail == _route.whead - 1
            || (_autoroute_data.enabled && _route.wtail == _route.whead))
        _next_way = NULL;
    else
        /* We have at least one waypoint. */
        _next_way = (_autoroute_data.enabled ? _route.whead+1 : _route.whead);
    _next_way_dist_rough = -1;

    /* Initialize _next_wpt. */
    _next_wpt = NULL;
    _next_wpt_dist_rough = -1;

    route_update_nears(FALSE);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

/**
 * Render a single track line to _map_pixmap.  If either point on the line
 * is a break (defined as unity == 0), a circle is drawn at the other point.
 * IT IS AN ERROR FOR BOTH POINTS TO INDICATE A BREAK.
 */
static void
map_render_segment(GdkGC *gc_norm, GdkGC *gc_alt,
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
            gdk_draw_arc(_map_pixmap, gc_alt,
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
            gdk_draw_arc(_map_pixmap, gc_alt,
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
map_render_track()
{
    TrackPoint *curr;
    printf("%s()\n", __PRETTY_FUNCTION__);

    for(curr = _track.head; curr != _track.tail; curr++)
        map_render_segment(_gc_track, _gc_track_break,
                curr->point.unitx, curr->point.unity,
                curr[1].point.unitx, curr[1].point.unity);

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
map_render_route()
{
    Point *curr;
    WayPoint *wcurr;
    printf("%s()\n", __PRETTY_FUNCTION__);

    _visible_way_first = _visible_way_last = NULL;
    for(curr = _route.head, wcurr = _route.whead; curr != _route.tail; curr++)
    {
        if(wcurr && wcurr <= _route.wtail && curr == wcurr->point)
        {
            guint x1 = unit2bufx(wcurr->point->unitx);
            guint y1 = unit2bufy(wcurr->point->unity);
            if((x1 < BUF_WIDTH_PIXELS)
                    && (y1 < BUF_HEIGHT_PIXELS))
            {
                gdk_draw_arc(_map_pixmap,
                        (wcurr==_next_way ? _gc_route_nextway : _gc_route_way),
                        FALSE, /* FALSE: not filled. */
                        x1 - _draw_line_width,
                        y1 - _draw_line_width,
                        2 * _draw_line_width,
                        2 * _draw_line_width,
                        0, /* start at 0 degrees. */
                        360 * 64);
                if(!_visible_way_first)
                    _visible_way_first = wcurr;
                _visible_way_last = wcurr;
            }
            wcurr++;
            if(!curr[1].unity)
                continue;
        }
        map_render_segment(_gc_route, _gc_route_way,
                curr->unitx, curr->unity, curr[1].unitx, curr[1].unity);
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

static void
map_render_paths()
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if((_show_tracks & ROUTES_MASK) && _route.head)
    {
        map_render_route();
        if(_next_way == NULL)
        {
            guint x1 = unit2bufx(_route.tail[-1].unitx);
            guint y1 = unit2bufy(_route.tail[-1].unity);
            if((x1 < BUF_WIDTH_PIXELS)
                    && (y1 < BUF_HEIGHT_PIXELS))
            {
                gdk_draw_arc(_map_pixmap,
                        _gc_route_nextway,
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
        map_render_track();

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

/**
 * Add a point to the _track list.  This function is slightly overloaded,
 * since it is what houses the check for "have we moved
 * significantly": it also initiates the re-calculation of the _near_point
 * data, as well as calling osso_display_state_on() when we have the focus.
 *
 * If a non-zero time is given, then the current position (as taken from the
 * _pos variable) is appended to _track with the given time.  If time is zero,
 * then _pos_null is appended to _track with time zero (this produces a "break"
 * in the track).
 */
static void
track_add(time_t time, gboolean newly_fixed)
{
    Point pos = (time == 0 ? _pos_null : _pos);
    printf("%s(%u, %u)\n", __PRETTY_FUNCTION__, pos.unitx, pos.unity);

    if(abs((gint)pos.unitx - _track.tail->point.unitx) > _draw_line_width
    || abs((gint)pos.unity - _track.tail->point.unity) > _draw_line_width)
    {
        if(time && _route.head
                && (newly_fixed ? (route_find_nearest_point(), TRUE)
                                : route_update_nears(TRUE)))
        {
            map_render_paths();
            MACRO_QUEUE_DRAW_AREA();
        }
        if(_show_tracks & TRACKS_MASK)
        {
            /* Instead of calling map_render_paths(), we'll draw the new line
             * ourselves and call gtk_widget_queue_draw_area(). */
            gint tx1, ty1, tx2, ty2;
            map_render_segment(_gc_track, _gc_track_break,
                    _track.tail->point.unitx, _track.tail->point.unity,
                    pos.unitx, pos.unity);
            if(time && _track.tail->point.unity)
            {
                tx1 = unit2x(_track.tail->point.unitx);
                ty1 = unit2y(_track.tail->point.unity);
                tx2 = unit2x(pos.unitx);
                ty2 = unit2y(pos.unity);
                gtk_widget_queue_draw_area(_map_widget,
                        MIN(tx1, tx2) - _draw_line_width,
                        MIN(ty1, ty2) - _draw_line_width,
                        abs(tx1 - tx2) + (2 * _draw_line_width),
                        abs(ty1 - ty2) + (2 * _draw_line_width));
            }
        }
        MACRO_TRACK_INCREMENT_TAIL(_track);

        _track.tail->point = pos;
        _track.tail->time = time;
        _track.tail->altitude = _gps.altitude;

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
    if(time && _next_way_dist_rough
            < (20 + (guint)_gps.speed) * _announce_notice_ratio * 3)
    {
        if(_enable_voice && strcmp(_next_way->desc, _last_spoken_phrase))
        {
            g_free(_last_spoken_phrase);
            _last_spoken_phrase = g_strdup(_next_way->desc);
            if(!fork())
            {
                /* We are the fork child.  Synthesize the voice. */
                hildon_play_system_sound(
                        "/usr/share/sounds/ui-information_note.wav");
                sleep(1);
#               define _voice_synth_path "/usr/bin/flite"
                printf("%s %s\n", _voice_synth_path, _last_spoken_phrase);
                execl(_voice_synth_path, _voice_synth_path,
                        "-t", _last_spoken_phrase, (char *)NULL);
                exit(0);
            }
        }
        MACRO_BANNER_SHOW_INFO(_window, _next_way->desc);
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
    GError *error = NULL;
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

    if(_rfcomm_req_proxy)
    {
        dbus_g_proxy_call(_rfcomm_req_proxy, BTCOND_RFCOMM_CANCEL_CONNECT_REQ,
                    &error,
                    G_TYPE_STRING, _rcvr_mac,
                    G_TYPE_STRING, "SPP",
                    G_TYPE_INVALID,
                    G_TYPE_INVALID);
        error = NULL;
        dbus_g_proxy_call(_rfcomm_req_proxy, BTCOND_RFCOMM_DISCONNECT_REQ,
                    &error,
                    G_TYPE_STRING, _rcvr_mac,
                    G_TYPE_STRING, "SPP",
                    G_TYPE_INVALID,
                    G_TYPE_INVALID);
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

static void rcvr_connect_later(); /* Forward declaration. */

static void
rcvr_connect_fd(gchar *fdpath)
{
    printf("%s(%s)\n", __PRETTY_FUNCTION__, fdpath);

    /* Create the file descriptor. */

    /* If file descriptor creation failed, try again later. */
    if(-1 == (_fd = open(fdpath, O_RDONLY)))
    {
        rcvr_disconnect();
        rcvr_connect_later();
    }
    else
    {
        /* Reset GPS read buffer */
        _gps_read_buf_curr = _gps_read_buf;
        *_gps_read_buf_curr = '\0';

        /* Create channel and add watches. */
        _channel = g_io_channel_unix_new(_fd);
        g_io_channel_set_flags(_channel, G_IO_FLAG_NONBLOCK, NULL);
        _error_sid = g_io_add_watch_full(_channel, G_PRIORITY_HIGH_IDLE,
                G_IO_ERR | G_IO_HUP, channel_cb_error, NULL, NULL);
        _connect_sid = g_io_add_watch_full(_channel, G_PRIORITY_HIGH_IDLE,
                G_IO_OUT, channel_cb_connect, NULL, NULL);
    }
    g_free(fdpath);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

static void
rcvr_connect_response(DBusGProxy *proxy, DBusGProxyCall *call_id)
{
    GError *error = NULL;
    gchar *fdpath = NULL;
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(_conn_state == RCVR_DOWN && _rcvr_mac)
    {
        if(!dbus_g_proxy_end_call(_rfcomm_req_proxy, call_id, &error, 
                    G_TYPE_STRING, &fdpath, G_TYPE_INVALID))
        {
            if(error->domain == DBUS_GERROR
                    && error->code == DBUS_GERROR_REMOTE_EXCEPTION)
            {
                /* If we're already connected, it's not an error, unless
                 * they don't give us the file descriptor path, in which
                 * case we re-connect.*/
                if(!strcmp(BTCOND_ERROR_CONNECTED,
                            dbus_g_error_get_name(error)) || !fdpath)
                {
                    g_printerr("Caught remote method exception %s: %s",
                            dbus_g_error_get_name(error),
                            error->message);
                    rcvr_disconnect();
                    rcvr_connect_later(); /* Try again later. */
                    return;
                }
            }
            else
            {
                /* Unknown error. */
                g_printerr("Error: %s\n", error->message);
                rcvr_disconnect();
                rcvr_connect_later(); /* Try again later. */
                return;
            }
        }
        rcvr_connect_fd(fdpath);
    }
    /* else { Looks like the middle of a disconnect.  Do nothing. } */

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

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

    if(_conn_state == RCVR_DOWN && _rcvr_mac)
    {
#ifndef DEBUG
        if(*_rcvr_mac != '/')
        {
            if(_rfcomm_req_proxy)
            {
                gint mybool = TRUE;
                dbus_g_proxy_begin_call(
                        _rfcomm_req_proxy, BTCOND_RFCOMM_CONNECT_REQ,
                        (DBusGProxyCallNotify)rcvr_connect_response, NULL, NULL,
                        G_TYPE_STRING, _rcvr_mac,
                        G_TYPE_STRING, "SPP",
                        G_TYPE_BOOLEAN, &mybool,
                        G_TYPE_INVALID);
            }
        }
        else
            rcvr_connect_fd(g_strdup(_rcvr_mac));

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

    latlon2unit(_gps.latitude, _gps.longitude, _pos.unitx, _pos.unity);

    tmp = (_gps.heading * (1.f / 180.f)) * PI;
    _vel_offsetx = (gint)(floorf(_gps.speed * sinf(tmp) + 0.5f));
    _vel_offsety = -(gint)(floorf(_gps.speed * cosf(tmp) + 0.5f));

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

/**
 * Update all GdkGC objects to reflect the current _draw_line_width.
 */
#define UPDATE_GC(gc) \
    gdk_gc_set_line_attributes(gc, \
            _draw_line_width, GDK_LINE_SOLID, GDK_CAP_ROUND, GDK_JOIN_ROUND);
static void
update_gcs()
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    gdk_color_alloc(gtk_widget_get_colormap(_map_widget),
            &_color_mark);
    gdk_color_alloc(gtk_widget_get_colormap(_map_widget),
            &_color_mark_velocity);
    gdk_color_alloc(gtk_widget_get_colormap(_map_widget),
            &_color_mark_old);
    gdk_color_alloc(gtk_widget_get_colormap(_map_widget),
            &_color_track);
    gdk_color_alloc(gtk_widget_get_colormap(_map_widget),
            &_color_track_break);
    gdk_color_alloc(gtk_widget_get_colormap(_map_widget),
            &_color_route);
    gdk_color_alloc(gtk_widget_get_colormap(_map_widget),
            &_color_route_way);
    gdk_color_alloc(gtk_widget_get_colormap(_map_widget),
            &_color_route_nextway);
    gdk_color_alloc(gtk_widget_get_colormap(_map_widget),
            &_color_poi);

    if(_gc_mark)
    {
        g_object_unref(_gc_mark);
        g_object_unref(_gc_mark_velocity);
        g_object_unref(_gc_mark_old);
        g_object_unref(_gc_track);
        g_object_unref(_gc_track_break);
        g_object_unref(_gc_route);
        g_object_unref(_gc_route_way);
        g_object_unref(_gc_route_nextway);
        g_object_unref(_gc_poi);
    }

    /* _gc_mark is used to draw the mark when data is current. */
    _gc_mark = gdk_gc_new(_map_pixmap);
    gdk_gc_set_foreground(_gc_mark, &_color_mark);
    gdk_gc_set_line_attributes(_gc_mark,
            _draw_line_width, GDK_LINE_SOLID, GDK_CAP_ROUND, GDK_JOIN_ROUND);

    /* _gc_mark_old is used to draw the mark when data is old. */
    _gc_mark_old = gdk_gc_new(_map_pixmap);
    gdk_gc_set_foreground(_gc_mark_old, &_color_mark_old);
    gdk_gc_set_line_attributes(_gc_mark_old,
            _draw_line_width, GDK_LINE_SOLID, GDK_CAP_ROUND, GDK_JOIN_ROUND);

    /* _gc_mark_velocity is used to draw the vel_current line. */
    _gc_mark_velocity = gdk_gc_new(_map_pixmap);
    gdk_gc_set_foreground(_gc_mark_velocity, &_color_mark_velocity);
    gdk_gc_set_line_attributes(_gc_mark_velocity,
            _draw_line_width, GDK_LINE_SOLID, GDK_CAP_ROUND, GDK_JOIN_ROUND);

    /* _gc_track is used to draw the track line. */
    _gc_track = gdk_gc_new(_map_pixmap);
    gdk_gc_set_foreground(_gc_track, &_color_track);
    gdk_gc_set_line_attributes(_gc_track,
            _draw_line_width, GDK_LINE_SOLID, GDK_CAP_ROUND, GDK_JOIN_ROUND);

    /* _gc_track_break is used to draw the track_break dots. */
    _gc_track_break = gdk_gc_new(_map_pixmap);
    gdk_gc_set_foreground(_gc_track_break, &_color_track_break);
    gdk_gc_set_line_attributes(_gc_track_break,
            _draw_line_width, GDK_LINE_SOLID, GDK_CAP_ROUND, GDK_JOIN_ROUND);

    /* _gc_route is used to draw the route line. */
    _gc_route = gdk_gc_new(_map_pixmap);
    gdk_gc_set_foreground(_gc_route, &_color_route);
    gdk_gc_set_line_attributes(_gc_route,
            _draw_line_width, GDK_LINE_SOLID, GDK_CAP_ROUND, GDK_JOIN_ROUND);

    /* _way_gc is used to draw the waypoint dots. */
    _gc_route_way = gdk_gc_new(_map_pixmap);
    gdk_gc_set_foreground(_gc_route_way, &_color_route_way);
    gdk_gc_set_line_attributes(_gc_route_way,
            _draw_line_width, GDK_LINE_SOLID, GDK_CAP_ROUND, GDK_JOIN_ROUND);

    /* _gc_route_nextway is used to draw the next_way labels. */
    _gc_route_nextway = gdk_gc_new(_map_pixmap);
    gdk_gc_set_foreground(_gc_route_nextway, &_color_route_nextway);
    gdk_gc_set_line_attributes(_gc_route_nextway,
            _draw_line_width, GDK_LINE_SOLID, GDK_CAP_ROUND, GDK_JOIN_ROUND);

    /* _gc_poi is used to draw the default POI icon. */
    _gc_poi = gdk_gc_new(_map_pixmap);
    gdk_gc_set_foreground(_gc_poi, &_color_poi);
    gdk_gc_set_background(_gc_poi, &_color_poi);
    gdk_gc_set_line_attributes(_gc_poi,
            _draw_line_width, GDK_LINE_SOLID, GDK_CAP_ROUND, GDK_JOIN_ROUND);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

/**
 * Save all configuration data to GCONF.
 */
static void
config_save()
{
    GConfClient *gconf_client = gconf_client_get_default();
    gchar buffer[16];
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(!gconf_client)
    {
        popup_error(_window,
                _("Failed to initialize GConf.  Settings were not saved."));
        return;
    }

    /* Save Receiver MAC from GConf. */
    if(_rcvr_mac)
        gconf_client_set_string(gconf_client,
                GCONF_KEY_RCVR_MAC, _rcvr_mac, NULL);
    else
        gconf_client_unset(gconf_client,
                GCONF_KEY_RCVR_MAC, NULL);

    /* Save Map Download URI Format. */
    gconf_client_set_string(gconf_client,
            GCONF_KEY_MAP_URI_FORMAT, _curr_repo->url, NULL);

    /* Save Map Download Zoom Steps. */
    gconf_client_set_int(gconf_client,
            GCONF_KEY_MAP_ZOOM_STEPS, _curr_repo->dl_zoom_steps, NULL);

    /* Save Map Cache Directory. */
    if(_curr_repo->cache_dir)
        gconf_client_set_string(gconf_client,
            GCONF_KEY_MAP_DIR_NAME, _curr_repo->cache_dir, NULL);

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

    /* Save Keep On When Fullscreen flag. */
    gconf_client_set_bool(gconf_client,
            GCONF_KEY_ALWAYS_KEEP_ON, _always_keep_on, NULL);

    /* Save Units. */
    gconf_client_set_string(gconf_client,
            GCONF_KEY_UNITS, UNITS_TEXT[_units], NULL);

    /* Save Escape Key Function. */
    gconf_client_set_string(gconf_client,
            GCONF_KEY_ESCAPE_KEY, ESCAPE_KEY_TEXT[_escape_key], NULL);

    /* Save Deg Format. */
    gconf_client_set_string(gconf_client,
            GCONF_KEY_DEG_FORMAT, DEG_FORMAT_TEXT[_degformat], NULL);

    /* Save Speed Limit On flag. */
    gconf_client_set_bool(gconf_client,
            GCONF_KEY_SPEED_LIMIT_ON, _speed_limit_on, NULL);

    /* Save Speed Limit. */
    gconf_client_set_int(gconf_client,
            GCONF_KEY_SPEED_LIMIT, _speed_limit, NULL);

    /* Save Speed Location. */
    gconf_client_set_string(gconf_client,
            GCONF_KEY_SPEED_LOCATION,
            SPEED_LOCATION_TEXT[_speed_location], NULL);

    /* Save Info Font Size. */
    gconf_client_set_string(gconf_client,
            GCONF_KEY_INFO_FONT_SIZE,
            INFO_FONT_TEXT[_info_font_size], NULL);

    /* Save last saved latitude. */
    gconf_client_set_float(gconf_client,
            GCONF_KEY_LAT, _gps.latitude, NULL);

    /* Save last saved longitude. */
    gconf_client_set_float(gconf_client,
            GCONF_KEY_LON, _gps.longitude, NULL);

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

    /* Save the repositories. */
    {
        GList *curr = _repo_list;
        GSList *temp_list = NULL;
        gint curr_repo_index = 0;

        for(curr = _repo_list; curr != NULL; curr = curr->next)
        {
            /* Build from each part of a repo, delimited by newline characters:
             * 1. url
             * 2. cache_dir
             * 3. dl_zoom_steps
             * 4. view_zoom_steps
             */
            RepoData *rd = curr->data;
            gchar buffer[BUFFER_SIZE];
            snprintf(buffer, sizeof(buffer),
                    "%s\n%s\n%s\n%d\n%d\n",
                    rd->name,
                    rd->url,
                    rd->cache_dir,
                    rd->dl_zoom_steps,
                    rd->view_zoom_steps);
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

    /* Save GPS Info flag. */
    gconf_client_set_bool(gconf_client,
            GCONF_KEY_GPS_INFO, _gps_info, NULL);

    /* Save Route Download Radius. */
    gconf_client_set_int(gconf_client,
            GCONF_KEY_ROUTE_DL_RADIUS, _route_dl_radius, NULL);

    /* Save Colors. */
    snprintf(buffer, sizeof(buffer), "#%02x%02x%02x",
            _color_mark.red >> 8,
            _color_mark.green >> 8,
            _color_mark.blue >> 8);
    gconf_client_set_string(gconf_client,
            GCONF_KEY_COLOR_MARK, buffer, NULL);

    snprintf(buffer, sizeof(buffer), "#%02x%02x%02x",
            _color_mark_velocity.red >> 8,
            _color_mark_velocity.green >> 8,
            _color_mark_velocity.blue >> 8);
    gconf_client_set_string(gconf_client,
            GCONF_KEY_COLOR_MARK_VELOCITY, buffer, NULL);

    snprintf(buffer, sizeof(buffer), "#%02x%02x%02x",
            _color_mark_old.red >> 8,
            _color_mark_old.green >> 8,
            _color_mark_old.blue >> 8);
    gconf_client_set_string(gconf_client,
            GCONF_KEY_COLOR_MARK_OLD, buffer, NULL);

    snprintf(buffer, sizeof(buffer), "#%02x%02x%02x",
            _color_track.red >> 8,
            _color_track.green >> 8,
            _color_track.blue >> 8);
    gconf_client_set_string(gconf_client,
            GCONF_KEY_COLOR_TRACK, buffer, NULL);

    snprintf(buffer, sizeof(buffer), "#%02x%02x%02x",
            _color_track_break.red >> 8,
            _color_track_break.green >> 8,
            _color_track_break.blue >> 8);
    gconf_client_set_string(gconf_client,
            GCONF_KEY_COLOR_TRACK_BREAK, buffer, NULL);

    snprintf(buffer, sizeof(buffer), "#%02x%02x%02x",
            _color_route.red >> 8,
            _color_route.green >> 8,
            _color_route.blue >> 8);
    gconf_client_set_string(gconf_client,
            GCONF_KEY_COLOR_ROUTE, buffer, NULL);

    snprintf(buffer, sizeof(buffer), "#%02x%02x%02x",
            _color_route_way.red >> 8,
            _color_route_way.green >> 8,
            _color_route_way.blue >> 8);
    gconf_client_set_string(gconf_client,
            GCONF_KEY_COLOR_ROUTE_WAY, buffer, NULL);

    snprintf(buffer, sizeof(buffer), "#%02x%02x%02x",
            _color_route_nextway.red >> 8,
            _color_route_nextway.green >> 8,
            _color_route_nextway.blue >> 8);
    gconf_client_set_string(gconf_client,
            GCONF_KEY_COLOR_ROUTE_NEXTWAY, buffer, NULL);

    snprintf(buffer, sizeof(buffer), "#%02x%02x%02x",
            _color_poi.red >> 8,
            _color_poi.green >> 8,
            _color_poi.blue >> 8);
    gconf_client_set_string(gconf_client,
            GCONF_KEY_COLOR_POI, buffer, NULL);

    /* Save POI database. */
    if(_poi_db)
        gconf_client_set_string(gconf_client,
                GCONF_KEY_POI_DB, _poi_db, NULL);
    else
        gconf_client_unset(gconf_client, GCONF_KEY_POI_DB, NULL);

    /* Save Show POI below zoom. */
    gconf_client_set_int(gconf_client,
            GCONF_KEY_POI_ZOOM, _poi_zoom, NULL);

    gconf_client_clear_cache(gconf_client);
    g_object_unref(gconf_client);

    /* Save route. */
    {
        GnomeVFSHandle *handle;
        gchar *route_file;
        route_file = gnome_vfs_uri_make_full_from_relative(
                _config_dir_uri, CONFIG_FILE_ROUTE);
        if(GNOME_VFS_OK == gnome_vfs_create(&handle, route_file,
                    GNOME_VFS_OPEN_WRITE, FALSE, 0600))
        {
            write_route_gpx(handle);
            gnome_vfs_close(handle);
        }
        g_free(route_file);
    }

    /* Save track. */
    {
        GnomeVFSHandle *handle;
        gchar *track_file;
        track_file = gnome_vfs_uri_make_full_from_relative(
                _config_dir_uri, CONFIG_FILE_TRACK);
        if(GNOME_VFS_OK == gnome_vfs_create(&handle, track_file,
                    GNOME_VFS_OPEN_WRITE, FALSE, 0600))
        {
            write_track_gpx(handle);
            gnome_vfs_close(handle);
        }
        g_free(track_file);
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

static void
force_min_visible_bars(HildonControlbar *control_bar, gint num_bars)
{
    GValue val;
    printf("%s()\n", __PRETTY_FUNCTION__);
    memset(&val, 0, sizeof(val));
    g_value_init(&val, G_TYPE_INT);
    g_value_set_int(&val, num_bars);
    g_object_set_property(G_OBJECT(control_bar), "minimum-visible-bars", &val);
    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}


typedef struct _ScanInfo ScanInfo;
struct _ScanInfo {
    GtkWidget *settings_dialog;
    GtkWidget *txt_rcvr_mac;
    GtkWidget *scan_dialog;
    GtkWidget *banner;
    GtkListStore *store;
    guint sid;
    DBusGProxy *req_proxy;
    DBusGProxy *sig_proxy;
};


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
    DBusGConnection *dbus_conn;
    printf("%s()\n", __PRETTY_FUNCTION__);

    /* Initialize D-Bus. */
    if(NULL == (dbus_conn = dbus_g_bus_get(DBUS_BUS_SYSTEM, &error)))
    {
        g_printerr("Failed to open connection to D-Bus: %s.\n",
                error->message);
        return 1;
    }

    if(NULL == (scan_info->req_proxy = dbus_g_proxy_new_for_name(dbus_conn,
            BTSEARCH_SERVICE,
            BTSEARCH_REQ_PATH,
            BTSEARCH_REQ_INTERFACE)))
    {
        g_printerr("Failed to create D-Bus request proxy for btsearch.");
        return 2;
    }

    if(NULL == (scan_info->sig_proxy = dbus_g_proxy_new_for_name(dbus_conn,
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
    GtkWidget *dialog;
    GtkWidget *lst_devices;
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;
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
            _("MAC"), renderer, "text", 0);
    gtk_tree_view_append_column(GTK_TREE_VIEW(lst_devices), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(
            _("Description"), renderer, "text", 1);
    gtk_tree_view_append_column(GTK_TREE_VIEW(lst_devices), column);

    gtk_widget_show_all(dialog);

    scan_info->banner = hildon_banner_show_animation(dialog, NULL,
            _("Scanning Bluetooth Devices"));

    if(scan_start_search(scan_info))
    {
        gtk_widget_destroy(scan_info->banner);
        popup_error(scan_info->settings_dialog,
                "An error occurred while attempting to scan for "
                "bluetooth devices.");
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
            gtk_entry_set_text(GTK_ENTRY(scan_info->txt_rcvr_mac), mac);
            break;
        }
        else
            popup_error(dialog,
                    _("Please select a bluetooth device from the list."));
    }

    gtk_widget_destroy(dialog);

    /* Clean up D-Bus. */
    dbus_g_proxy_call(scan_info->req_proxy, BTSEARCH_STOP_SEARCH_REQ,
                &error, G_TYPE_INVALID, G_TYPE_INVALID);
    g_object_unref(scan_info->req_proxy);
    g_object_unref(scan_info->sig_proxy);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

typedef struct _BrowseInfo BrowseInfo;
struct _BrowseInfo {
    GtkWidget *dialog;
    GtkWidget *txt;
};

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


typedef struct _ColorsDialogInfo ColorsDialogInfo;
struct _ColorsDialogInfo {
    GtkWidget *col_mark;
    GtkWidget *col_mark_velocity;
    GtkWidget *col_mark_old;
    GtkWidget *col_track;
    GtkWidget *col_track_break;
    GtkWidget *col_route;
    GtkWidget *col_route_way;
    GtkWidget *col_route_nextway;
    GtkWidget *col_poi;
};

static gboolean
settings_dialog_colors_reset(GtkWidget *widget, ColorsDialogInfo *cdi)
{
    printf("%s()\n", __PRETTY_FUNCTION__);
    hildon_color_button_set_color(
            HILDON_COLOR_BUTTON(cdi->col_mark),
            &DEFAULT_COLOR_MARK);
    hildon_color_button_set_color(
            HILDON_COLOR_BUTTON(cdi->col_mark_velocity),
            &DEFAULT_COLOR_MARK_VELOCITY);
    hildon_color_button_set_color(
            HILDON_COLOR_BUTTON(cdi->col_mark_old),
            &DEFAULT_COLOR_MARK_OLD);
    hildon_color_button_set_color(
            HILDON_COLOR_BUTTON(cdi->col_track),
            &DEFAULT_COLOR_TRACK);
    hildon_color_button_set_color(
            HILDON_COLOR_BUTTON(cdi->col_track_break),
            &DEFAULT_COLOR_TRACK_BREAK);
    hildon_color_button_set_color(
            HILDON_COLOR_BUTTON(cdi->col_route),
            &DEFAULT_COLOR_ROUTE);
    hildon_color_button_set_color(
            HILDON_COLOR_BUTTON(cdi->col_route_way),
            &DEFAULT_COLOR_ROUTE_WAY);
    hildon_color_button_set_color(
            HILDON_COLOR_BUTTON(cdi->col_route_nextway),
            &DEFAULT_COLOR_ROUTE_NEXTWAY);
    hildon_color_button_set_color(
            HILDON_COLOR_BUTTON(cdi->col_poi),
            &DEFAULT_COLOR_POI);
    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
settings_dialog_colors(GtkWidget *widget, GtkWidget *parent)
{
    GtkWidget *dialog;
    GtkWidget *table;
    GtkWidget *label;
    GtkWidget *btn_defaults;
    ColorsDialogInfo cdi;
    printf("%s()\n", __PRETTY_FUNCTION__);

    dialog = gtk_dialog_new_with_buttons(_("Colors"),
            GTK_WINDOW(parent), GTK_DIALOG_MODAL,
            GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
            NULL);

    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area),
            btn_defaults = gtk_button_new_with_label(_("Defaults")));
    g_signal_connect(G_OBJECT(btn_defaults), "clicked",
                      G_CALLBACK(settings_dialog_colors_reset), &cdi);

    gtk_dialog_add_button(GTK_DIALOG(dialog),
            GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
            table = gtk_table_new(4, 3, FALSE), TRUE, TRUE, 0);

    /* Mark. */
    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new(_("GPS Mark")),
            0, 1, 0, 1, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(table),
            cdi.col_mark = hildon_color_button_new(),
            1, 2, 0, 1, 0, 0, 2, 4);
    hildon_color_button_set_color(
            HILDON_COLOR_BUTTON(cdi.col_mark), &_color_mark);
    gtk_table_attach(GTK_TABLE(table),
            cdi.col_mark_velocity = hildon_color_button_new(),
            2, 3, 0, 1, 0, 0, 2, 4);
    hildon_color_button_set_color(
            HILDON_COLOR_BUTTON(cdi.col_mark_velocity), &_color_mark_velocity);
    gtk_table_attach(GTK_TABLE(table),
            cdi.col_mark_old = hildon_color_button_new(),
            3, 4, 0, 1, 0, 0, 2, 4);
    hildon_color_button_set_color(
            HILDON_COLOR_BUTTON(cdi.col_mark_old), &_color_mark_old);

    /* Track. */
    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new(_("Track")),
            0, 1, 1, 2, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(table),
            cdi.col_track = hildon_color_button_new(),
            1, 2, 1, 2, 0, 0, 2, 4);
    hildon_color_button_set_color(
            HILDON_COLOR_BUTTON(cdi.col_track), &_color_track);
    gtk_table_attach(GTK_TABLE(table),
            cdi.col_track_break = hildon_color_button_new(),
            2, 3, 1, 2, 0, 0, 2, 4);
    hildon_color_button_set_color(
            HILDON_COLOR_BUTTON(cdi.col_track_break), &_color_track_break);

    /* Route. */
    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new(_("Route")),
            0, 1, 2, 3, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(table),
            cdi.col_route = hildon_color_button_new(),
            1, 2, 2, 3, 0, 0, 2, 4);
    hildon_color_button_set_color(
            HILDON_COLOR_BUTTON(cdi.col_route), &_color_route);
    gtk_table_attach(GTK_TABLE(table),
            cdi.col_route_way = hildon_color_button_new(),
            2, 3, 2, 3, 0, 0, 2, 4);
    hildon_color_button_set_color(
            HILDON_COLOR_BUTTON(cdi.col_route_way), &_color_route_way);
    gtk_table_attach(GTK_TABLE(table),
            cdi.col_route_nextway = hildon_color_button_new(),
            3, 4, 2, 3, 0, 0, 2, 4);
    hildon_color_button_set_color(
            HILDON_COLOR_BUTTON(cdi.col_route_nextway), &_color_route_nextway);

    /* POI. */
    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new(_("POI")),
            0, 1, 3, 4, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(table),
            cdi.col_poi = hildon_color_button_new(),
            1, 2, 3, 4, 0, 0, 2, 4);
    hildon_color_button_set_color(
            HILDON_COLOR_BUTTON(cdi.col_poi), &_color_poi);

    gtk_widget_show_all(dialog);

    while(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(dialog)))
    {
        GdkColor *color;

        color = hildon_color_button_get_color(
                HILDON_COLOR_BUTTON(cdi.col_mark));
        _color_mark = *color;

        color = hildon_color_button_get_color(
                HILDON_COLOR_BUTTON(cdi.col_mark_velocity));
        _color_mark_velocity = *color;

        color = hildon_color_button_get_color(
                HILDON_COLOR_BUTTON(cdi.col_mark_old));
        _color_mark_old = *color;

        color = hildon_color_button_get_color(
                HILDON_COLOR_BUTTON(cdi.col_track));
        _color_track = *color;

        color = hildon_color_button_get_color(
                HILDON_COLOR_BUTTON(cdi.col_track_break));
        _color_track_break = *color;

        color = hildon_color_button_get_color(
                HILDON_COLOR_BUTTON(cdi.col_route));
        _color_route = *color;

        color = hildon_color_button_get_color(
                HILDON_COLOR_BUTTON(cdi.col_route_way));
        _color_route_way = *color;

        color = hildon_color_button_get_color(
                HILDON_COLOR_BUTTON(cdi.col_route_nextway));
        _color_route_nextway = *color;

        color = hildon_color_button_get_color(
                HILDON_COLOR_BUTTON(cdi.col_poi));
        _color_poi = *color;

        update_gcs();
        break;
    }

    gtk_widget_destroy(dialog);

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
    GtkWidget *hbox;
    GtkWidget *hbox2;
    GtkWidget *label;
    GtkWidget *txt_rcvr_mac;
    GtkWidget *num_center_ratio;
    GtkWidget *num_lead_ratio;
    GtkWidget *num_announce_notice;
    GtkWidget *chk_enable_voice;
    GtkWidget *num_voice_speed;
    GtkWidget *num_voice_pitch;
    GtkWidget *num_draw_line_width;
    GtkWidget *chk_always_keep_on;
    GtkWidget *cmb_units;
    GtkWidget *cmb_escape_key;
    GtkWidget *cmb_degformat;
    GtkWidget *btn_scan;
    GtkWidget *btn_colors;

    GtkWidget *txt_poi_db;
    GtkWidget *btn_browsepoi;
    GtkWidget *num_poi_zoom;
    GtkWidget *chk_speed_limit_on;
    GtkWidget *num_speed;
    GtkWidget *cmb_speed_location;
    GtkWidget *cmb_info_font_size;

    BrowseInfo browse_info = {0, 0};
    ScanInfo scan_info = {0};
    gboolean rcvr_changed = FALSE;
    guint i;
    printf("%s()\n", __PRETTY_FUNCTION__);

    dialog = gtk_dialog_new_with_buttons(_("Maemo Mapper Settings"),
            GTK_WINDOW(_window), GTK_DIALOG_MODAL,
            GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
            NULL);

    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area),
            btn_colors = gtk_button_new_with_label(_("Colors...")));

    gtk_dialog_add_button(GTK_DIALOG(dialog),
            GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
            notebook = gtk_notebook_new(), TRUE, TRUE, 0);

    /* Receiver page. */
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
            table = gtk_table_new(2, 3, FALSE),
            label = gtk_label_new(_("GPS")));

    /* Receiver MAC Address. */
    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new(_("MAC")),
            0, 1, 0, 1, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(table),
            hbox = gtk_hbox_new(FALSE, 4),
            1, 2, 0, 1, GTK_EXPAND | GTK_FILL, 0, 2, 4);
    gtk_box_pack_start(GTK_BOX(hbox),
            txt_rcvr_mac = gtk_entry_new(),
            TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox),
            btn_scan = gtk_button_new_with_label(_("Scan...")),
            FALSE, FALSE, 0);

    /* Note!. */
    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new(
                _("Note: For rfcomm, enter the device path "
                    "(e.g. \"/dev/rfcomm0\").")),
            0, 2, 1, 2, GTK_FILL, 0, 2, 4);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
    gtk_misc_set_alignment(GTK_MISC(label), 0.5f, 0.5f);


    /* Auto-Center page. */
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
            table = gtk_table_new(2, 2, FALSE),
            label = gtk_label_new(_("Auto-Center")));

    /* Auto-Center Sensitivity. */
    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new(_("Sensitivity")),
            0, 1, 0, 1, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(table),
            label = gtk_alignment_new(0.f, 0.5f, 0.f, 0.f),
            1, 2, 0, 1, GTK_EXPAND | GTK_FILL, 0, 2, 4);
    gtk_container_add(GTK_CONTAINER(label),
            num_center_ratio = hildon_controlbar_new());
    hildon_controlbar_set_range(HILDON_CONTROLBAR(num_center_ratio), 1, 10);
    force_min_visible_bars(HILDON_CONTROLBAR(num_center_ratio), 1);

    /* Lead Amount. */
    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new(_("Lead Amount")),
            0, 1, 1, 2, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(table),
            label = gtk_alignment_new(0.f, 0.5f, 0.f, 0.f),
            1, 2, 1, 2, GTK_EXPAND | GTK_FILL, 0, 2, 4);
    gtk_container_add(GTK_CONTAINER(label),
            num_lead_ratio = hildon_controlbar_new());
    hildon_controlbar_set_range(HILDON_CONTROLBAR(num_lead_ratio), 1, 10);
    force_min_visible_bars(HILDON_CONTROLBAR(num_lead_ratio), 1);

    /* Announcement. */
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
            table = gtk_table_new(2, 3, FALSE),
            label = gtk_label_new(_("Announce")));

    /* Announcement Advance Notice. */
    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new(_("Advance Notice")),
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
                _("Enable Voice Synthesis (requires flite)")),
            0, 2, 1, 2, GTK_EXPAND | GTK_FILL, 0, 2, 4);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chk_enable_voice),
            _enable_voice);

    /* Voice Speed and Pitch. */
    gtk_table_attach(GTK_TABLE(table),
            hbox = gtk_hbox_new(FALSE, 12),
            0, 2, 2, 3, 0, 0, 2, 6);
    gtk_box_pack_start(GTK_BOX(hbox),
            hbox2 = gtk_hbox_new(FALSE, 4),
            TRUE, TRUE, 4);
    gtk_box_pack_start(GTK_BOX(hbox2),
            label = gtk_label_new(_("Speed")),
            TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox2),
            num_voice_speed = hildon_controlbar_new(),
            TRUE, TRUE, 0);
    hildon_controlbar_set_range(HILDON_CONTROLBAR(num_voice_speed), 1, 10);
    force_min_visible_bars(HILDON_CONTROLBAR(num_voice_speed), 1);

    gtk_box_pack_start(GTK_BOX(hbox),
            hbox2 = gtk_hbox_new(FALSE, 4),
            TRUE, TRUE, 4);
    gtk_box_pack_start(GTK_BOX(hbox2),
            label = gtk_label_new(_("Pitch")),
            TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox2),
            num_voice_pitch = hildon_controlbar_new(),
            TRUE, TRUE, 0);
    hildon_controlbar_set_range(HILDON_CONTROLBAR(num_voice_pitch), -2, 8);
    force_min_visible_bars(HILDON_CONTROLBAR(num_voice_pitch), 1);

    /* Misc. page. */
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
            table = gtk_table_new(2, 3, FALSE),
            label = gtk_label_new(_("Misc.")));

    /* Line Width. */
    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new(_("Line Width")),
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
                _("Keep Display On Only in Fullscreen Mode")),
            0, 2, 1, 2, GTK_EXPAND | GTK_FILL, 0, 2, 4);

    gtk_table_attach(GTK_TABLE(table),
            hbox = gtk_hbox_new(FALSE, 4),
            0, 2, 2, 3, GTK_FILL, 0, 2, 4);

    /* Escape Key. */
    gtk_box_pack_start(GTK_BOX(hbox),
            label = gtk_label_new(_("Escape Key")),
            FALSE, FALSE, 0);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_box_pack_start(GTK_BOX(hbox),
            label = gtk_alignment_new(0.f, 0.5f, 0.f, 0.f),
            TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(label),
            cmb_escape_key = gtk_combo_box_new_text());
    for(i = 0; i < ESCAPE_KEY_ENUM_COUNT; i++)
        gtk_combo_box_append_text(GTK_COMBO_BOX(cmb_escape_key),
                ESCAPE_KEY_TEXT[i]);

    /* Misc. 2 page. */
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
            table = gtk_table_new(2, 3, FALSE),
            label = gtk_label_new(_("Misc. 2")));

    /* Units. */
    gtk_table_attach(GTK_TABLE(table),
            hbox = gtk_hbox_new(FALSE, 4),
            0, 1, 0, 1, GTK_FILL, 0, 2, 4);
    gtk_box_pack_start(GTK_BOX(hbox),
            label = gtk_label_new(_("Units")),
            FALSE, FALSE, 0);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_box_pack_start(GTK_BOX(hbox),
            label = gtk_alignment_new(0.f, 0.5f, 0.f, 0.f),
            TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(label),
            cmb_units = gtk_combo_box_new_text());
    for(i = 0; i < UNITS_ENUM_COUNT; i++)
        gtk_combo_box_append_text(GTK_COMBO_BOX(cmb_units), UNITS_TEXT[i]);

    /* Degrees format */
    gtk_table_attach(GTK_TABLE(table),
            hbox = gtk_hbox_new(FALSE, 4),
            1, 2, 0, 1, GTK_FILL, 0, 2, 4);
    gtk_box_pack_start(GTK_BOX(hbox),
            label = gtk_label_new(_("Degrees Format")),
            FALSE, FALSE, 0);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_box_pack_start(GTK_BOX(hbox),
            label = gtk_alignment_new(0.f, 0.5f, 0.f, 0.f),
            TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(label),
            cmb_degformat = gtk_combo_box_new_text());
    for(i = 0; i < DEG_FORMAT_ENUM_COUNT; i++)
        gtk_combo_box_append_text(GTK_COMBO_BOX(cmb_degformat),
            DEG_FORMAT_TEXT[i]);

    /* Speed warner. */
    gtk_table_attach(GTK_TABLE(table),
            hbox = gtk_hbox_new(FALSE, 4),
            0, 2, 1, 2, GTK_FILL, 0, 2, 4);

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
                SPEED_LOCATION_TEXT[i]);

    /* Information Font Size. */
    gtk_table_attach(GTK_TABLE(table),
            hbox = gtk_hbox_new(FALSE, 4),
            0, 2, 2, 3, GTK_FILL, 0, 2, 4);
    gtk_box_pack_start(GTK_BOX(hbox),
            label = gtk_label_new(_("Information Font Size")),
            FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox),
            cmb_info_font_size = gtk_combo_box_new_text(),
            FALSE, FALSE, 0);
    for(i = 0; i < INFO_FONT_ENUM_COUNT; i++)
        gtk_combo_box_append_text(GTK_COMBO_BOX(cmb_info_font_size),
                INFO_FONT_TEXT[i]);


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
    gtk_box_pack_start(GTK_BOX(hbox),
            btn_browsepoi = gtk_button_new_with_label(_("Browse...")),
            FALSE, FALSE, 0);

    /* Show POI below zoom. */
    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new(_("Show POI below zoom")),
            0, 1, 2, 3, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(table),
            label = gtk_alignment_new(0.f, 0.5f, 0.f, 0.f),
            1, 2, 2, 3, GTK_EXPAND | GTK_FILL, 0, 2, 4);
    gtk_container_add(GTK_CONTAINER(label),
            num_poi_zoom = hildon_number_editor_new(0, MAX_ZOOM));


    /* Connect signals. */
    scan_info.settings_dialog = dialog;
    scan_info.txt_rcvr_mac = txt_rcvr_mac;
    g_signal_connect(G_OBJECT(btn_scan), "clicked",
                     G_CALLBACK(scan_bluetooth), &scan_info);
    g_signal_connect(G_OBJECT(btn_colors), "clicked",
                     G_CALLBACK(settings_dialog_colors), dialog);

    browse_info.dialog = dialog;
    browse_info.txt = txt_poi_db;
    g_signal_connect(G_OBJECT(btn_browsepoi), "clicked",
                     G_CALLBACK(settings_dialog_browse_forfile), &browse_info);

    /* Initialize fields. */
    if(_rcvr_mac)
        gtk_entry_set_text(GTK_ENTRY(txt_rcvr_mac), _rcvr_mac);
    if(_poi_db)
        gtk_entry_set_text(GTK_ENTRY(txt_poi_db), _poi_db);
    hildon_number_editor_set_value(HILDON_NUMBER_EDITOR(num_poi_zoom),
            _poi_zoom);
    hildon_controlbar_set_value(HILDON_CONTROLBAR(num_center_ratio),
            _center_ratio);
    hildon_controlbar_set_value(HILDON_CONTROLBAR(num_lead_ratio),
            _lead_ratio);
    hildon_controlbar_set_value(HILDON_CONTROLBAR(num_announce_notice),
            _announce_notice_ratio);
    hildon_controlbar_set_value(HILDON_CONTROLBAR(num_voice_speed),
            (gint)(_voice_speed * 3 + 0.5));
    hildon_controlbar_set_value(HILDON_CONTROLBAR(num_voice_pitch),
            _voice_pitch);
    hildon_controlbar_set_value(HILDON_CONTROLBAR(num_draw_line_width),
            _draw_line_width);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chk_always_keep_on),
            !_always_keep_on);
    gtk_combo_box_set_active(GTK_COMBO_BOX(cmb_units), _units);
    gtk_combo_box_set_active(GTK_COMBO_BOX(cmb_escape_key), _escape_key);
    gtk_combo_box_set_active(GTK_COMBO_BOX(cmb_degformat), _degformat);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chk_speed_limit_on),
            _speed_limit_on);
    hildon_number_editor_set_range(HILDON_NUMBER_EDITOR(num_speed),
            1, 300);
    hildon_number_editor_set_value(HILDON_NUMBER_EDITOR(num_speed),
            _speed_limit);
    gtk_combo_box_set_active(GTK_COMBO_BOX(cmb_speed_location),
            _speed_location);
    gtk_combo_box_set_active(GTK_COMBO_BOX(cmb_info_font_size),
            _info_font_size);

    gtk_widget_show_all(dialog);

    while(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(dialog)))
    {
        /* Set _rcvr_mac if necessary. */
        if(!*gtk_entry_get_text(GTK_ENTRY(txt_rcvr_mac)))
        {
            /* User specified no rcvr mac - set _rcvr_mac to NULL. */
            if(_rcvr_mac)
            {
                g_free(_rcvr_mac);
                _rcvr_mac = NULL;
                rcvr_changed = TRUE;
                gtk_widget_set_sensitive(
                        GTK_WIDGET(_menu_gps_details_item), FALSE);
            }
            if(_enable_gps)
            {
                gtk_check_menu_item_set_active(
                        GTK_CHECK_MENU_ITEM(_menu_enable_gps_item), FALSE);
                popup_error(dialog, _("No GPS Receiver MAC Provided.\n"
                        "GPS Disabled."));
                rcvr_changed = TRUE;
                gtk_widget_set_sensitive(GTK_WIDGET(_menu_gps_details_item),
                        FALSE);
                gtk_widget_set_sensitive(GTK_WIDGET(_menu_gps_reset_item),
                        FALSE);
            }
        }
        else if(!_rcvr_mac || strcmp(_rcvr_mac,
                      gtk_entry_get_text(GTK_ENTRY(txt_rcvr_mac))))
        {
            /* User specified a new rcvr mac. */
            g_free(_rcvr_mac);
            _rcvr_mac = g_strdup(gtk_entry_get_text(GTK_ENTRY(txt_rcvr_mac)));
            rcvr_changed = TRUE;
        }

        _center_ratio = hildon_controlbar_get_value(
                HILDON_CONTROLBAR(num_center_ratio));

        _lead_ratio = hildon_controlbar_get_value(
                HILDON_CONTROLBAR(num_lead_ratio));

        _draw_line_width = hildon_controlbar_get_value(
                HILDON_CONTROLBAR(num_draw_line_width));

        _always_keep_on = !gtk_toggle_button_get_active(
                GTK_TOGGLE_BUTTON(chk_always_keep_on));

        _units = gtk_combo_box_get_active(GTK_COMBO_BOX(cmb_units));
        _escape_key = gtk_combo_box_get_active(GTK_COMBO_BOX(cmb_escape_key));
        _degformat = gtk_combo_box_get_active(GTK_COMBO_BOX(cmb_degformat));

        _speed_limit_on = gtk_toggle_button_get_active(
                GTK_TOGGLE_BUTTON(chk_speed_limit_on));
        _speed_limit = hildon_number_editor_get_value(
                HILDON_NUMBER_EDITOR(num_speed));
        _speed_location = gtk_combo_box_get_active(
                GTK_COMBO_BOX(cmb_speed_location));

        _info_font_size = gtk_combo_box_get_active(
                GTK_COMBO_BOX(cmb_info_font_size));

        _announce_notice_ratio = hildon_controlbar_get_value(
                HILDON_CONTROLBAR(num_announce_notice));

        _voice_speed = hildon_controlbar_get_value(
                HILDON_CONTROLBAR(num_voice_speed)) / 3.0;

        _voice_pitch = hildon_controlbar_get_value(
                HILDON_CONTROLBAR(num_voice_pitch));

        _enable_voice = gtk_toggle_button_get_active(
                GTK_TOGGLE_BUTTON(chk_enable_voice));

        if(_dbconn)
        {
            sqlite_close(_db);
            _dbconn = FALSE;
            gtk_widget_set_sensitive(_cmenu_add_poi, FALSE);
            gtk_widget_set_sensitive(_cmenu_edit_poi, FALSE);
            gtk_widget_set_sensitive(_menu_poi_item, FALSE);
        }
        g_free(_poi_db);
        if(strlen(gtk_entry_get_text(GTK_ENTRY(txt_poi_db))))
        {
            _poi_db = g_strdup(gtk_entry_get_text(GTK_ENTRY(txt_poi_db)));
            db_connect();
            gtk_widget_set_sensitive(_cmenu_add_poi, _dbconn);
            gtk_widget_set_sensitive(_cmenu_edit_poi, _dbconn);
            gtk_widget_set_sensitive(_menu_poi_item, _dbconn);
        }
        else
            _poi_db = NULL;

        _poi_zoom = hildon_number_editor_get_value(
        HILDON_NUMBER_EDITOR(num_poi_zoom));

        update_gcs();

        config_save();
        break;
    }

    gtk_widget_hide(dialog); /* Destroying causes a crash (!?!?!??!) */

    vprintf("%s(): return %d\n", __PRETTY_FUNCTION__, rcvr_changed);
    return rcvr_changed;
}

static gint
download_comparefunc(const ProgressUpdateInfo *a,
        const ProgressUpdateInfo *b, gpointer user_data)
{
    gint diff = (a->priority - b->priority);
    if(diff)
        return diff;
    diff = (a->tilex - b->tilex);
    if(diff)
        return diff;
    diff = (a->tiley - b->tiley);
    if(diff)
        return diff;
    diff = (a->zoom - b->zoom);
    if(diff)
        return diff;
    diff = (a->repo - b->repo);
    if(diff)
        return diff;
    /* Otherwise, deletes are "greatest" (least priority). */
    if(!a->retries)
        return (b->retries ? -1 : 0);
    else if(!b->retries)
        return (a->retries ? 1 : 0);
    /* Do updates after non-updates (because they'll both be done anyway). */
    return (a->retries - b->retries);
}

/**
 * Free a ProgressUpdateInfo data structure that was allocated during the
 * auto-map-download process.
 */
static void
progress_update_info_free(ProgressUpdateInfo *pui)
{
    vprintf("%s()\n", __PRETTY_FUNCTION__);

    g_free(pui->src_str);
    g_free(pui->dest_str);

    g_slice_free(ProgressUpdateInfo, pui);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

static void
config_update_proxy()
{
    GConfClient *gconf_client = gconf_client_get_default();
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(_iap_http_proxy_host)
        g_free(_iap_http_proxy_host);

    /* Get proxy data and register for updates. */
    if(gconf_client_get_bool(gconf_client,
                GCONF_KEY_HTTP_PROXY_ON, NULL))
    {
        /* HTTP Proxy is on. */
        _iap_http_proxy_host = gconf_client_get_string(gconf_client,
                GCONF_KEY_HTTP_PROXY_HOST, NULL);
        _iap_http_proxy_port = gconf_client_get_int(gconf_client,
                GCONF_KEY_HTTP_PROXY_PORT, NULL);
    }
    else
    {
        /* HTTP Proxy is off. */
        _iap_http_proxy_host = NULL;
        _iap_http_proxy_port = 0;
    }
    g_object_unref(gconf_client);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
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
    gchar *str;
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(!gconf_client)
    {
        popup_error(_window, _("Failed to initialize GConf.  Quitting."));
        exit(1);
    }

    /* Initialize _config_dir. */
    {
        gchar *config_dir;
        config_dir = gnome_vfs_expand_initial_tilde(CONFIG_DIR_NAME);
        _config_dir_uri = gnome_vfs_make_uri_from_input(config_dir);
        gnome_vfs_make_directory(_config_dir_uri, 0700);
        g_free(config_dir);
    }

    /* Retrieve route. */
    {
        gchar *route_file;
        gchar *bytes;
        gint size;

        route_file = gnome_vfs_uri_make_full_from_relative(
                _config_dir_uri, CONFIG_FILE_ROUTE);
        if(GNOME_VFS_OK == gnome_vfs_read_entire_file(
                    route_file, &size, &bytes))
            parse_route_gpx(bytes, size, 0); /* 0 to replace route. */
        g_free(route_file);
    }

    /* Retrieve track. */
    {
        gchar *track_file;
        gchar *bytes;
        gint size;

        track_file = gnome_vfs_uri_make_full_from_relative(
                _config_dir_uri, CONFIG_FILE_TRACK);
        if(GNOME_VFS_OK == gnome_vfs_read_entire_file(
                    track_file, &size, &bytes))
            parse_track_gpx(bytes, size, 0); /* 0 to replace track. */
        g_free(track_file);
    }

    /* Get Receiver MAC from GConf.  Default is scanned via hci_inquiry. */
    {
        _rcvr_mac = gconf_client_get_string(
                gconf_client, GCONF_KEY_RCVR_MAC, NULL);
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

    /* Get Voice Speed - Default is 1.0. */
    value = gconf_client_get(gconf_client, GCONF_KEY_VOICE_SPEED, NULL);
    if(value)
    {
        _voice_speed = gconf_value_get_float(value);
        gconf_value_free(value);
    }
    else
        _voice_speed = 1.0;

    /* Get Voice Speed - Default is 0. */
    value = gconf_client_get(gconf_client, GCONF_KEY_VOICE_PITCH, NULL);
    if(value)
    {
        _voice_pitch = gconf_value_get_int(value);
        gconf_value_free(value);
    }
    else
        _voice_pitch = 3;

    /* Get Always Keep On flag.  Default is FALSE. */
    _always_keep_on = gconf_client_get_bool(gconf_client,
            GCONF_KEY_ALWAYS_KEEP_ON, NULL);

    /* Get Units.  Default is UNITS_KM. */
    {
        gchar *units_str = gconf_client_get_string(gconf_client,
                GCONF_KEY_UNITS, NULL);
        guint i = 0;
        if(units_str)
            for(i = UNITS_ENUM_COUNT - 1; i > 0; i--)
                if(!strcmp(units_str, UNITS_TEXT[i]))
                    break;
        _units = i;
    }

    /* Get Escape Key.  Default is ESCAPE_KEY_TOGGLE_TRACKS. */
    {
        gchar *escape_key_str = gconf_client_get_string(gconf_client,
                GCONF_KEY_ESCAPE_KEY, NULL);
        guint i = 0;
        if(escape_key_str)
            for(i = ESCAPE_KEY_ENUM_COUNT - 1; i > 0; i--)
                if(!strcmp(escape_key_str, ESCAPE_KEY_TEXT[i]))
                    break;
        _escape_key = i;
    }

    /* Get Deg format.  Default is DDPDDDDD. */
    {
        gchar *degformat_key_str = gconf_client_get_string(gconf_client,
                GCONF_KEY_DEG_FORMAT, NULL);
        guint i = 0;
        if(degformat_key_str)
            for(i = DEG_FORMAT_ENUM_COUNT - 1; i > 0; i--)
                if(!strcmp(degformat_key_str, DEG_FORMAT_TEXT[i]))
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
        guint i = 0;
        if(speed_location_str)
            for(i = SPEED_LOCATION_ENUM_COUNT - 1; i > 0; i--)
                if(!strcmp(speed_location_str, SPEED_LOCATION_TEXT[i]))
                    break;
        _speed_location = i;
    }

    /* Get Info Font Size.  Default is INFO_FONT_MEDIUM. */
    {
        gchar *info_font_size_str = gconf_client_get_string(gconf_client,
                GCONF_KEY_INFO_FONT_SIZE, NULL);
        guint i = -1;
        if(info_font_size_str)
            for(i = INFO_FONT_ENUM_COUNT - 1; i >= 0; i--)
                if(!strcmp(info_font_size_str, INFO_FONT_TEXT[i]))
                    break;
        if(i == -1)
            i = INFO_FONT_MEDIUM;
        _info_font_size = i;
    }

    /* Get last saved latitude.  Default is 0. */
    _gps.latitude = gconf_client_get_float(gconf_client, GCONF_KEY_LAT, NULL);

    /* Get last saved longitude.  Default is somewhere in Midwest. */
    value = gconf_client_get(gconf_client, GCONF_KEY_LON, NULL);
    _gps.longitude = gconf_client_get_float(gconf_client, GCONF_KEY_LON, NULL);

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
            center_lat = _gps.latitude;

        /* Get last saved longitude.  Default is last saved longitude. */
        value = gconf_client_get(gconf_client, GCONF_KEY_CENTER_LON, NULL);
        if(value)
        {
            center_lon = gconf_value_get_float(value);
            gconf_value_free(value);
        }
        else
            center_lon = _gps.longitude;

        latlon2unit(center_lat, center_lon, _center.unitx, _center.unity);
    }

    /* Load the repositories. */
    {
        GSList *list, *curr;
        guint curr_repo_index = gconf_client_get_int(gconf_client,
            GCONF_KEY_CURRREPO, NULL);
        list = gconf_client_get_list(gconf_client,
            GCONF_KEY_REPOSITORIES, GCONF_VALUE_STRING, NULL);

        for(curr = list; curr != NULL; curr = curr->next)
        {
            /* Parse each part of a repo, delimited by newline characters:
             * 1. url
             * 2. cache_dir
             * 3. dl_zoom_steps
             * 4. view_zoom_steps
             */
            gchar *str = curr->data;
            RepoData *rd = g_new(RepoData, 1);
            rd->name = g_strdup(strsep(&str, "\n"));
            rd->url = g_strdup(strsep(&str, "\n"));
            rd->cache_dir = g_strdup(strsep(&str, "\n"));
            if(!(rd->dl_zoom_steps = atoi(strsep(&str, "\n"))))
                rd->dl_zoom_steps = 2;
            if(!(rd->view_zoom_steps = atoi(strsep(&str, "\n"))))
                rd->view_zoom_steps = 1;

            _repo_list = g_list_append(_repo_list, rd);
            if(!curr_repo_index--)
                _curr_repo = rd;
            g_free(curr->data);
        }
        g_slist_free(list);
    }

    if(_repo_list == NULL)
    {
        /* We have no repositories - create a default one. */
        RepoData *repo = g_new(RepoData, 1);

        /* Many fields can be retrieved from the "old" gconf keys. */

        /* Get Map Cache Dir.  Default is ~/MyDocs/.documents/Maps. */
        repo->cache_dir = gconf_client_get_string(gconf_client,
                GCONF_KEY_MAP_DIR_NAME, NULL);

        if(!repo->cache_dir)
            repo->cache_dir = gnome_vfs_expand_initial_tilde(
                    "~/MyDocs/.documents/Maps");

        /* Get Map Download URI Format.  Default is "". */
        repo->url = gconf_client_get_string(gconf_client,
                GCONF_KEY_MAP_URI_FORMAT, NULL);
        if(!repo->url)
            repo->url = g_strdup("");

        /* Get Map Download Zoom Steps.  Default is 2. */
        repo->dl_zoom_steps = gconf_client_get_int(gconf_client,
                GCONF_KEY_MAP_ZOOM_STEPS, NULL);
        if(!repo->dl_zoom_steps)
            repo->dl_zoom_steps = 2;

        /* Other fields are brand new. */
        repo->name = g_strdup("Default");
        repo->view_zoom_steps = 1;
        _repo_list = g_list_append(_repo_list, repo);
        _curr_repo = repo;
    }

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
    BOUND(_zoom, 0, MAX_ZOOM - 1);
    _world_size_tiles = unit2tile(WORLD_SIZE_UNITS);

    /* Speed and Heading are always initialized as 0. */
    _gps.speed = 0.f;
    _gps.heading = 0.f;

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

    /* Get POI Database.  Default is in ~/MyDocs/.documents/Maps */
    _poi_db = gconf_client_get_string(gconf_client,
            GCONF_KEY_POI_DB, NULL);
    if(_poi_db == NULL)
        _poi_db = gnome_vfs_make_uri_full_from_relative(
                _curr_repo->cache_dir, "poi.db");
    db_connect();

    _poi_zoom = gconf_client_get_int(gconf_client,
            GCONF_KEY_POI_ZOOM, NULL);
    if(!_poi_zoom)
    _poi_zoom = 6;


    /* Get GPS Info flag.  Default is FALSE. */
    value = gconf_client_get(gconf_client, GCONF_KEY_GPS_INFO, NULL);
    if(value)
    {
        _gps_info = gconf_value_get_bool(value);
        gconf_value_free(value);
    }
    else
        _gps_info = FALSE;

    /* Get Route Download Radius.  Default is 4. */
    value = gconf_client_get(gconf_client, GCONF_KEY_ROUTE_DL_RADIUS, NULL);
    if(value)
    {
        _route_dl_radius = gconf_value_get_int(value);
        gconf_value_free(value);
    }
    else
        _route_dl_radius = 4;

    /* Initialize colors. */
    str = gconf_client_get_string(gconf_client,
            GCONF_KEY_COLOR_MARK, NULL);
    if(!str || !gdk_color_parse(str, &_color_mark))
        _color_mark = DEFAULT_COLOR_MARK;

    str = gconf_client_get_string(gconf_client,
            GCONF_KEY_COLOR_MARK_VELOCITY, NULL);
    if(!str || !gdk_color_parse(str, &_color_mark_velocity))
        _color_mark_velocity = DEFAULT_COLOR_MARK_VELOCITY;

    str = gconf_client_get_string(gconf_client,
            GCONF_KEY_COLOR_MARK_OLD, NULL);
    if(!str || !gdk_color_parse(str, &_color_mark_old))
        _color_mark_old = DEFAULT_COLOR_MARK_OLD;

    str = gconf_client_get_string(gconf_client,
            GCONF_KEY_COLOR_TRACK, NULL);
    if(!str || !gdk_color_parse(str, &_color_track))
        _color_track = DEFAULT_COLOR_TRACK;

    str = gconf_client_get_string(gconf_client,
            GCONF_KEY_COLOR_TRACK_BREAK, NULL);
    if(!str || !gdk_color_parse(str, &_color_track_break))
        _color_track_break = DEFAULT_COLOR_TRACK_BREAK;

    str = gconf_client_get_string(gconf_client,
            GCONF_KEY_COLOR_ROUTE, NULL);
    if(!str || !gdk_color_parse(str, &_color_route))
        _color_route = DEFAULT_COLOR_ROUTE;

    str = gconf_client_get_string(gconf_client,
            GCONF_KEY_COLOR_ROUTE_WAY, NULL);
    if(!str || !gdk_color_parse(str, &_color_route_way))
        _color_route_way = DEFAULT_COLOR_ROUTE_WAY;

    str = gconf_client_get_string(gconf_client,
            GCONF_KEY_COLOR_ROUTE_NEXTWAY, NULL);
    if(!str || !gdk_color_parse(str, &_color_route_nextway))
        _color_route_nextway = DEFAULT_COLOR_ROUTE_NEXTWAY;

    str = gconf_client_get_string(gconf_client,
            GCONF_KEY_COLOR_POI, NULL);
    if(!str || !gdk_color_parse(str, &_color_poi))
        _color_poi = DEFAULT_COLOR_POI;

    /* Get current proxy settings. */
    config_update_proxy();

    gconf_client_clear_cache(gconf_client);
    g_object_unref(gconf_client);

    /* GPS data init */
    _gps.fix = 1;
    _gps.satinuse = 0;
    _gps.satinview = 0;

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

static void
menu_maps_remove_repos()
{
    GList *curr;
    printf("%s()\n", __PRETTY_FUNCTION__);

    /* Delete one menu item for each repo. */
    for(curr = _repo_list; curr; curr = curr->next)
    {
        gtk_widget_destroy(gtk_container_get_children(
                    GTK_CONTAINER(_menu_maps_submenu))->data);
    }
    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

static void
menu_maps_add_repos()
{
    GList *curr;
    GtkWidget *last_repo = NULL;
    printf("%s()\n", __PRETTY_FUNCTION__);

    for(curr = g_list_last(_repo_list); curr; curr = curr->prev)
    {
        RepoData *rd = (RepoData*)curr->data;
        GtkWidget *menu_item;
        if(last_repo)
            gtk_menu_prepend(_menu_maps_submenu, menu_item
                    = gtk_radio_menu_item_new_with_label_from_widget(
                        GTK_RADIO_MENU_ITEM(last_repo), rd->name));
        else
        {
            gtk_menu_prepend(_menu_maps_submenu, menu_item
                    = gtk_radio_menu_item_new_with_label(NULL, rd->name));
            last_repo = menu_item;
        }
        gtk_check_menu_item_set_active(
                GTK_CHECK_MENU_ITEM(menu_item),
                rd == _curr_repo);
        rd->menu_item = menu_item;
    }

    /* Add signals (must be after entire menu is built). */
    {
        GList *currmi = gtk_container_get_children(
                GTK_CONTAINER(_menu_maps_submenu));
        for(curr = _repo_list; curr; curr = curr->next, currmi = currmi->next)
        {
            g_signal_connect(G_OBJECT(currmi->data), "activate",
                             G_CALLBACK(menu_cb_maps_select), curr->data);
        }
    }

    gtk_widget_show_all(_menu_maps_submenu);
    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

/**
 * Create the menu items needed for the drop down menu.
 */
static void
menu_init()
{
    /* Create needed handles. */
    GtkMenu *menu;
    GtkWidget *submenu;
    GtkWidget *menu_item;
    printf("%s()\n", __PRETTY_FUNCTION__);

    /* Get the menu of our view. */
    menu = GTK_MENU(gtk_menu_new());

    /* Create the menu items. */

    /* The "Routes" submenu. */
    gtk_menu_append(menu, menu_item
            = gtk_menu_item_new_with_label(_("Route")));
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item),
            submenu = gtk_menu_new());
    gtk_menu_append(submenu, _menu_route_open_item
            = gtk_menu_item_new_with_label(_("Open...")));
    gtk_menu_append(submenu, _menu_route_download_item
            = gtk_menu_item_new_with_label(_("Download...")));
    gtk_menu_append(submenu, _menu_route_save_item
            = gtk_menu_item_new_with_label(_("Save...")));
    gtk_menu_append(submenu, _menu_route_reset_item
        = gtk_menu_item_new_with_label(_("Reset")));
    gtk_menu_append(submenu, _menu_route_clear_item
        = gtk_menu_item_new_with_label(_("Clear")));

    /* The "Track" submenu. */
    gtk_menu_append(menu, menu_item
            = gtk_menu_item_new_with_label(_("Track")));
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item),
            submenu = gtk_menu_new());
    gtk_menu_append(submenu, _menu_track_open_item
            = gtk_menu_item_new_with_label(_("Open...")));
    gtk_menu_append(submenu, _menu_track_save_item
            = gtk_menu_item_new_with_label(_("Save...")));
    gtk_menu_append(submenu, _menu_track_mark_way_item
            = gtk_menu_item_new_with_label(_("Insert Breakpoint")));
    gtk_menu_append(submenu, _menu_track_clear_item
            = gtk_menu_item_new_with_label(_("Clear")));

    gtk_menu_append(menu, menu_item
            = gtk_menu_item_new_with_label(_("Maps")));
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item),
            _menu_maps_submenu = gtk_menu_new());
    gtk_menu_append(_menu_maps_submenu, gtk_separator_menu_item_new());
    gtk_menu_append(_menu_maps_submenu, _menu_maps_mapman_item
            = gtk_menu_item_new_with_label(_("Manage Maps...")));
    gtk_menu_append(_menu_maps_submenu, _menu_maps_repoman_item
            = gtk_menu_item_new_with_label(_("Manage Repositories...")));
    gtk_menu_append(_menu_maps_submenu, _menu_auto_download_item
            = gtk_check_menu_item_new_with_label(_("Auto-Download")));
    gtk_check_menu_item_set_active(
            GTK_CHECK_MENU_ITEM(_menu_auto_download_item), _auto_download);
    menu_maps_add_repos(_curr_repo);

    gtk_menu_append(menu, gtk_separator_menu_item_new());

    gtk_menu_append(menu, menu_item
            = gtk_menu_item_new_with_label(_("View")));
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item),
            submenu = gtk_menu_new());
    gtk_menu_append(submenu, _menu_fullscreen_item
            = gtk_check_menu_item_new_with_label(_("Full Screen")));
    gtk_check_menu_item_set_active(
            GTK_CHECK_MENU_ITEM(_menu_fullscreen_item), _fullscreen);
    gtk_menu_append(submenu, _menu_show_routes_item
            = gtk_check_menu_item_new_with_label(_("Route")));
    gtk_check_menu_item_set_active(
            GTK_CHECK_MENU_ITEM(_menu_show_routes_item),
            _show_tracks & ROUTES_MASK);
    gtk_menu_append(submenu, _menu_show_tracks_item
            = gtk_check_menu_item_new_with_label(_("Track")));
    gtk_check_menu_item_set_active(
            GTK_CHECK_MENU_ITEM(_menu_show_tracks_item),
            _show_tracks & TRACKS_MASK);
    gtk_menu_append(submenu, _menu_show_velvec_item
            = gtk_check_menu_item_new_with_label(_("Velocity Vector")));
    gtk_check_menu_item_set_active(
            GTK_CHECK_MENU_ITEM(_menu_show_velvec_item), _show_velvec);
    gtk_menu_append(submenu, _menu_poi_item
            = gtk_menu_item_new_with_label(_("POI Categories...")));
    gtk_widget_set_sensitive(_menu_poi_item, _dbconn);


    gtk_menu_append(menu, menu_item
            = gtk_menu_item_new_with_label(_("Auto-Center")));
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item),
            submenu = gtk_menu_new());
    gtk_menu_append(submenu, _menu_ac_latlon_item
            = gtk_radio_menu_item_new_with_label(NULL, _("Lat/Lon")));
    gtk_check_menu_item_set_active(
            GTK_CHECK_MENU_ITEM(_menu_ac_latlon_item),
            _center_mode == CENTER_LATLON);
    gtk_menu_append(submenu, _menu_ac_lead_item
            = gtk_radio_menu_item_new_with_label_from_widget(
                GTK_RADIO_MENU_ITEM(_menu_ac_latlon_item), _("Lead")));
    gtk_check_menu_item_set_active(
            GTK_CHECK_MENU_ITEM(_menu_ac_lead_item),
            _center_mode == CENTER_LEAD);
    gtk_menu_append(submenu, _menu_ac_none_item
            = gtk_radio_menu_item_new_with_label_from_widget(
                GTK_RADIO_MENU_ITEM(_menu_ac_latlon_item), _("None")));
    gtk_check_menu_item_set_active(
            GTK_CHECK_MENU_ITEM(_menu_ac_none_item),
            _center_mode < 0);

    gtk_menu_append(menu, menu_item
            = gtk_menu_item_new_with_label(_("GPS")));
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item),
            submenu = gtk_menu_new());
    gtk_menu_append(submenu, _menu_enable_gps_item
            = gtk_check_menu_item_new_with_label(_("Enable GPS")));
    gtk_check_menu_item_set_active(
            GTK_CHECK_MENU_ITEM(_menu_enable_gps_item), _enable_gps);
    gtk_menu_append(submenu, _menu_gps_show_info_item
            = gtk_check_menu_item_new_with_label(_("Show Information")));
    gtk_check_menu_item_set_active(
            GTK_CHECK_MENU_ITEM(_menu_gps_show_info_item), _gps_info);
    gtk_menu_append(submenu, _menu_gps_details_item
            = gtk_menu_item_new_with_label(_("Details...")));
    gtk_widget_set_sensitive(GTK_WIDGET(_menu_gps_details_item), _enable_gps);
    gtk_menu_append(submenu, _menu_gps_reset_item
        = gtk_menu_item_new_with_label(_("Reset Bluetooth")));
    gtk_widget_set_sensitive(GTK_WIDGET(_menu_gps_reset_item), _enable_gps);

    gtk_menu_append(menu, gtk_separator_menu_item_new());

    gtk_menu_append(menu, _menu_settings_item
        = gtk_menu_item_new_with_label(_("Settings...")));

    gtk_menu_append(menu, gtk_separator_menu_item_new());

    gtk_menu_append(menu, _menu_help_item
        = gtk_menu_item_new_with_label(_("Help")));

    gtk_menu_append(menu, _menu_close_item
        = gtk_menu_item_new_with_label(_("Close")));

    /* We need to show menu items. */
    gtk_widget_show_all(GTK_WIDGET(menu));

    hildon_window_set_menu(HILDON_WINDOW(_window), menu);

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
    g_signal_connect(G_OBJECT(_menu_poi_item), "activate",
                      G_CALLBACK(menu_cb_category), NULL);

    g_signal_connect(G_OBJECT(_menu_maps_repoman_item), "activate",
                      G_CALLBACK(menu_cb_maps_repoman), NULL);
    g_signal_connect(G_OBJECT(_menu_maps_mapman_item), "activate",
                      G_CALLBACK(menu_cb_mapman), NULL);
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
    g_signal_connect(G_OBJECT(_menu_gps_show_info_item), "toggled",
                      G_CALLBACK(menu_cb_gps_show_info), NULL);
    g_signal_connect(G_OBJECT(_menu_gps_details_item), "activate",
                      G_CALLBACK(menu_cb_gps_details), NULL);
    g_signal_connect(G_OBJECT(_menu_gps_reset_item), "activate",
                      G_CALLBACK(menu_cb_gps_reset), NULL);
    g_signal_connect(G_OBJECT(_menu_auto_download_item), "toggled",
                      G_CALLBACK(menu_cb_auto_download), NULL);
    g_signal_connect(G_OBJECT(_menu_settings_item), "activate",
                      G_CALLBACK(menu_cb_settings), NULL);
    g_signal_connect(G_OBJECT(_menu_help_item), "activate",
                      G_CALLBACK(menu_cb_help), NULL);
    g_signal_connect(G_OBJECT(_menu_close_item), "activate",
                      G_CALLBACK(gtk_main_quit), NULL);

    /* Setup the context menu. */
    menu = GTK_MENU(gtk_menu_new());

    /* Setup the map context menu. */
    gtk_menu_append(menu, menu_item
            = gtk_menu_item_new_with_label(_("Location")));
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item),
            submenu = gtk_menu_new());

    /* Setup the map context menu. */
    gtk_menu_append(submenu, _cmenu_loc_show_latlon_item
            = gtk_menu_item_new_with_label(_("Show Lat/Lon")));
    gtk_menu_append(submenu, _cmenu_loc_clip_latlon_item
            = gtk_menu_item_new_with_label(_("Copy Lat/Lon to Clipboard")));
    gtk_menu_append(submenu, gtk_separator_menu_item_new());
    gtk_menu_append(submenu, _cmenu_loc_route_to_item
            = gtk_menu_item_new_with_label(_("Download Route to...")));
    gtk_menu_append(submenu, _cmenu_loc_distance_to_item
            = gtk_menu_item_new_with_label(_("Show Distance to")));
    gtk_menu_append(submenu, gtk_separator_menu_item_new());
    gtk_menu_append(submenu, _cmenu_add_poi
                = gtk_menu_item_new_with_label(_("Add POI")));
    gtk_widget_set_sensitive(_cmenu_add_poi, _dbconn);
    gtk_menu_append(submenu, _cmenu_edit_poi
                = gtk_menu_item_new_with_label(_("Edit POI")));
    gtk_widget_set_sensitive(_cmenu_edit_poi, _dbconn);

    /* Setup the waypoint context menu. */
    gtk_menu_append(menu, menu_item
            = gtk_menu_item_new_with_label(_("Waypoint")));
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item),
            submenu = gtk_menu_new());

    gtk_menu_append(submenu, _cmenu_way_show_latlon_item
            = gtk_menu_item_new_with_label(_("Show Lat/Lon")));
    gtk_menu_append(submenu, _cmenu_way_show_desc_item
            = gtk_menu_item_new_with_label(_("Show Description")));
    gtk_menu_append(submenu, _cmenu_way_clip_latlon_item
            = gtk_menu_item_new_with_label(_("Copy Lat/Lon to Clipboard")));
    gtk_menu_append(submenu, _cmenu_way_clip_desc_item
           = gtk_menu_item_new_with_label(_("Copy Description to Clipboard")));
    gtk_menu_append(submenu, gtk_separator_menu_item_new());
    gtk_menu_append(submenu, _cmenu_way_route_to_item
            = gtk_menu_item_new_with_label(_("Download Route to...")));
    gtk_menu_append(submenu, _cmenu_way_distance_to_item
            = gtk_menu_item_new_with_label(_("Show Distance to")));
    gtk_menu_append(submenu, _cmenu_way_delete_item
            = gtk_menu_item_new_with_label(_("Delete")));

    /* Connect signals for context menu. */
    g_signal_connect(G_OBJECT(_cmenu_loc_show_latlon_item), "activate",
                      G_CALLBACK(cmenu_cb_loc_show_latlon), NULL);
    g_signal_connect(G_OBJECT(_cmenu_loc_clip_latlon_item), "activate",
                      G_CALLBACK(cmenu_cb_loc_clip_latlon), NULL);
    g_signal_connect(G_OBJECT(_cmenu_loc_route_to_item), "activate",
                      G_CALLBACK(cmenu_cb_loc_route_to), NULL);
    g_signal_connect(G_OBJECT(_cmenu_loc_distance_to_item), "activate",
                      G_CALLBACK(cmenu_cb_loc_distance_to), NULL);
    g_signal_connect(G_OBJECT(_cmenu_add_poi), "activate",
                        G_CALLBACK(cmenu_cb_add_poi), NULL);
    g_signal_connect(G_OBJECT(_cmenu_edit_poi), "activate",
                        G_CALLBACK(cmenu_cb_edit_poi), NULL);
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

    gtk_widget_show_all(GTK_WIDGET(menu));

    gtk_widget_tap_and_hold_setup(_map_widget, GTK_WIDGET(menu), NULL, 0);

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
            /* Connect to receiver. */
            if(_enable_gps)
                rcvr_connect_now();
        }
        else
            gtk_main_quit();
    }
    gtk_window_present(GTK_WINDOW(_window));

    /* Re-enable any banners that might have been up. */
    {
        ConnState old_state = _conn_state;
        set_conn_state(RCVR_OFF);
        set_conn_state(old_state);
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return FALSE;
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
                _conn_state == RCVR_FIXED ? _gc_mark : _gc_mark_old,
                FALSE, /* not filled. */
                _mark_x1 - _draw_line_width, _mark_y1 - _draw_line_width,
                2 * _draw_line_width, 2 * _draw_line_width,
                0, 360 * 64);
        gdk_draw_line(
                _map_widget->window,
                _conn_state == RCVR_FIXED
                    ? (_show_velvec ? _gc_mark_velocity : _gc_mark)
                    : _gc_mark_old,
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
map_pixbuf_scale_inplace(GdkPixbuf* pixbuf, guint ratio_p2,
        guint src_x, guint src_y)
{
    guint dest_x = 0, dest_y = 0, dest_dim = TILE_SIZE_PIXELS;
    guint rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    guint n_channels = gdk_pixbuf_get_n_channels(pixbuf);
    guchar* pixels = gdk_pixbuf_get_pixels(pixbuf);
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
            src_offset_y = (src_y + (y >> ratio_p2)) * rowstride;
            dest_offset_y = (dest_y + y) * rowstride;
            x = dest_dim - 1;
            if((unsigned)(dest_y + y - src_y) < src_dim
                    && (unsigned)(dest_x + x - src_x) < src_dim)
                x -= src_dim;
            for(; x >= 0; x--)
            {
                guint src_offset, dest_offset, i;
                src_offset = src_offset_y + (src_x+(x>>ratio_p2)) * n_channels;
                dest_offset = dest_offset_y + (dest_x + x) * n_channels;
                pixels[dest_offset] = pixels[src_offset];
                for(i = n_channels - 1; i; --i) /* copy other channels */
                    pixels[dest_offset + i] = pixels[src_offset + i];
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
 * Trim pixbufs that are bigger than tiles. (Those pixbufs result, when
 * captions should be cut off.)
 */
static GdkPixbuf*
pixbuf_trim(GdkPixbuf* pixbuf)
{
    vprintf("%s()\n", __PRETTY_FUNCTION__);
    GdkPixbuf* mpixbuf = gdk_pixbuf_new(
            GDK_COLORSPACE_RGB, gdk_pixbuf_get_has_alpha(pixbuf),
            8, TILE_SIZE_PIXELS, TILE_SIZE_PIXELS);

    gdk_pixbuf_copy_area(pixbuf,
            (gdk_pixbuf_get_width(pixbuf) - TILE_SIZE_PIXELS) / 2,
            (gdk_pixbuf_get_height(pixbuf) - TILE_SIZE_PIXELS) / 2,
            TILE_SIZE_PIXELS, TILE_SIZE_PIXELS,
            mpixbuf,
            0, 0);

    g_object_unref(pixbuf);
    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return mpixbuf;
}


/**
 * Given a wms uri pattern, compute the coordinate transformation and
 * trimming.
 * 'proj' is used for the conversion
 */
static gchar*
map_convert_wms_to_wms(gint tilex, gint tiley, gint zoomlevel, gchar* uri)
{
    gchar cmd[BUFFER_SIZE], srs[BUFFER_SIZE];
    gchar *ret = NULL;
    FILE* in;
    gfloat lon1, lat1, lon2, lat2;

    gchar *widthstr   = strcasestr(uri,"WIDTH=");
    gchar *heightstr  = strcasestr(uri,"HEIGHT=");
    gchar *srsstr     = strcasestr(uri,"SRS=EPSG");
    gchar *srsstre    = strchr(srsstr,'&');
    vprintf("%s()\n", __PRETTY_FUNCTION__);

    /* missing: test if found */
    strcpy(srs,"epsg");
    strncpy(srs+4,srsstr+8,256);
    /* missing: test srsstre-srsstr < 526 */
    srs[srsstre-srsstr-4] = 0;
    /* convert to lower, as WMC is EPSG and cs2cs is epsg */

    gint dwidth  = widthstr ? atoi(widthstr+6) - TILE_SIZE_PIXELS : 0;
    gint dheight = heightstr ? atoi(heightstr+7) - TILE_SIZE_PIXELS : 0;

    unit2latlon(tile2zunit(tilex,zoomlevel)
            - pixel2zunit(dwidth/2,zoomlevel),
            tile2zunit(tiley+1,zoomlevel)
            + pixel2zunit((dheight+1)/2,zoomlevel),
            lat1, lon1);

    unit2latlon(tile2zunit(tilex+1,zoomlevel)
            + pixel2zunit((dwidth+1)/2,zoomlevel),
            tile2zunit(tiley,zoomlevel)
            - pixel2zunit(dheight/2,zoomlevel),
            lat2, lon2);

    setlocale(LC_NUMERIC, "C");

    snprintf(cmd, sizeof(cmd),
            "(echo \"%.6f %.6f\"; echo \"%.6f %.6f\") | "
            "/usr/bin/cs2cs +proj=longlat +datum=WGS84 +to +init=%s -f %%.6f "
            " > /tmp/tmpcs2cs ",
             lon1, lat1, lon2, lat2, srs);
    system(cmd);

    if(!(in = g_fopen("/tmp/tmpcs2cs","r")))
        g_printerr("Cannot open results of conversion\n");
    else if(5 != fscanf(in,"%f %f %s %f %f", &lon1, &lat1, cmd, &lon2, &lat2))
    {
        g_printerr("Wrong conversion\n");
        fclose(in);
    }
    else
    {
        fclose(in);
        ret = g_strdup_printf(uri, lon1, lat1, lon2, lat2);
    }

    setlocale(LC_NUMERIC, "");

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return ret;
}


/**
 * Given the xyz coordinates of our map coordinate system, write the qrst
 * quadtree coordinates to buffer.
 */
static void
map_convert_coords_to_quadtree_string(
        gint x, gint y, gint zoomlevel, gchar *buffer)
{
    static const gchar *const quadrant = "qrts";
    gchar *ptr = buffer;
    gint n;
    vprintf("%s()\n", __PRETTY_FUNCTION__);

    *ptr++ = 't';
    for(n = 16 - zoomlevel; n >= 0; n--)
    {
        gint xbit = (x >> n) & 1;
        gint ybit = (y >> n) & 1;
        *ptr++ = quadrant[xbit + 2 * ybit];
    }
    *ptr++ = '\0';
    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

/**
 * Construct the URL that we should fetch, based on the current URI format.
 * This method works differently depending on if a "%s" string is present in
 * the URI format, since that would indicate a quadtree-based map coordinate
 * system.
 */
static gchar*
map_construct_url(guint tilex, guint tiley, guint zoom)
{
    vprintf("%s()\n", __PRETTY_FUNCTION__);
    if(strstr(_curr_repo->url, "%s"))
    {
        /* This is a satellite-map URI. */
        gchar location[MAX_ZOOM + 2];
        map_convert_coords_to_quadtree_string(tilex, tiley, zoom, location);
        return g_strdup_printf(_curr_repo->url, location);
    }
    else if(strstr(_curr_repo->url, "SERVICE=WMS"))
    {
        /* This is a wms-map URI. */
        return map_convert_wms_to_wms(tilex, tiley, zoom, _curr_repo->url);
    }
    else
        /* This is a street-map URI. */
        return g_strdup_printf(_curr_repo->url, tilex, tiley, zoom);
    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

/**
 * Initiate a download of the given xyz coordinates using the given buffer
 * as the URL.  If the map already exists on disk, or if we are already
 * downloading the map, then this method does nothing.
 */
static void
map_initiate_download(guint tilex, guint tiley, guint zoom, gint retries)
{
    ProgressUpdateInfo *pui;
    vprintf("%s(%u, %u, %u, %d)\n", __PRETTY_FUNCTION__, tilex, tiley, zoom,
            retries);

    if(!_iap_connected && !_iap_connecting)
    {
        _iap_connecting = TRUE;
        osso_iap_connect(OSSO_IAP_ANY, OSSO_IAP_REQUESTED_CONNECT, NULL);
    }

    pui = g_slice_new(ProgressUpdateInfo);
    pui->tilex = tilex;
    pui->tiley = tiley;
    pui->zoom = zoom;
    pui->priority = (abs((gint)tilex - unit2tile(_center.unitx))
            + abs((gint)tiley - unit2tile(_center.unity)));
    if(!retries)
        pui->priority = -pui->priority; /* "Negative" makes them lowest pri. */
    pui->retries = retries;
    pui->repo = _curr_repo;

    if(g_tree_lookup(_pui_tree, pui) || g_tree_lookup(_downloading_tree, pui))
    {
        /* Already downloading. */
        g_slice_free(ProgressUpdateInfo, pui);
        return;
    }
    pui->src_str = NULL;
    pui->dest_str = NULL;
    pui->file = NULL;

    g_tree_insert(_pui_tree, pui, pui);
    if(_iap_connected && !_curl_sid)
        _curl_sid = g_timeout_add(100,
                (GSourceFunc)curl_download_timeout, NULL);

    if(!_num_downloads++ && !_download_banner)
        _download_banner = hildon_banner_show_progress(
                _window, NULL, _("Downloading maps"));

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

static void
map_render_poi()
{
    guint unitx, unity;
    gfloat lat1, lat2, lon1, lon2, tmp;
    gchar slat1[10], slat2[10], slon1[10], slon2[10];
    gchar buffer[100];
    gchar **pszResult;
    gint nRow, nColumn, row, poix, poiy;
    GdkPixbuf *pixbuf = NULL;
    GError *error = NULL;
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(_db && _poi_zoom > _zoom)
    {
        unitx = x2unit(1);
        unity = y2unit(1);
        unit2latlon(unitx, unity, lat1, lon1);
        unitx = x2unit(BUF_WIDTH_PIXELS);
        unity = y2unit(BUF_HEIGHT_PIXELS);
        unit2latlon(unitx, unity, lat2, lon2);
        if(lat1 > lat2)
        {
            tmp = lat2;
            lat2 = lat1;
            lat1 = tmp;
        }
        if(lon1 > lon2)
        {
            tmp = lon2;
            lon2 = lon1;
            lon1 = tmp;
        }

        g_ascii_dtostr(slat1, sizeof(slat1), lat1);
        g_ascii_dtostr(slat2, sizeof(slat2), lat2);
        g_ascii_dtostr(slon1, sizeof(slon1), lon1);
        g_ascii_dtostr(slon2, sizeof(slon2), lon2);

        if(SQLITE_OK == sqlite_get_table_printf(_db,
                    "select p.lat, p.lon, lower(p.label), lower(c.label)"
                    " from poi p, category c "
                    " where p.lat between %s and %s "
                    " and p.lon  between %s and %s "
                    " and c.enabled = 1 and p.cat_id = c.cat_id",
                    &pszResult, &nRow, &nColumn, NULL,
                    slat1, slat2, slon1, slon2))
        {
            for(row=1; row<nRow+1; row++)
            {
                lat1 = g_ascii_strtod(pszResult[row*nColumn+0], NULL);
                lon1 = g_ascii_strtod(pszResult[row*nColumn+1], NULL);
                latlon2unit(lat1, lon1, unitx, unity);
                poix = unit2bufx(unitx);
                poiy = unit2bufy(unity);

                /* Try to get icon for specific POI first. */
                snprintf(buffer, sizeof(buffer), "%s/poi/%s.jpg",
                        _curr_repo->cache_dir,
                        pszResult[row*nColumn+2]);
                pixbuf = gdk_pixbuf_new_from_file(buffer, &error);
                if(error)
                {
                    /* No icon for specific POI - try for category. */
                    error = NULL;
                    snprintf(buffer, sizeof(buffer), "%s/poi/%s.jpg",
                            _curr_repo->cache_dir, pszResult[row*nColumn+3]);
                    pixbuf = gdk_pixbuf_new_from_file(buffer, &error);
                }
                if(error)
                {
                    /* No icon for POI or for category - draw default. */
                    error = NULL;
                    gdk_draw_rectangle(_map_pixmap, _gc_poi, TRUE,
                        poix - (gint)(0.5f * _draw_line_width),
                        poiy - (gint)(0.5f * _draw_line_width),
                        3 * _draw_line_width,
                        3 * _draw_line_width);
                }
                else
                {
                    gdk_draw_pixbuf(
                            _map_pixmap,
                            _gc_poi,
                            pixbuf,
                            0, 0,
                            poix - gdk_pixbuf_get_width(pixbuf) / 2,
                            poiy - gdk_pixbuf_get_height(pixbuf) / 2,
                            -1,-1,
                            GDK_RGB_DITHER_NONE, 0, 0);
                    g_object_unref(pixbuf);
                }
             }
            sqlite_free_table(pszResult);
        }
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

static void
map_render_tile(guint tilex, guint tiley, guint destx, guint desty,
        gboolean fast_fail) {
    gchar buffer[BUFFER_SIZE];
    GdkPixbuf *pixbuf = NULL;
    GError *error = NULL;
    guint zoff;

    if(tilex < _world_size_tiles && tiley < _world_size_tiles)
    {
        /* The tile is possible. */
        vprintf("%s(%u, %u, %u, %u)\n", __PRETTY_FUNCTION__,
                tilex, tiley, destx, desty);
        snprintf(buffer, sizeof(buffer), "%s/%u/%u/%u.jpg",
            _curr_repo->cache_dir, _zoom, tilex, tiley);
        pixbuf = gdk_pixbuf_new_from_file(buffer, &error);

        if(error)
        {
            pixbuf = NULL;
            error = NULL;
        }

        if(!pixbuf && !fast_fail && _auto_download && *_curr_repo->url
                && !(_zoom % _curr_repo->dl_zoom_steps))
        {
            map_initiate_download(tilex, tiley, _zoom,
                    -INITIAL_DOWNLOAD_RETRIES);
            fast_fail = TRUE;
        }
        /* Check if we need to trim. */
        else if(pixbuf && (gdk_pixbuf_get_width(pixbuf) != TILE_SIZE_PIXELS
                    || gdk_pixbuf_get_height(pixbuf) != TILE_SIZE_PIXELS))
            pixbuf = pixbuf_trim(pixbuf);

        for(zoff = 1; !pixbuf && (_zoom + zoff) <= MAX_ZOOM
                && zoff <= TILE_SIZE_P2; zoff += 1)
        {
            /* Attempt to blit a wider map. */
            snprintf(buffer, sizeof(buffer), "%s/%u/%u/%u.jpg",
                    _curr_repo->cache_dir, _zoom + zoff,
                    (tilex >> zoff), (tiley >> zoff));
            pixbuf = gdk_pixbuf_new_from_file(buffer, &error);
            if(error)
            {
                pixbuf = NULL;
                error = NULL;
            }
            if(pixbuf)
            {
                /* Check if we need to trim. */
                if(gdk_pixbuf_get_width(pixbuf) != TILE_SIZE_PIXELS
                        || gdk_pixbuf_get_height(pixbuf) != TILE_SIZE_PIXELS)
                    pixbuf = pixbuf_trim(pixbuf);
                map_pixbuf_scale_inplace(pixbuf, zoff,
                    (tilex - ((tilex>>zoff) << zoff)) << (TILE_SIZE_P2-zoff),
                    (tiley - ((tiley>>zoff) << zoff)) << (TILE_SIZE_P2-zoff));
            }
            else
            {
                if(_auto_download && *_curr_repo->url
                        && !((_zoom + zoff) % _curr_repo->dl_zoom_steps))
                {
                    if(!fast_fail)
                        map_initiate_download(
                                tilex >> zoff, tiley >> zoff, _zoom + zoff,
                                -INITIAL_DOWNLOAD_RETRIES);
                    fast_fail = TRUE;
                }
            }
        }
    }
    if(pixbuf)
    {
        gdk_draw_pixbuf(
                _map_pixmap,
                _gc_mark,
                pixbuf,
                0, 0,
                destx, desty,
                TILE_SIZE_PIXELS, TILE_SIZE_PIXELS,
                GDK_RGB_DITHER_NONE, 0, 0);
        g_object_unref(pixbuf);
    }
    else
    {
        gdk_draw_rectangle(_map_pixmap, _map_widget->style->black_gc, TRUE,
                destx, desty,
                TILE_SIZE_PIXELS, TILE_SIZE_PIXELS);
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

static gboolean
map_download_idle_refresh(ProgressUpdateInfo *pui)
{
    vprintf("%s(%p)\n", __PRETTY_FUNCTION__, pui);

    /* Test if download succeeded (only if retries != 0). */
    if(!pui->retries || g_file_test(pui->dest_str, G_FILE_TEST_EXISTS))
    {
        gint zoom_diff = pui->zoom - _zoom;
        /* Only refresh at same or "lower" (more detailed) zoom level. */
        if(zoom_diff >= 0)
        {
            /* If zoom has changed since we first put in the request for
             * this tile, then we may have to update more than one tile. */
            guint tilex, tiley, tilex_end, tiley_end;
            for(tilex = pui->tilex << zoom_diff,
                    tilex_end = tilex + (1 << zoom_diff);
                    tilex < tilex_end; tilex++)
            {
                for(tiley = pui->tiley<<zoom_diff,
                        tiley_end = tiley + (1 << zoom_diff);
                        tiley < tiley_end; tiley++)
                {
                    if((tilex-_base_tilex) < 4 && (tiley-_base_tiley) < 3)
                    {
                        map_render_tile(
                                tilex, tiley,
                                ((tilex - _base_tilex) << TILE_SIZE_P2),
                                ((tiley - _base_tiley) << TILE_SIZE_P2),
                                TRUE);
                        map_render_paths();
                        map_render_poi();
                        gtk_widget_queue_draw_area(
                            _map_widget,
                            ((tilex-_base_tilex)<<TILE_SIZE_P2) - _offsetx,
                            ((tiley-_base_tiley)<<TILE_SIZE_P2) - _offsety,
                            TILE_SIZE_PIXELS, TILE_SIZE_PIXELS);
                    }
                }
            }
        }
    }
    /* Else the download failed. Update retries and maybe try again. */
    else
    {
        if(pui->retries > 0)
            --pui->retries;
        else if(pui->retries < 0)
            ++pui->retries;
        if(pui->retries)
        {
            /* removal automatically calls progress_update_info_free(). */
            g_tree_steal(_downloading_tree, pui);
            g_tree_insert(_pui_tree, pui, pui);
            if(_iap_connected && !_curl_sid)
                _curl_sid = g_timeout_add(100,
                        (GSourceFunc)curl_download_timeout, NULL);
            /* Don't do anything else. */
            return FALSE;
        }
    }

    /* removal automatically calls progress_update_info_free(). */
    g_tree_remove(_downloading_tree, pui);

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
    map_render_paths();
    map_render_poi();
    MACRO_QUEUE_DRAW_AREA();

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

static gboolean
get_next_pui(gpointer key, gpointer value, ProgressUpdateInfo **data)
{
    *data = key;
    return TRUE;
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

        g_free(_autoroute_data.dest);
        _autoroute_data.dest = NULL;

        g_free(_autoroute_data.src_str);
        _autoroute_data.src_str = NULL;

        if(_autoroute_data.curl_easy)
        {
            if(_curl_multi)
                curl_multi_remove_handle(_curl_multi,
                        _autoroute_data.curl_easy);
            curl_easy_cleanup(_autoroute_data.curl_easy);
            _autoroute_data.curl_easy = NULL;
        }

        g_free(_autoroute_data.rdl_data.bytes);
        _autoroute_data.rdl_data.bytes = NULL;
        _autoroute_data.rdl_data.bytes_read = 0;

        _autoroute_data.in_progress = FALSE;
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

static gboolean
curl_download_timeout()
{
    static guint destroy_counter = 50;
    gint num_transfers = 0, num_msgs = 0;
    gint deletes_left = 50; /* only do 50 deletes at a time. */
    CURLMsg *msg;
    vprintf("%s()\n", __PRETTY_FUNCTION__);

    if(_curl_multi && CURLM_CALL_MULTI_PERFORM
            == curl_multi_perform(_curl_multi, &num_transfers))
        return TRUE; /* Give UI a chance first. */

    while(_curl_multi && (msg = curl_multi_info_read(_curl_multi, &num_msgs)))
    {
        if(msg->msg == CURLMSG_DONE)
        {
            if(msg->easy_handle == _autoroute_data.curl_easy)
            {
                /* This is the autoroute download. */
                /* Now, parse the autoroute and update the display. */
                if(_autoroute_data.enabled && parse_route_gpx(
                       _autoroute_data.rdl_data.bytes,
                       _autoroute_data.rdl_data.bytes_read, 0))
                {
                    /* Find the nearest route point, if we're connected. */
                    route_find_nearest_point();
                    map_force_redraw();
                }
                cancel_autoroute(); /* We're done. Clean up. */
            }
            else
            {
                ProgressUpdateInfo *pui = g_hash_table_lookup(
                        _pui_by_easy, msg->easy_handle);
                g_queue_push_head(_curl_easy_queue, msg->easy_handle);
                g_hash_table_remove(_pui_by_easy, msg->easy_handle);
                fclose(pui->file);
                if(msg->data.result != CURLE_OK)
                {
                    if(!pui->retries)
                    {
                        /* No more retries left - something must be wrong. */
                        MACRO_BANNER_SHOW_INFO(_window,
                            _("Error in download.  Check internet connection"
                                " and/or URL Format."));
                    }
                    g_unlink(pui->dest_str); /* Delete so we try again. */
                }
                curl_multi_remove_handle(_curl_multi, msg->easy_handle);
                g_idle_add_full(G_PRIORITY_HIGH_IDLE,
                        (GSourceFunc)map_download_idle_refresh, pui, NULL);
            }
        }
    }

    /* Up to 1 transfer per tile. */
    while(num_transfers < (BUF_WIDTH_TILES * BUF_HEIGHT_TILES)
            && g_tree_nnodes(_pui_tree))
    {
        ProgressUpdateInfo *pui;
        g_tree_foreach(_pui_tree, (GTraverseFunc)get_next_pui, &pui);

        if(pui->retries)
        {
            /* This is a download. */
            FILE *f;
            g_tree_steal(_pui_tree, pui);
            g_tree_insert(_downloading_tree, pui, pui);

            pui->src_str = map_construct_url(pui->tilex, pui->tiley,pui->zoom);
            pui->dest_str = g_strdup_printf("%s/%u/%u/%u.jpg",
                    pui->repo->cache_dir, pui->zoom, pui->tilex, pui->tiley);

            /* Check to see if we need to overwrite. */
            if(pui->retries > 0)
            {
                /* We're not updating - check if file already exists. */
                if(g_file_test(pui->dest_str, G_FILE_TEST_EXISTS))
                {
                    g_idle_add_full(G_PRIORITY_HIGH_IDLE,
                            (GSourceFunc)map_download_idle_refresh, pui, NULL);
                    continue;
                }
            }

            /* Attempt to open the file for writing. */
            if(!(f = g_fopen(pui->dest_str, "w")) && errno == ENOENT)
            {
                /* Directory doesn't exist yet - create it, then we'll retry */
                gchar buffer[BUFFER_SIZE];
                snprintf(buffer, sizeof(buffer), "%s/%u",
                        pui->repo->cache_dir, pui->zoom);
                gnome_vfs_make_directory(buffer, 0775);
                snprintf(buffer, sizeof(buffer), "%s/%u/%u",
                        pui->repo->cache_dir, pui->zoom, pui->tilex);
                gnome_vfs_make_directory(buffer, 0775);
                f = g_fopen(pui->dest_str, "w");
            }

            if(f)
            {
                CURL *curl_easy;
                pui->file = f;
                curl_easy = g_queue_pop_tail(_curl_easy_queue);
                if(!curl_easy)
                {
                    /* Need a new curl_easy. */
                    MACRO_CURL_EASY_INIT(curl_easy);
                }
                curl_easy_setopt(curl_easy, CURLOPT_URL, pui->src_str);
                curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, f);
                g_hash_table_insert(_pui_by_easy, curl_easy, pui);
                if(!_curl_multi)
                {
                    /* Initialize CURL. */
                    _curl_multi = curl_multi_init();
                    /*curl_multi_setopt(_curl_multi, CURLMOPT_PIPELINING, 1);*/
                }
                curl_multi_add_handle(_curl_multi, curl_easy);
                num_transfers++;
            }
            else
            {
                /* Unable to download file. */
                gchar buffer[BUFFER_SIZE];
                snprintf(buffer, sizeof(buffer), "%s:\n%s",
                        _("Failed to open file for writing"), pui->dest_str);
                MACRO_BANNER_SHOW_INFO(_window, buffer);
                g_idle_add_full(G_PRIORITY_HIGH_IDLE,
                        (GSourceFunc)map_download_idle_refresh, pui, NULL);
                continue;
            }
        }
        else if(--deletes_left)
        {
            /* This is a delete. */
            gchar buffer[BUFFER_SIZE];
            g_tree_steal(_pui_tree, pui);
            g_tree_insert(_downloading_tree, pui, pui);

            snprintf(buffer, sizeof(buffer), "%s/%u/%u/%u.jpg",
                    pui->repo->cache_dir, pui->zoom, pui->tilex, pui->tiley);
            g_unlink(buffer);
            g_idle_add_full(G_PRIORITY_HIGH_IDLE,
                    (GSourceFunc)map_download_idle_refresh, pui, NULL);
        }
        else
            break;
    }

    if(!(num_transfers || g_tree_nnodes(_pui_tree)))
    {
        /* Destroy curl after 50 counts (5 seconds). */
        if(--destroy_counter)
        {
            /* Clean up curl. */
            CURL *curr;
            while((curr = g_queue_pop_tail(_curl_easy_queue)))
                curl_easy_cleanup(curr);

            curl_multi_cleanup(_curl_multi);
            _curl_multi = NULL;

            _curl_sid = 0;
            vprintf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
            return FALSE;
        }
    }
    else
        destroy_counter = 50;

    vprintf("%s(): return TRUE (%d, %d)\n", __PRETTY_FUNCTION__,
            num_transfers, g_tree_nnodes(_pui_tree));
    return TRUE;
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
    if(new_zoom > (MAX_ZOOM - 1))
        return;
    _zoom = new_zoom / _curr_repo->view_zoom_steps
                     * _curr_repo->view_zoom_steps;
    _world_size_tiles = unit2tile(WORLD_SIZE_UNITS);

    /* If we're leading, update the center to reflect new zoom level. */
    MACRO_RECALC_CENTER(_center.unitx, _center.unity);

    /* Update center bounds to reflect new zoom level. */
    _min_center.unitx = pixel2unit(grid2pixel(_screen_grids_halfwidth));
    _min_center.unity = pixel2unit(grid2pixel(_screen_grids_halfheight));
    _max_center.unitx = WORLD_SIZE_UNITS-grid2unit(_screen_grids_halfwidth) -1;
    _max_center.unity = WORLD_SIZE_UNITS-grid2unit(_screen_grids_halfheight)-1;

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
                            _gc_mark,
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
        map_render_paths();
        map_render_poi();
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

    /* Draw speed info */
    if(_speed_limit_on)
        speed_limit();

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
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

    dialog= hildon_file_chooser_dialog_new(GTK_WINDOW(_window),chooser_action);

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
            gchar buffer[BUFFER_SIZE];
            snprintf(buffer, sizeof(buffer),
                    "%s %s:\n%s", _("Failed to open file for"),
                    chooser_action == GTK_FILE_CHOOSER_ACTION_OPEN
                    ? _("reading") : _("writing"),
                    gnome_vfs_result_to_string(vfs_result));
            popup_error(dialog, buffer);
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
            g_free(*dir);
            *dir = gtk_file_chooser_get_current_folder_uri(
                    GTK_FILE_CHOOSER(dialog));
        }

        /* If desired, save the file for later. */
        if(file)
        {
            g_free(*file);
            *file = gtk_file_chooser_get_uri(GTK_FILE_CHOOSER(dialog));
        }
    }

    gtk_widget_destroy(dialog);

    vprintf("%s(): return %d\n", __PRETTY_FUNCTION__, success);
    return success;
}

/**
 * Read the data provided by the given handle as GPX data, updating the
 * auto-route with that data.
 */
static size_t
route_dl_cb_read(void *ptr, size_t size, size_t nmemb,
        RouteDownloadData *rdl_data)
{
    size_t old_size = rdl_data->bytes_read;
    vprintf("%s()\n", __PRETTY_FUNCTION__);

    rdl_data->bytes_read += size * nmemb;
    rdl_data->bytes = g_renew(gchar, rdl_data->bytes,
            rdl_data->bytes_read);
    g_memmove(rdl_data->bytes + old_size, ptr, size * nmemb);

    vprintf("%s(): return %d\n", __PRETTY_FUNCTION__, size * nmemb);
    return (size * nmemb);
}

/**
 * This function is periodically run to download updated auto-route data
 * from the route source.
 */
static gboolean
auto_route_dl_idle()
{
    gchar latstr[32], lonstr[32];
    vprintf("%s(%f, %f, %s)\n", __PRETTY_FUNCTION__,
            _gps.latitude, _gps.longitude, _autoroute_data.dest);

    g_ascii_dtostr(latstr, 32, _gps.latitude);
    g_ascii_dtostr(lonstr, 32, _gps.longitude);
    _autoroute_data.src_str = g_strdup_printf(
            "http://www.gnuite.com/cgi-bin/gpx.cgi?saddr=%s,%s&daddr=%s",
            latstr, lonstr, _autoroute_data.dest);

    MACRO_CURL_EASY_INIT(_autoroute_data.curl_easy);
    curl_easy_setopt(_autoroute_data.curl_easy, CURLOPT_URL,
            _autoroute_data.src_str);
    curl_easy_setopt(_autoroute_data.curl_easy, CURLOPT_WRITEFUNCTION,
            route_dl_cb_read);
    curl_easy_setopt(_autoroute_data.curl_easy, CURLOPT_WRITEDATA,
            &_autoroute_data.rdl_data);
    if(!_curl_multi)
    {
        /* Initialize CURL. */
        _curl_multi = curl_multi_init();
        /*curl_multi_setopt(_curl_multi, CURLMOPT_PIPELINING, 1);*/
    }
    curl_multi_add_handle(_curl_multi, _autoroute_data.curl_easy);
    if(_iap_connected && !_curl_sid)
        _curl_sid = g_timeout_add(100,
                (GSourceFunc)curl_download_timeout, NULL);
    _autoroute_data.in_progress = TRUE;

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return FALSE;
}

/**
 * Save state and destroy all non-UI elements created by this program in
 * preparation for exiting.
 */
static void
maemo_mapper_destroy(void)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(_curl_sid)
    {
        g_source_remove(_curl_sid);
        _curl_sid = 0;
    }
    config_save();
    rcvr_disconnect();
    /* _program and widgets have already been destroyed. */

    if(_dbconn)
        sqlite_close(_db);

    MACRO_CLEAR_TRACK(_track);
    if(_route.head)
        MACRO_CLEAR_TRACK(_route);

    /* Clean up CURL. */
    if(_curl_multi)
    {
        CURL *curr;
        CURLMsg *msg;
        gint num_transfers, num_msgs;

        /* First, remove all downloads from _pui_tree. */
        g_tree_destroy(_pui_tree);

        /* Finish up all downloads. */
        while(CURLM_CALL_MULTI_PERFORM
                == curl_multi_perform(_curl_multi, &num_transfers)
                || num_transfers) { }

        /* Close all finished files. */
        while((msg = curl_multi_info_read(_curl_multi, &num_msgs)))
        {
            if(msg->msg == CURLMSG_DONE)
            {
                /* This is a map download. */
                ProgressUpdateInfo *pui = g_hash_table_lookup(
                        _pui_by_easy, msg->easy_handle);
                g_queue_push_head(_curl_easy_queue, msg->easy_handle);
                g_hash_table_remove(_pui_by_easy, msg->easy_handle);
                fclose(pui->file);
                curl_multi_remove_handle(_curl_multi, msg->easy_handle);
            }
        }

        while((curr = g_queue_pop_tail(_curl_easy_queue)))
            curl_easy_cleanup(curr);

        curl_multi_cleanup(_curl_multi);
        _curl_multi = NULL;

        g_queue_free(_curl_easy_queue);
        g_tree_destroy(_downloading_tree);
        g_hash_table_destroy(_pui_by_easy);
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

static DBusHandlerResult
get_connection_status_signal_cb(DBusConnection *connection,
        DBusMessage *message, void *user_data)
{
    gchar *iap_name = NULL, *iap_nw_type = NULL, *iap_state = NULL;
    printf("%s()\n", __PRETTY_FUNCTION__);

    /* check signal */
    if(!dbus_message_is_signal(message,
                ICD_DBUS_INTERFACE,
                ICD_STATUS_CHANGED_SIG))
    {
        vprintf("%s(): return DBUS_HANDLER_RESULT_NOT_YET_HANDLED\n",
                __PRETTY_FUNCTION__);
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    if(!dbus_message_get_args(message, NULL,
                DBUS_TYPE_STRING, &iap_name,
                DBUS_TYPE_STRING, &iap_nw_type,
                DBUS_TYPE_STRING, &iap_state,
                DBUS_TYPE_INVALID))
    {
        vprintf("%s(): return DBUS_HANDLER_RESULT_NOT_YET_HANDLED\n",
                __PRETTY_FUNCTION__);
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    printf("  > iap_state = %s\n", iap_state);
    if(!strcmp(iap_state, "CONNECTED"))
    {
        if(!_iap_connected)
        {
            _iap_connected = TRUE;
            config_update_proxy();
            if(!_curl_sid)
                _curl_sid = g_timeout_add(100,
                        (GSourceFunc)curl_download_timeout, NULL);
        }
    }
    else if(_iap_connected)
    {
        _iap_connected = FALSE;
        if(_curl_sid)
        {
            g_source_remove(_curl_sid);
            _curl_sid = 0;
        }
    }

    vprintf("%s(): return DBUS_HANDLER_RESULT_HANDLED\n",
            __PRETTY_FUNCTION__);
    return DBUS_HANDLER_RESULT_HANDLED;
}

static void
iap_callback(struct iap_event_t *event, void *arg)
{
    printf("%s()\n", __PRETTY_FUNCTION__);
    _iap_connecting = FALSE;
    if(event->type == OSSO_IAP_CONNECTED && !_iap_connected)
    {
        _iap_connected = TRUE;
        config_update_proxy();
        if(!_curl_sid)
            _curl_sid = g_timeout_add(100,
                    (GSourceFunc)curl_download_timeout, NULL);
    }
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
    UNITS_TEXT[UNITS_KM] = _("km");
    UNITS_TEXT[UNITS_MI] = _("mi.");
    UNITS_TEXT[UNITS_NM] = _("n.m.");

    INFO_FONT_TEXT[INFO_FONT_XXSMALL] = "xx-small";
    INFO_FONT_TEXT[INFO_FONT_XSMALL] = "x-small";
    INFO_FONT_TEXT[INFO_FONT_SMALL] = "small";
    INFO_FONT_TEXT[INFO_FONT_MEDIUM] = "medium";
    INFO_FONT_TEXT[INFO_FONT_LARGE] = "large";
    INFO_FONT_TEXT[INFO_FONT_XLARGE] = "x-large";
    INFO_FONT_TEXT[INFO_FONT_XXLARGE] = "xx-large";

    ESCAPE_KEY_TEXT[ESCAPE_KEY_TOGGLE_TRACKS] = _("Toggle Tracks");
    ESCAPE_KEY_TEXT[ESCAPE_KEY_CHANGE_REPO] = _("Next Repository");
    ESCAPE_KEY_TEXT[ESCAPE_KEY_RESET_BLUETOOTH] = _("Reset Bluetooth");
    ESCAPE_KEY_TEXT[ESCAPE_KEY_TOGGLE_GPS] = _("Toggle GPS");
    ESCAPE_KEY_TEXT[ESCAPE_KEY_TOGGLE_GPSINFO] = _("Toggle GPS Info");
    ESCAPE_KEY_TEXT[ESCAPE_KEY_TOGGLE_SPEEDLIMIT] = _("Toggle Speed Limit");

    DEG_FORMAT_TEXT[DDPDDDDD] = "dd.ddddd\u00b0";
    DEG_FORMAT_TEXT[DD_MMPMMM] = "dd\u00b0mm.mmm'";
    DEG_FORMAT_TEXT[DD_MM_SSPS] = "dd\u00b0mm'ss.s\"";

    SPEED_LOCATION_TEXT[SPEED_LOCATION_TOP_LEFT] = _("Top-Left");
    SPEED_LOCATION_TEXT[SPEED_LOCATION_TOP_RIGHT] = _("Top-Right");
    SPEED_LOCATION_TEXT[SPEED_LOCATION_BOTTOM_RIGHT] = _("Bottom-Right");
    SPEED_LOCATION_TEXT[SPEED_LOCATION_BOTTOM_LEFT] = _("Bottom-Left");

    /* Set up track array (must be done before config). */
    memset(&_track, 0, sizeof(_track));
    memset(&_route, 0, sizeof(_route));
    MACRO_INIT_TRACK(_track);

    config_init();

    /* Initialize _program. */
    _program = HILDON_PROGRAM(hildon_program_get_instance());
    g_set_application_name("Maemo Mapper");

    /* Initialize _window. */
    _window = GTK_WIDGET(hildon_window_new());
    hildon_program_add_window(_program, HILDON_WINDOW(_window));

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

    _map_pixmap = gdk_pixmap_new(
                _map_widget->window,
                BUF_WIDTH_PIXELS, BUF_HEIGHT_PIXELS,
                -1); /* -1: use bit depth of widget->window. */

    _curl_easy_queue = g_queue_new();

    _pui_tree = g_tree_new_full(
            (GCompareDataFunc)download_comparefunc, NULL,
            (GDestroyNotify)progress_update_info_free, NULL);
    _downloading_tree = g_tree_new_full(
            (GCompareDataFunc)download_comparefunc, NULL,
            (GDestroyNotify)progress_update_info_free, NULL);

    _pui_by_easy = g_hash_table_new(g_direct_hash, g_direct_equal);

    /* Connect signals. */
    g_signal_connect(G_OBJECT(_sat_panel), "expose_event",
            G_CALLBACK(sat_panel_expose), NULL);
    g_signal_connect(G_OBJECT(_heading_panel), "expose_event",
            G_CALLBACK(heading_panel_expose), NULL);
    g_signal_connect(G_OBJECT(_window), "destroy",
            G_CALLBACK(gtk_main_quit), NULL);

    g_signal_connect(G_OBJECT(_window), "key_press_event",
            G_CALLBACK(window_cb_key_press), NULL);

    g_signal_connect(G_OBJECT(_window), "key_release_event",
            G_CALLBACK(window_cb_key_release), NULL);

    g_signal_connect(G_OBJECT(_map_widget), "configure_event",
            G_CALLBACK(map_cb_configure), NULL);

    g_signal_connect(G_OBJECT(_map_widget), "expose_event",
            G_CALLBACK(map_cb_expose), NULL);

    g_signal_connect(G_OBJECT(_map_widget), "button_press_event",
            G_CALLBACK(map_cb_button_press), NULL);

    g_signal_connect(G_OBJECT(_map_widget), "button_release_event",
            G_CALLBACK(map_cb_button_release), NULL);

    gtk_widget_add_events(_map_widget,
            GDK_EXPOSURE_MASK
            | GDK_BUTTON_PRESS_MASK
            | GDK_BUTTON_RELEASE_MASK);

    osso_hw_set_event_cb(_osso, NULL, osso_cb_hw_state, NULL);

    /* Initialize data. */

    /* set XML_TZONE */
    {
        time_t time1;
        struct tm time2;
        time1 = time(NULL);
        localtime_r(&time1, &time2);
        snprintf(XML_TZONE, sizeof(XML_TZONE), "%+03ld:%02ld",
                (time2.tm_gmtoff / 60 / 60), (time2.tm_gmtoff / 60) % 60);
        _gmtoffset = time2.tm_gmtoff;
    }

    _last_spoken_phrase = g_strdup("");

    memset(&_autoroute_data, 0, sizeof(_autoroute_data));

    integerize_data();

    /* Initialize our line styles. */
    update_gcs();

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
            gchar buffer[BUFFER_SIZE];
            snprintf(buffer, sizeof(buffer),
                    "%s %s:\n%s", _("Failed to open file for"),
                    _("reading"), gnome_vfs_result_to_string(vfs_result));
            popup_error(_window, buffer);
        }
        else
        {
            /* If auto is enabled, append the route, otherwise replace it. */
            if(parse_route_gpx(buffer, size, 0))
            {
                MACRO_BANNER_SHOW_INFO(_window, _("Route Opened"));
            }
            else
                popup_error(_window, _("Error parsing GPX file."));
            g_free(buffer);
        }
        g_free(file_uri);
    }

    /* If we have a route, calculate the next point. */
    if(_route.head)
        route_find_nearest_point();

    /* Add D-BUS signal handler for 'status_changed' */
    {
        DBusConnection *dbus_conn = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);
        gchar *filter_string = g_strdup_printf(
                "interface=%s", ICD_DBUS_INTERFACE);
        /* add match */
        dbus_bus_add_match(dbus_conn, filter_string, NULL);

        g_free (filter_string);

        /* add the callback */
        dbus_connection_add_filter(dbus_conn,
                    get_connection_status_signal_cb,
                    NULL, NULL);
    }
    osso_iap_cb(iap_callback);

    {
        DBusGConnection *dbus_conn;
        GError *error = NULL;

        /* Initialize D-Bus. */
        if(NULL == (dbus_conn = dbus_g_bus_get(DBUS_BUS_SYSTEM, &error)))
        {
            g_printerr("Failed to open connection to D-Bus: %s.\n",
                    error->message);
            error = NULL;
        }

        if(NULL == (_rfcomm_req_proxy = dbus_g_proxy_new_for_name(
                        dbus_conn,
                        BTCOND_SERVICE,
                        BTCOND_REQ_PATH,
                        BTCOND_REQ_INTERFACE)))
        {
            g_printerr("Failed to open connection to %s.\n",
                    BTCOND_REQ_INTERFACE);
        }
    }

#ifdef DEBUG
    _iap_connected = TRUE;
#endif

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

    /* Init libcurl. */
    curl_global_init(CURL_GLOBAL_NOTHING);

    maemo_mapper_init(argc, argv);

    if(OSSO_OK != osso_rpc_set_default_cb_f(_osso, dbus_cb_default, NULL))
    {
        g_printerr("osso_rpc_set_default_cb_f failed.\n");
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
        else
        {
            if(_conn_state > RCVR_OFF)
            {
                GConfClient *gconf_client = gconf_client_get_default();
                if(!gconf_client || gconf_client_get_bool(
                            gconf_client, GCONF_KEY_DISCONNECT_ON_COVER, NULL))
                {
                    gconf_client_clear_cache(gconf_client);
                    g_object_unref(gconf_client);
                    set_conn_state(RCVR_OFF);
                    rcvr_disconnect();
                    track_add(0, FALSE);
                    /* Pretend autoroute is in progress to avoid download. */
                    if(_autoroute_data.enabled)
                        _autoroute_data.in_progress = TRUE;
                }
            }
            if(_curl_sid)
            {
                g_source_remove(_curl_sid);
                _curl_sid = 0;
            }
        }
    }
    else if(state->save_unsaved_data_ind)
    {
        config_save();
        _must_save_data = TRUE;
    }
    else
    {
        if(_conn_state == RCVR_OFF && _enable_gps)
        {
            set_conn_state(RCVR_DOWN);
            rcvr_connect_later();
            if(_autoroute_data.enabled)
                _autoroute_data.in_progress = TRUE;
        }

        /* Start curl in case there are downloads pending. */
        if(_iap_connected && !_curl_sid)
            _curl_sid = g_timeout_add(100,
                    (GSourceFunc)curl_download_timeout, NULL);
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

static gboolean
key_zoom_timeout()
{
    printf("%s()\n", __PRETTY_FUNCTION__);
    if(_key_zoom_new < _zoom)
    {
        /* We're currently zooming in (_zoom is decreasing). */
        guint test = _key_zoom_new - _curr_repo->view_zoom_steps;
        if(test < MAX_ZOOM)
            /* We can zoom some more.  Hurray! */
            _key_zoom_new = test;
        else
            /* We can't zoom anymore.  Booooo! */
            return FALSE;
    }
    else
    {
        /* We're currently zooming out (_zoom is increasing). */
        guint test = _key_zoom_new + _curr_repo->view_zoom_steps;
        if(test < MAX_ZOOM)
            /* We can zoom some more.  Hurray! */
            _key_zoom_new = test;
        else
            /* We can't zoom anymore.  Booooo! */
            return FALSE;
    }

    /* We can zoom more - tell them how much they're zooming. */
    {
        gchar buffer[32];
        snprintf(buffer, sizeof(buffer),
                "%s %d", _("Zoom to Level"), _key_zoom_new);
        MACRO_BANNER_SHOW_INFO(_window, buffer);
    }
    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static void
reset_bluetooth()
{
    printf("%s()\n", __PRETTY_FUNCTION__);
    if(system("/usr/bin/sudo -l | grep -q '/usr/sbin/hciconfig  *hci0  *reset'"
            " && sudo /usr/sbin/hciconfig hci0 reset"))
        popup_error(_window,
                _("An error occurred while trying to reset the bluetooth "
                "radio.\n\n"
                "Did you make sure to modify\nthe /etc/sudoers file?"));
    else if(_conn_state > RCVR_OFF)
    {
        set_conn_state(RCVR_DOWN);
        rcvr_connect_later();
    }
    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

static gboolean
window_cb_key_press(GtkWidget* widget, GdkEventKey *event)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    switch(event->keyval)
    {
        case HILDON_HARDKEY_UP:
            map_pan(0, -PAN_UNITS);
            return TRUE;

        case HILDON_HARDKEY_DOWN:
            map_pan(0, PAN_UNITS);
            return TRUE;

        case HILDON_HARDKEY_LEFT:
            map_pan(-PAN_UNITS, 0);
            return TRUE;

        case HILDON_HARDKEY_RIGHT:
            map_pan(PAN_UNITS, 0);
            return TRUE;

        case HILDON_HARDKEY_SELECT:
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

        case HILDON_HARDKEY_FULLSCREEN:
            gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(
                        _menu_fullscreen_item), !_fullscreen);
            return TRUE;

        case HILDON_HARDKEY_INCREASE:
        case HILDON_HARDKEY_DECREASE:
            if(!_key_zoom_timeout_sid)
            {
                _key_zoom_new = _zoom
                    + (event->keyval == HILDON_HARDKEY_INCREASE
                            ? -_curr_repo->view_zoom_steps
                            : _curr_repo->view_zoom_steps);
                /* Remember, _key_zoom_new is unsigned. */
                if(_key_zoom_new < MAX_ZOOM)
                {
                    gchar buffer[80];
                    snprintf(buffer, sizeof(buffer),"%s %d",
                            _("Zoom to Level"), _key_zoom_new);
                    MACRO_BANNER_SHOW_INFO(_window, buffer);
                    _key_zoom_timeout_sid = g_timeout_add(
                        500, key_zoom_timeout, NULL);
                }
            }
            return TRUE;

        case HILDON_HARDKEY_ESC:
            switch(_escape_key)
            {
                case ESCAPE_KEY_CHANGE_REPO:
                    {
                        GList *curr = g_list_find(_repo_list, _curr_repo);
                        if(curr && curr->next)
                            curr = curr->next;
                        else
                            curr = _repo_list;
                        _curr_repo = curr->data;
                        gtk_check_menu_item_set_active(
                                GTK_CHECK_MENU_ITEM(_curr_repo->menu_item),
                                TRUE);
                    }
                    break;
                case ESCAPE_KEY_RESET_BLUETOOTH:
                    reset_bluetooth();
                    break;
                case ESCAPE_KEY_TOGGLE_GPS:
                    gtk_check_menu_item_set_active(
                            GTK_CHECK_MENU_ITEM(_menu_enable_gps_item),
                            !_enable_gps);
                    break;
                case ESCAPE_KEY_TOGGLE_GPSINFO:
                    gtk_check_menu_item_set_active(
                            GTK_CHECK_MENU_ITEM(_menu_gps_show_info_item),
                            !_gps_info);
                    break;
                case ESCAPE_KEY_TOGGLE_SPEEDLIMIT:
                    _speed_limit_on ^= 1;
                    break;
                default:
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
                            /* There is no history, or they changed something
                             * since the last historical change. Save and
                             * clear. */
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

        default:
            return FALSE;
    }
    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

static gboolean
window_cb_key_release(GtkWidget* widget, GdkEventKey *event)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    switch(event->keyval)
    {
        case HILDON_HARDKEY_INCREASE:
        case HILDON_HARDKEY_DECREASE:
            if(_key_zoom_timeout_sid)
            {
                g_source_remove(_key_zoom_timeout_sid);
                _key_zoom_timeout_sid = 0;
                map_set_zoom(_key_zoom_new);
            }
            return TRUE;

        default:
            return FALSE;
    }
    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
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
    _max_center.unitx = WORLD_SIZE_UNITS-grid2unit(_screen_grids_halfwidth) -1;
    _max_center.unity = WORLD_SIZE_UNITS-grid2unit(_screen_grids_halfheight)-1;

    map_center_unit(_center.unitx, _center.unity);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
sat_panel_expose(GtkWidget *widget, GdkEventExpose *event)
{
    PangoContext *context = NULL;
    PangoLayout *layout = NULL;
    PangoFontDescription *fontdesc = NULL;
    gchar *tmp = NULL;
    guint x, y;
    printf("%s()\n", __PRETTY_FUNCTION__);

    draw_sat_info(widget,
        0, 0,
        widget->allocation.width,
        widget->allocation.height,
        FALSE);

    context = gtk_widget_get_pango_context(widget);
    layout = pango_layout_new(context);
    fontdesc =  pango_font_description_new();

    pango_font_description_set_family(fontdesc,"Sans Serif");
    pango_font_description_set_size(fontdesc, 14*PANGO_SCALE);

    /* Sat View/In Use */
    tmp = g_strdup_printf("%d/%d", _gps.satinuse, _gps.satinview);
    pango_layout_set_font_description (layout, fontdesc);
    pango_layout_set_text(layout, tmp, strlen(tmp));
    gdk_draw_layout(widget->window,
        widget->style->fg_gc[GTK_STATE_NORMAL],
        20, 2,
        layout);
    g_free(tmp);

    switch(_gps.fix)
    {
        case 2:
        case 3: tmp = g_strdup_printf("%dD fix", _gps.fix); break;
        default: tmp = g_strdup_printf("nofix"); break;
    }
    pango_layout_set_text(layout, tmp, strlen(tmp));
    pango_layout_set_alignment(layout, PANGO_ALIGN_RIGHT);
    pango_layout_get_pixel_size(layout, &x, &y);
    gdk_draw_layout(widget->window,
        widget->style->fg_gc[GTK_STATE_NORMAL],
        widget->allocation.width - 20 - x, 2,
        layout);
    g_free(tmp);

    pango_font_description_free(fontdesc);
    g_object_unref(layout);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
heading_panel_expose(GtkWidget *widget, GdkEventExpose *event)
{
    guint size, xoffset, yoffset, i, x, y;
    gint dir;
    gfloat tmp;
    gchar *text;
    PangoContext            *context=NULL;
    PangoLayout             *layout=NULL;
    PangoFontDescription    *fontdesc=NULL;
    printf("%s()\n", __PRETTY_FUNCTION__);

    size = MIN(widget->allocation.width, widget->allocation.height);
    if(widget->allocation.width > widget->allocation.height)
    {
        xoffset = (widget->allocation.width - widget->allocation.height) / 2;
        yoffset = 0;
    }
    else
    {
        xoffset = 0;
        yoffset = (widget->allocation.height - widget->allocation.width) / 2;
    }
    context = gtk_widget_get_pango_context(widget);
    layout = pango_layout_new(context);
    fontdesc =  pango_font_description_new();
    pango_font_description_set_family(fontdesc,"Sans Serif");
    pango_font_description_set_size(fontdesc,12*PANGO_SCALE);
    pango_layout_set_font_description (layout, fontdesc);
    pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);

    text = g_strdup_printf("%3.0f\u00b0", _gps.heading);
    pango_layout_set_text(layout, text, -1);
    pango_layout_get_pixel_size(layout, &x, &y);

    gdk_draw_layout(widget->window,
        widget->style->fg_gc[GTK_STATE_NORMAL],
        xoffset + size/2 - x/2,
        yoffset + size - y - 2, layout);
    g_free(text);

    gdk_draw_arc (widget->window,
        widget->style->fg_gc[GTK_WIDGET_STATE (widget)],
        FALSE,
        xoffset, yoffset + size/2,
        size, size,
        0, 64 * 180);

    /* Simple arrow for heading*/
    gdk_draw_line(widget->window,
        widget->style->fg_gc[GTK_WIDGET_STATE (widget)],
        xoffset + size/2 + 3,
        yoffset + size - y - 5,
        xoffset + size/2,
        yoffset + size/2 + 5);

    gdk_draw_line(widget->window,
        widget->style->fg_gc[GTK_WIDGET_STATE (widget)],
        xoffset + size/2 - 3,
        yoffset + size - y - 5,
        xoffset + size/2,
        yoffset + size/2 + 5);

    gdk_draw_line(widget->window,
        widget->style->fg_gc[GTK_WIDGET_STATE (widget)],
        xoffset + size/2 - 3,
        yoffset + size - y - 5,
        xoffset + size/2,
        yoffset + size - y - 8);

    gdk_draw_line(widget->window,
        widget->style->fg_gc[GTK_WIDGET_STATE (widget)],
        xoffset + size/2 + 3,
        yoffset + size - y - 5,
        xoffset + size/2,
        yoffset + size - y - 8);

    gint angle[5] = {-90,-45,0,45,90};
    gint fsize[5] = {0,4,10,4,0};
    for(i = 0; i < 5; i++)
    {
        dir = (gint)(_gps.heading/45)*45 + angle[i];

        switch(dir)
        {
            case   0:
            case 360: text = g_strdup("N"); break;
            case  45:
            case 405:
                text = g_strdup("NE"); break;
            case  90:
                text = g_strdup("E"); break;
            case 135:
                text = g_strdup("SE"); break;
            case 180:
                text = g_strdup("S"); break;
            case 225:
                text = g_strdup("SW"); break;
            case 270:
            case -90:
                text = g_strdup("W"); break;
            case 315:
            case -45:
                text = g_strdup("NW"); break;
            default :
                text = g_strdup("??");
                break;
        }

        tmp = ((dir - _gps.heading) * (1.f / 180.f)) * PI;
        gdk_draw_line(widget->window,
            widget->style->fg_gc[GTK_WIDGET_STATE (widget)],
            xoffset + size/2 + ((size/2 - 5) * sinf(tmp)),
            yoffset + size - ((size/2 - 5) * cosf(tmp)),
            xoffset + size/2 + ((size/2 + 5) * sinf(tmp)),
            yoffset + size - ((size/2 + 5) * cosf(tmp)));

        x = fsize[i];
        if(abs((guint)(_gps.heading/45)*45 - _gps.heading)
                > abs((guint)(_gps.heading/45)*45 + 45 - _gps.heading)
                && (i > 0))
            x = fsize[i - 1];

        pango_font_description_set_size(fontdesc,(10 + x)*PANGO_SCALE);
        pango_layout_set_font_description (layout, fontdesc);
        pango_layout_set_text(layout, text, -1);
        pango_layout_get_pixel_size(layout, &x, &y);
        x = xoffset + size/2 + ((size/2 + 15) * sinf(tmp)) - x/2,
        y = yoffset + size - ((size/2 + 15) * cosf(tmp)) - y/2,

        gdk_draw_layout(widget->window,
            widget->style->fg_gc[GTK_STATE_NORMAL],
            x, y, layout);
        g_free(text);
    }

    pango_font_description_free (fontdesc);
    g_object_unref (layout);

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
            _gc_mark,
            _map_pixmap,
            event->area.x + _offsetx, event->area.y + _offsety,
            event->area.x, event->area.y,
            event->area.width, event->area.height);
    map_draw_mark();

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
map_cb_button_press(GtkWidget *widget, GdkEventButton *event)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    _cmenu_position_x = event->x + 0.5;
    _cmenu_position_y = event->y + 0.5;

    /* Return FALSE to allow context menu to work. */
    vprintf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
    return FALSE;
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
        unit2latlon(_pos.unitx, _pos.unity, _gps.latitude, _gps.longitude);
        _gps.speed = 20.f;
        integerize_data();
        track_add(time(NULL), FALSE);
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

    /* Return FALSE to avoid context menu (if it hasn't popped up already). */
    vprintf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
    return FALSE;
}

static gboolean
channel_cb_error(GIOChannel *src, GIOCondition condition, gpointer data)
{
    printf("%s(%d)\n", __PRETTY_FUNCTION__, condition);

    /* An error has occurred - re-connect(). */
    rcvr_disconnect();
    track_add(0, FALSE);
    _speed_excess = FALSE;

    if(_conn_state > RCVR_OFF)
    {
        set_conn_state(RCVR_DOWN);
        gps_hide_text();
        rcvr_connect_later();
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return FALSE;
}

static gboolean
channel_cb_connect(GIOChannel *src, GIOCondition condition, gpointer data)
{
    printf("%s(%d)\n", __PRETTY_FUNCTION__, condition);

    set_conn_state(RCVR_UP);
    _input_sid = g_io_add_watch_full(_channel, G_PRIORITY_HIGH_IDLE,
            G_IO_IN | G_IO_PRI, channel_cb_input, NULL, NULL);

    _connect_sid = 0;
    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return FALSE;
}

#define MACRO_PARSE_INT(tofill, str) { \
    gchar *error_check; \
    (tofill) = strtol((str), &error_check, 10); \
    if(error_check == (str)) \
    { \
        g_printerr("Line %d: Failed to parse string as int: %s\n", \
                __LINE__, str); \
        MACRO_BANNER_SHOW_INFO(_window, \
                _("Invalid NMEA input from receiver!")); \
        return; \
    } \
}
#define MACRO_PARSE_FLOAT(tofill, str) { \
    gchar *error_check; \
    (tofill) = g_ascii_strtod((str), &error_check); \
    if(error_check == (str)) \
    { \
        g_printerr("Failed to parse string as float: %s\n", str); \
        MACRO_BANNER_SHOW_INFO(_window, \
                _("Invalid NMEA input from receiver!")); \
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
    gchar *token, *dpoint, *gpstime = NULL, *gpsdate = NULL, tmp[15];
    gdouble tmpd = 0.f;
    guint tmpi = 0;
    gboolean newly_fixed = FALSE;
    vprintf("%s(): %s\n", __PRETTY_FUNCTION__, sentence);

#define DELIM ","

    /* Parse time. */
    token = strsep(&sentence, DELIM);
    if(*token)
        gpstime = g_strdup(token);

    token = strsep(&sentence, DELIM);
    /* Token is now Status. */
    if(*token != 'A')
    {
        /* Data is invalid - not enough satellites?. */
        if(_conn_state != RCVR_UP)
        {
            set_conn_state(RCVR_UP);
            track_add(0, FALSE);
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
        _gps.latitude = tmpi + (tmpd * (1.0 / 60.0));
    }

    /* Parse N or S. */
    token = strsep(&sentence, DELIM);
    if(*token)
    {
        deg_format(_gps.latitude, tmp);
        snprintf(_gps.slatitude, sizeof(_gps.slatitude),
                "%c %s", token[0], tmp);
        if(token[0] == 'S')
            _gps.latitude = -_gps.latitude;
    }

    /* Parse the longitude. */
    token = strsep(&sentence, DELIM);
    if(*token)
    {
        dpoint = strchr(token, '.');
        MACRO_PARSE_FLOAT(tmpd, dpoint - 2);
        dpoint[-2] = '\0';
        MACRO_PARSE_INT(tmpi, token);
        _gps.longitude = tmpi + (tmpd * (1.0 / 60.0));
    }

    /* Parse E or W. */
    token = strsep(&sentence, DELIM);
    if(*token)
    {
        deg_format(_gps.longitude, tmp);
        snprintf(_gps.slongitude, sizeof(_gps.slongitude),
                "%c %s", token[0], tmp);
        if(*token && token[0] == 'W')
            _gps.longitude = -_gps.longitude;
    }

    /* Parse speed over ground, knots. */
    token = strsep(&sentence, DELIM);
    if(*token)
    {
        MACRO_PARSE_FLOAT(_gps.speed, token);
        if(_gps.fix > 1)
            _gps.maxspeed = MAX(_gps.maxspeed, _gps.speed);
    }

    /* Parse heading, degrees from true north. */
    token = strsep(&sentence, DELIM);
    if(*token)
    {
        MACRO_PARSE_FLOAT(_gps.heading, token);
    }

    /* Parse date. */
    token = strsep(&sentence, DELIM);
    if(*token)
    {
        time_t timeloc;
        struct tm time;
        gpsdate = g_strdup_printf("%s%s", token, gpstime);
        strptime(gpsdate, "%d%m%y%H%M%S", &time);
        if(time.tm_year >= 0 && time.tm_year <= 68)
            time.tm_year += 100; /* year 2000 */
        time.tm_mon -= 1;
        timeloc = mktime(&time);
        timeloc += _gmtoffset;
        localtime_r(&timeloc, &_gps.timeloc);
        g_free(gpstime);
        g_free(gpsdate);
    }

    /* Translate data into integers. */
    integerize_data();

    /* Add new data to track. */
    if(_conn_state == RCVR_FIXED)
        track_add(time(NULL), newly_fixed);

    /* Move mark to new location. */
    refresh_mark();

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

 static void
channel_parse_gga(gchar *sentence)
{
    /* GGA          Global Positioning System Fix Data
     1. Fix Time
     2. Latitude
     3. N or S
     4. Longitude
     5. E or W
     6. Fix quality
                   0 = invalid
                   1 = GPS fix (SPS)
                   2 = DGPS fix
                   3 = PPS fix
                   4 = Real Time Kinematic
                   5 = Float RTK
                   6 = estimated (dead reckoning) (2.3 feature)
                   7 = Manual input mode
                   8 = Simulation mode
     7. Number of satellites being tracked
     8. Horizontal dilution of position
     9. Altitude, Meters, above mean sea level
     10. Alt unit (meters)
     11. Height of geoid (mean sea level) above WGS84 ellipsoid
     12. unit
     13. (empty field) time in seconds since last DGPS update
     14. (empty field) DGPS station ID number
     15. the checksum data
     */
    gchar *token;
    vprintf("%s(): %s\n", __PRETTY_FUNCTION__, sentence);

#define DELIM ","

    /* Skip Fix time */
    token = strsep(&sentence, DELIM);
    /* Skip latitude */
    token = strsep(&sentence, DELIM);
    /* Skip N or S */
    token = strsep(&sentence, DELIM);
    /* Skip longitude */
    token = strsep(&sentence, DELIM);
    /* Skip S or W */
    token = strsep(&sentence, DELIM);

    /* Parse Fix quality */
    token = strsep(&sentence, DELIM);
    if(*token)
        MACRO_PARSE_INT(_gps.fixquality, token);

    /* Skip number of satellites */
    token = strsep(&sentence, DELIM);

    /* Parse Horizontal dilution of position */
    token = strsep(&sentence, DELIM);
    if(*token)
        MACRO_PARSE_INT(_gps.hdop, token);

    /* Altitude */
    token = strsep(&sentence, DELIM);
    if(*token)
    {
        MACRO_PARSE_FLOAT(_gps.altitude, token);
    }
    else
        _gps.altitude = NAN;

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

static void
channel_parse_gsa(gchar *sentence)
{
    /* GPS DOP and active satellites
     *  1) Auto selection of 2D or 3D fix (M = manual)
     *  2) 3D fix - values include: 1 = no fix, 2 = 2D, 3 = 2D
     *  3) PRNs of satellites used for fix
     *     (space for 12)
     *  4) PDOP (dilution of precision)
     *  5) Horizontal dilution of precision (HDOP)
     *  6) Vertical dilution of precision (VDOP)
     *  7) Checksum
     */
    gchar *token;
    guint i;
    vprintf("%s(): %s\n", __PRETTY_FUNCTION__, sentence);

#define DELIM ","

    /* Skip Auto selection. */
    token = strsep(&sentence, DELIM);

    /* 3D fix. */
    token = strsep(&sentence, DELIM);
    MACRO_PARSE_INT(_gps.fix, token);

    _gps.satinuse = 0;
    for(i = 0; i < 12; i++)
    {
        token = strsep(&sentence, DELIM);
        if(*token)
            _gps.satforfix[_gps.satinuse++] = atoi(token);
    }

    /* PDOP */
    token = strsep(&sentence, DELIM);
    if(*token)
        MACRO_PARSE_FLOAT(_gps.pdop, token);

    /* HDOP */
    token = strsep(&sentence, DELIM);
    if(*token)
        MACRO_PARSE_FLOAT(_gps.hdop, token);

    /* VDOP */
    token = strsep(&sentence, DELIM);
    if(*token)
        MACRO_PARSE_FLOAT(_gps.vdop, token);

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
    static guint satcnt = 0;
    vprintf("%s(): %s\n", __PRETTY_FUNCTION__, sentence);

    /* Parse number of messages. */
    token = strsep(&sentence, DELIM);
    if(!*token)
        return; /* because this is an invalid sentence. */
    MACRO_PARSE_INT(nummsgs, token);

    /* Parse message number. */
    token = strsep(&sentence, DELIM);
    if(!*token)
        return; /* because this is an invalid sentence. */
    MACRO_PARSE_INT(msgcnt, token);

    /* Parse number of satellites in view. */
    token = strsep(&sentence, DELIM);
    if(*token)
        MACRO_PARSE_INT(_gps.satinview, token);

    /* Loop until there are no more satellites to parse. */
    while(sentence && satcnt < 12)
    {
        /* Get token for Satellite Number. */
        token = strsep(&sentence, DELIM);
        if(!sentence)
            break; /* because this is an invalid sentence. */
        _gps_sat[satcnt].prn = atoi(token);

        /* Get token for elevation in degrees (0-90). */
        token = strsep(&sentence, DELIM);
        if(!sentence)
            break; /* because this is an invalid sentence. */
        _gps_sat[satcnt].elevation = atoi(token);

        /* Get token for azimuth in degrees to true north (0-359). */
        token = strsep(&sentence, DELIM);
        if(!sentence)
            break; /* because this is an invalid sentence. */
        _gps_sat[satcnt].azimuth = atoi(token);

        /* Get token for SNR. */
        token = strsep(&sentence, DELIM);
        if((_gps_sat[satcnt].snr = atoi(token)))
        {
            /* SNR is non-zero - add to total and count as used. */
            running_total += _gps_sat[satcnt].snr;
            num_sats_used++;
        }
        satcnt++;
    }

    if(msgcnt == nummsgs)
    {
        /*  This is the last message. Calculate signal strength. */
        if(num_sats_used)
        {
            if(_conn_state == RCVR_UP)
            {
                gdouble fraction = running_total * sqrt(num_sats_used)
                    / num_sats_used / 100.0;
                BOUND(fraction, 0.0, 1.0);
                hildon_banner_set_fraction(
                        HILDON_BANNER(_fix_banner), fraction);
            }
            running_total = 0;
            num_sats_used = 0;
        }
        satcnt = 0;

        /* Keep awake while they watch the progress bar. */
        KEEP_DISPLAY_ON();
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

static gboolean
channel_cb_input(GIOChannel *src, GIOCondition condition, gpointer data)
{
    gsize bytes_read;
    vprintf("%s(%d)\n", __PRETTY_FUNCTION__, condition);

    if(G_IO_STATUS_NORMAL == g_io_channel_read_chars(
                _channel,
                _gps_read_buf_curr,
                _gps_read_buf_last - _gps_read_buf_curr,
                &bytes_read,
                NULL))
    {
        gchar *eol;
        _gps_read_buf_curr += bytes_read;
        *_gps_read_buf_curr = '\0'; /* append a \0 so we can read as string */
        while((eol = strchr(_gps_read_buf, '\n')))
        {
            gchar *sptr = _gps_read_buf + 1; /* Skip the $ */
            guint csum = 0;
            if(*_gps_read_buf == '$')
            {
                /* This is the beginning of a sentence; okay to parse. */
                *eol = '\0'; /* overwrite \n with \0 */
                while(*sptr && *sptr != '*')
                    csum ^= *sptr++;

                /* If we're at a \0 (meaning there is no checksum), or if the
                 * checksum is good, then parse the sentence. */
                if(!*sptr || csum == strtol(sptr + 1, NULL, 16))
                {
                    if(*sptr)
                        *sptr = '\0'; /* take checksum out of the buffer. */
                    if(!strncmp(_gps_read_buf + 3, "GSV", 3))
                    {
                        if(_conn_state == RCVR_UP
                                || _gps_info || _satdetails_on)
                            channel_parse_gsv(_gps_read_buf + 7);
                    }
                    else if(!strncmp(_gps_read_buf + 3, "RMC", 3))
                        channel_parse_rmc(_gps_read_buf + 7);
                    else if(!strncmp(_gps_read_buf + 3, "GGA", 3))
                        channel_parse_gga(_gps_read_buf + 7);
                    else if(!strncmp(_gps_read_buf + 3, "GSA", 3))
                        channel_parse_gsa(_gps_read_buf + 7);

                    if(_gps_info)
                        gps_display_data();
                    if(_satdetails_on)
                        gps_display_details();
                }
                else
                {
                    /* There was a checksum, and it was bad. */
                    g_printerr("%s: Bad checksum in NMEA sentence:\n%s\n",
                            __PRETTY_FUNCTION__, _gps_read_buf);
                }
            }

            /* If eol is at or after (_gps_read_buf_curr - 1) */
            if(eol >= (_gps_read_buf_curr - 1))
            {
                /* Last read was a newline - reset read buffer */
                _gps_read_buf_curr = _gps_read_buf;
                *_gps_read_buf_curr = '\0';
            }
            else
            {
                /* Move the next line to the front of the buffer. */
                memmove(_gps_read_buf, eol + 1,
                        _gps_read_buf_curr - eol); /* include terminating 0 */
                /* Subtract _curr so that it's pointing at the new \0. */
                _gps_read_buf_curr -= (eol - _gps_read_buf + 1);
            }
        }
    }

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
gps_toggled_from(GtkToggleButton *chk_gps,
        GtkWidget *txt_from)
{
    gchar buffer[80];
    gchar strlat[32];
    gchar strlon[32];
    printf("%s()\n", __PRETTY_FUNCTION__);

    g_ascii_formatd(strlat, 32, "%.06f", _gps.latitude);
    g_ascii_formatd(strlon, 32, "%.06f", _gps.longitude);
    snprintf(buffer, sizeof(buffer), "%s, %s", strlat, strlon);
    gtk_widget_set_sensitive(txt_from, !gtk_toggle_button_get_active(chk_gps));
    gtk_entry_set_text(GTK_ENTRY(txt_from), buffer);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
gps_toggled_auto(GtkToggleButton *chk_gps,
        GtkWidget *chk_auto)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    gtk_widget_set_sensitive(chk_auto, gtk_toggle_button_get_active(chk_gps));

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

/**
 * Display a dialog box to the user asking them to download a route.  The
 * "From" and "To" textfields may be initialized using the first two
 * parameters.  The third parameter, if set to TRUE, will cause the "Use GPS
 * Location" checkbox to be enabled, which automatically sets the "From" to the
 * current GPS position (this overrides any value that may have been passed as
 * the "To" initializer).
 * None of the passed strings are freed - that is left to the caller, and it is
 * safe to free either string as soon as this function returns.
 */
static gboolean
route_download(gchar *from, gchar *to, gboolean from_here)
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
    printf("%s()\n", __PRETTY_FUNCTION__);

    /* Connect to the internet pre-emptively to prevent lack thereof. */
    if(!_iap_connected && !_iap_connecting)
    {
        _iap_connecting = TRUE;
        osso_iap_connect(OSSO_IAP_ANY, OSSO_IAP_REQUESTED_CONNECT, NULL);
    }

    dialog = gtk_dialog_new_with_buttons(_("Download Route"),
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
                _("Use GPS Location")),
            TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox),
            chk_auto = gtk_check_button_new_with_label(
                _("Auto-Update")),
            TRUE, TRUE, 0);
    gtk_widget_set_sensitive(chk_auto, FALSE);

    /* Origin. */
    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new(_("Origin")),
            0, 1, 1, 2, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(table),
            txt_from = gtk_entry_new(),
            1, 2, 1, 2, GTK_EXPAND | GTK_FILL, 0, 2, 4);
    gtk_entry_set_completion(GTK_ENTRY(txt_from), from_comp);
    gtk_entry_set_width_chars(GTK_ENTRY(txt_from), 25);

    /* Destination. */
    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new(_("Destination")),
            0, 1, 2, 3, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(table),
            txt_to = gtk_entry_new(),
            1, 2, 2, 3, GTK_EXPAND | GTK_FILL, 0, 2, 4);
    gtk_entry_set_completion(GTK_ENTRY(txt_to), to_comp);
    gtk_entry_set_width_chars(GTK_ENTRY(txt_to), 25);

    g_signal_connect(G_OBJECT(chk_gps), _("toggled"),
                      G_CALLBACK(gps_toggled_from), txt_from);
    g_signal_connect(G_OBJECT(chk_gps), _("toggled"),
                      G_CALLBACK(gps_toggled_auto), chk_auto);

    /* Initialize fields. */
    gtk_entry_set_text(GTK_ENTRY(txt_from), (from ? from : ""));
    gtk_entry_set_text(GTK_ENTRY(txt_to), (to ? to : ""));

    if(from_here)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chk_gps), TRUE);

    gtk_widget_show_all(dialog);

    while(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(dialog)))
    {
        CURL *curl_easy;
        RouteDownloadData rdl_data = {0, 0};
        gchar buffer[BUFFER_SIZE];
        const gchar *from, *to;
        gchar *from_escaped, *to_escaped;

        from = gtk_entry_get_text(GTK_ENTRY(txt_from));
        if(!strlen(from))
        {
            popup_error(dialog, _("Please specify a start location."));
            continue;
        }

        to = gtk_entry_get_text(GTK_ENTRY(txt_to));
        if(!strlen(to))
        {
            popup_error(dialog, _("Please specify an end location."));
            continue;
        }

        from_escaped = gnome_vfs_escape_string(from);
        to_escaped = gnome_vfs_escape_string(to);
        snprintf(buffer, sizeof(buffer),
                "http://www.gnuite.com/cgi-bin/gpx.cgi?saddr=%s&daddr=%s",
                from_escaped, to_escaped);
        g_free(from_escaped);
        g_free(to_escaped);

        /* Attempt to download the route from the server. */
        MACRO_CURL_EASY_INIT(curl_easy);
        curl_easy_setopt(curl_easy, CURLOPT_URL, buffer);
        curl_easy_setopt(curl_easy, CURLOPT_WRITEFUNCTION,
                route_dl_cb_read);
        curl_easy_setopt(curl_easy, CURLOPT_WRITEDATA, &rdl_data);
        if(CURLE_OK != curl_easy_perform(curl_easy))
        {
            popup_error(dialog,
                    _("Failed to connect to GPX Directions server"));
            curl_easy_cleanup(curl_easy);
            g_free(rdl_data.bytes);
            /* Let them try again */
            continue;
        }
        curl_easy_cleanup(curl_easy);

        if(strncmp(rdl_data.bytes, "<?xml", strlen("<?xml")))
        {
            /* Not an XML document - must be bad locations. */
            popup_error(dialog,
                    _("Could not generate directions. Make sure your "
                    "source and destination are valid."));
            g_free(rdl_data.bytes);
            /* Let them try again. */
        }
        /* Else, if GPS is enabled, append the route, otherwise replace it. */
        else if(parse_route_gpx(rdl_data.bytes, rdl_data.bytes_read,
                    (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(chk_gps))
                        ? 0 : 1)))
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

            MACRO_BANNER_SHOW_INFO(_window, _("Route Downloaded"));
            g_free(rdl_data.bytes);

            /* Success! Get out of the while loop. */
            break;
        }
        else
        {
            popup_error(dialog, _("Error parsing GPX file."));
            g_free(rdl_data.bytes);
            /* Let them try again. */
        }
    }

    gtk_widget_destroy(dialog);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_route_download(GtkAction *action)
{
    printf("%s()\n", __PRETTY_FUNCTION__);
    route_download(NULL, NULL, FALSE);
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
        if(parse_route_gpx(buffer, size, _autoroute_data.enabled ? 0 : 1))
        {
            cancel_autoroute();

            /* Find the nearest route point, if we're connected. */
            route_find_nearest_point();

            map_force_redraw();
            MACRO_BANNER_SHOW_INFO(_window, _("Route Opened"));
        }
        else
            popup_error(_window, _("Error parsing GPX file."));
        g_free(buffer);
    }

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_route_reset(GtkAction *action)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(_route.head)
    {
        route_find_nearest_point();
        map_render_paths();
        map_render_poi();
        MACRO_QUEUE_DRAW_AREA();
    }

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
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

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
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
        if(parse_track_gpx(buffer, size, -1))
        {
            map_force_redraw();
            MACRO_BANNER_SHOW_INFO(_window, _("Track Opened"));
        }
        else
            popup_error(_window, _("Error parsing GPX file."));
        g_free(buffer);
    }

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
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
        if(write_track_gpx(handle))
        {
            MACRO_BANNER_SHOW_INFO(_window, _("Track Saved"));
        }
        else
            popup_error(_window, _("Error writing GPX file."));
        gnome_vfs_close(handle);
    }

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_track_mark_way(GtkAction *action)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(_track.tail->point.unity)
    {
        guint x1, y1;

        /* To mark a "waypoint" in a track, we'll add a (0, 0) point and then
         * another instance of the most recent track point. */
        MACRO_TRACK_INCREMENT_TAIL(_track);
        *_track.tail = _track_null;
        MACRO_TRACK_INCREMENT_TAIL(_track);
        *_track.tail = _track.tail[2];

        /** Instead of calling map_render_paths(), we'll just add the waypoint
         * ourselves. */
        x1 = unit2bufx(_track.tail->point.unitx);
        y1 = unit2bufy(_track.tail->point.unity);
        /* Make sure this circle will be visible. */
        if((x1 < BUF_WIDTH_PIXELS)
                && ((unsigned)y1 < BUF_HEIGHT_PIXELS))
            gdk_draw_arc(_map_pixmap, _gc_track_break,
                    FALSE, /* FALSE: not filled. */
                    x1 - _draw_line_width,
                    y1 - _draw_line_width,
                    2 * _draw_line_width,
                    2 * _draw_line_width,
                    0, /* start at 0 degrees. */
                    360 * 64);
    }

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_route_save(GtkAction *action)
{
    GnomeVFSHandle *handle;
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(!_route.head)
    {
        popup_error(_window, _("No route is loaded."));
        return TRUE;
    }

    if(open_file(NULL, &handle, NULL, &_route_dir_uri, NULL,
                    GTK_FILE_CHOOSER_ACTION_SAVE))
    {
        if(write_route_gpx(handle))
        {
            MACRO_BANNER_SHOW_INFO(_window, _("Route Saved"));
        }
        else
            popup_error(_window, _("Error writing GPX file."));
        gnome_vfs_close(handle);
    }

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_track_clear(GtkAction *action)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    _track.tail = _track.head;
    map_force_redraw();

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
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
        map_render_paths();
        MACRO_QUEUE_DRAW_AREA();
        MACRO_BANNER_SHOW_INFO(_window, _("Tracks are now shown"));
    }
    else
    {
        _show_tracks &= ~TRACKS_MASK;
        map_force_redraw();
        MACRO_BANNER_SHOW_INFO(_window, _("Tracks are now hidden"));
    }

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
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
        map_render_paths();
        MACRO_QUEUE_DRAW_AREA();
        MACRO_BANNER_SHOW_INFO(_window, _("Routes are now shown"));
    }
    else
    {
        _show_tracks &= ~ROUTES_MASK;
        map_force_redraw();
        MACRO_BANNER_SHOW_INFO(_window, _("Routes are now hidden"));
    }

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_show_velvec(GtkAction *action)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    _show_velvec = gtk_check_menu_item_get_active(
            GTK_CHECK_MENU_ITEM(_menu_show_velvec_item));
    map_move_mark();
    gtk_widget_set_sensitive(GTK_WIDGET(_menu_gps_details_item), _enable_gps);
    gps_show_info();

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_gps_show_info(GtkAction *action)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    _gps_info = gtk_check_menu_item_get_active(
                GTK_CHECK_MENU_ITEM(_menu_gps_show_info_item));

    gps_show_info();

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_ac_lead(GtkAction *action)
{
    guint new_center_unitx, new_center_unity;
    printf("%s()\n", __PRETTY_FUNCTION__);

    _center_mode = CENTER_LEAD;
    MACRO_BANNER_SHOW_INFO(_window, _("Auto-Center Mode: Lead"));
    MACRO_RECALC_CENTER(new_center_unitx, new_center_unity);
    map_center_unit(new_center_unitx, new_center_unity);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_ac_latlon(GtkAction *action)
{
    guint new_center_unitx, new_center_unity;
    printf("%s()\n", __PRETTY_FUNCTION__);

    _center_mode = CENTER_LATLON;
    MACRO_BANNER_SHOW_INFO(_window, _("Auto-Center Mode: Lat/Lon"));
    MACRO_RECALC_CENTER(new_center_unitx, new_center_unity);
    map_center_unit(new_center_unitx, new_center_unity);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_ac_none(GtkAction *action)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    _center_mode = -_center_mode;
    MACRO_BANNER_SHOW_INFO(_window, _("Auto-Center Off"));

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}


typedef struct _RepoManInfo RepoManInfo;
struct _RepoManInfo {
    GtkWidget *dialog;
    GtkWidget *notebook;
    GtkWidget *cmb_repos;
    GList *repo_edits;
};

typedef struct _RepoEditInfo RepoEditInfo;
struct _RepoEditInfo {
    gchar *name;
    GtkWidget *page_table;
    GtkWidget *txt_url;
    GtkWidget *txt_cache_dir;
    GtkWidget *num_dl_zoom_steps;
    GtkWidget *num_view_zoom_steps;
    GtkWidget *btn_browse;
    BrowseInfo browse_info;
};

static gboolean
repoman_dialog_select(GtkWidget *widget, RepoManInfo *rmi)
{
    printf("%s()\n", __PRETTY_FUNCTION__);
    gint curr_index = gtk_combo_box_get_active(GTK_COMBO_BOX(rmi->cmb_repos));
    gtk_notebook_set_current_page(GTK_NOTEBOOK(rmi->notebook), curr_index);
    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
repoman_dialog_browse(GtkWidget *widget, BrowseInfo *browse_info)
{
    GtkWidget *dialog;
    printf("%s()\n", __PRETTY_FUNCTION__);

    dialog = GTK_WIDGET(
            hildon_file_chooser_dialog_new(GTK_WINDOW(browse_info->dialog),
            GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER));

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
repoman_dialog_rename(GtkWidget *widget, RepoManInfo *rmi)
{
    GtkWidget *hbox;
    GtkWidget *label;
    GtkWidget *txt_name;
    GtkWidget *dialog;
    printf("%s()\n", __PRETTY_FUNCTION__);

    dialog = gtk_dialog_new_with_buttons(_("New Name"),
            GTK_WINDOW(rmi->dialog), GTK_DIALOG_MODAL,
            GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
            GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
            NULL);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
            hbox = gtk_hbox_new(FALSE, 4), FALSE, FALSE, 4);

    gtk_box_pack_start(GTK_BOX(hbox),
            label = gtk_label_new(_("Name")),
            FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox),
            txt_name = gtk_entry_new(),
            TRUE, TRUE, 0);

    gtk_widget_show_all(dialog);

    while(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(dialog)))
    {
        gint active = gtk_combo_box_get_active(GTK_COMBO_BOX(rmi->cmb_repos));
        RepoEditInfo *rei = g_list_nth_data(rmi->repo_edits, active);
        g_free(rei->name);
        rei->name = g_strdup(gtk_entry_get_text(GTK_ENTRY(txt_name)));
        gtk_combo_box_insert_text(GTK_COMBO_BOX(rmi->cmb_repos),
                active, g_strdup(rei->name));
        gtk_combo_box_set_active(GTK_COMBO_BOX(rmi->cmb_repos), active);
        gtk_combo_box_remove_text(GTK_COMBO_BOX(rmi->cmb_repos), active + 1);
        break;
    }

    gtk_widget_destroy(dialog);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
repoman_dialog_delete(GtkWidget *widget, RepoManInfo *rmi)
{
    gchar buffer[100];
    GtkWidget *confirm;
    gint active = gtk_combo_box_get_active(GTK_COMBO_BOX(rmi->cmb_repos));
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(gtk_tree_model_iter_n_children(GTK_TREE_MODEL(
                    gtk_combo_box_get_model(GTK_COMBO_BOX(rmi->cmb_repos))),
                                NULL) <= 1)
    {
        popup_error(rmi->dialog,
                _("Cannot delete the last repository - there must be at"
                " lease one repository."));
        vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
        return TRUE;
    }

    snprintf(buffer, sizeof(buffer), "%s:\n%s\n",
            _("Confirm delete of repository"),
            gtk_combo_box_get_active_text(GTK_COMBO_BOX(rmi->cmb_repos)));
    confirm = hildon_note_new_confirmation(GTK_WINDOW(_window), buffer);

    if(GTK_RESPONSE_OK == gtk_dialog_run(GTK_DIALOG(confirm)))
    {
        gtk_combo_box_remove_text(GTK_COMBO_BOX(rmi->cmb_repos), active);
        gtk_notebook_remove_page(GTK_NOTEBOOK(rmi->notebook), active);
        rmi->repo_edits = g_list_remove_link(
                rmi->repo_edits,
                g_list_nth(rmi->repo_edits, active));
        gtk_combo_box_set_active(GTK_COMBO_BOX(rmi->cmb_repos),
                MAX(0, active - 1));
    }

    gtk_widget_destroy(confirm);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static RepoEditInfo*
repoman_dialog_add_repo(RepoManInfo *rmi, gchar *name)
{
    GtkWidget *label;
    GtkWidget *hbox;
    RepoEditInfo *rei = g_new(RepoEditInfo, 1);
    printf("%s(%s)\n", __PRETTY_FUNCTION__, name);

    rei->name = name;

    /* Maps page. */
    gtk_notebook_append_page(GTK_NOTEBOOK(rmi->notebook),
            rei->page_table = gtk_table_new(4, 4, FALSE),
            gtk_label_new(name));

    /* Map download URI. */
    gtk_table_attach(GTK_TABLE(rei->page_table),
            label = gtk_label_new(_("URI Format")),
            0, 1, 0, 1, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(rei->page_table),
            rei->txt_url = gtk_entry_new(),
            1, 3, 0, 1, GTK_EXPAND | GTK_FILL, 0, 2, 4);

    /* Map Directory. */
    gtk_table_attach(GTK_TABLE(rei->page_table),
            label = gtk_label_new(_("Cache Dir.")),
            0, 1, 1, 2, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(rei->page_table),
            hbox = gtk_hbox_new(FALSE, 4),
            1, 3, 1, 2, GTK_EXPAND | GTK_FILL, 0, 2, 4);
    gtk_box_pack_start(GTK_BOX(hbox),
            rei->txt_cache_dir = gtk_entry_new(),
            TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox),
            rei->btn_browse = gtk_button_new_with_label(_("Browse...")),
            FALSE, FALSE, 0);

    /* Download Zoom Steps. */
    gtk_table_attach(GTK_TABLE(rei->page_table),
            label = gtk_label_new(_("Download Zoom Steps")),
            0, 2, 2, 3, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(rei->page_table),
            label = gtk_alignment_new(0.f, 0.5f, 0.f, 0.f),
            2, 3, 2, 3, GTK_EXPAND | GTK_FILL, 0, 2, 4);
    gtk_container_add(GTK_CONTAINER(label),
            rei->num_dl_zoom_steps = hildon_controlbar_new());
    hildon_controlbar_set_range(
            HILDON_CONTROLBAR(rei->num_dl_zoom_steps), 1, 4);
    hildon_controlbar_set_value(
            HILDON_CONTROLBAR(rei->num_dl_zoom_steps), 2);
    force_min_visible_bars(HILDON_CONTROLBAR(rei->num_dl_zoom_steps), 1);

    /* Download Zoom Steps. */
    gtk_table_attach(GTK_TABLE(rei->page_table),
            label = gtk_label_new(_("View Zoom Steps")),
            0, 2, 3, 4, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(rei->page_table),
            label = gtk_alignment_new(0.f, 0.5f, 0.f, 0.f),
            2, 3, 3, 4, GTK_EXPAND | GTK_FILL, 0, 2, 4);
    gtk_container_add(GTK_CONTAINER(label),
            rei->num_view_zoom_steps = hildon_controlbar_new());
    hildon_controlbar_set_range(
            HILDON_CONTROLBAR(rei->num_view_zoom_steps), 1, 4);
    hildon_controlbar_set_value(
            HILDON_CONTROLBAR(rei->num_view_zoom_steps), 1);
    force_min_visible_bars(HILDON_CONTROLBAR(rei->num_view_zoom_steps), 1);

    rmi->repo_edits = g_list_append(rmi->repo_edits, rei);

    /* Connect signals. */
    rei->browse_info.dialog = rmi->dialog;
    rei->browse_info.txt = rei->txt_cache_dir;
    g_signal_connect(G_OBJECT(rei->btn_browse), "clicked",
                      G_CALLBACK(repoman_dialog_browse),
                      &rei->browse_info);

    gtk_widget_show_all(rei->page_table);

    gtk_combo_box_append_text(GTK_COMBO_BOX(rmi->cmb_repos), name);
    gtk_combo_box_set_active(GTK_COMBO_BOX(rmi->cmb_repos),
            gtk_tree_model_iter_n_children(GTK_TREE_MODEL(
                    gtk_combo_box_get_model(GTK_COMBO_BOX(rmi->cmb_repos))),
                NULL) - 1);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return rei;
}

static gboolean
repoman_dialog_new(GtkWidget *widget, RepoManInfo *rmi)
{
    GtkWidget *hbox;
    GtkWidget *label;
    GtkWidget *txt_name;
    GtkWidget *dialog;
    printf("%s()\n", __PRETTY_FUNCTION__);

    dialog = gtk_dialog_new_with_buttons(_("New Repository"),
            GTK_WINDOW(rmi->dialog), GTK_DIALOG_MODAL,
            GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
            GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
            NULL);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
            hbox = gtk_hbox_new(FALSE, 4), FALSE, FALSE, 4);

    gtk_box_pack_start(GTK_BOX(hbox),
            label = gtk_label_new(_("Name")),
            FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox),
            txt_name = gtk_entry_new(),
            TRUE, TRUE, 0);

    gtk_widget_show_all(dialog);

    while(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(dialog)))
    {
        repoman_dialog_add_repo(rmi,
                g_strdup(gtk_entry_get_text(GTK_ENTRY(txt_name))));
        break;
    }

    gtk_widget_destroy(dialog);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_maps_repoman(GtkAction *action)
{
    RepoManInfo rmi;
    GtkWidget *hbox;
    GtkWidget *btn_rename;
    GtkWidget *btn_delete;
    GtkWidget *btn_new;
    guint i, curr_repo_index = 0;
    GList *curr;
    printf("%s()\n", __PRETTY_FUNCTION__);

    rmi.dialog = gtk_dialog_new_with_buttons(_("Repositories"),
            GTK_WINDOW(_window), GTK_DIALOG_MODAL,
            GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
            GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
            NULL);

    hbox = gtk_hbox_new(FALSE, 4);

    gtk_box_pack_start(GTK_BOX(hbox),
            rmi.cmb_repos = gtk_combo_box_new_text(), FALSE, FALSE, 4);

    gtk_box_pack_start(GTK_BOX(hbox),
            gtk_vseparator_new(), TRUE, TRUE, 4);
    gtk_box_pack_start(GTK_BOX(hbox),
            btn_rename = gtk_button_new_with_label(_("Rename...")),
            FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(hbox),
            btn_delete = gtk_button_new_with_label(_("Delete...")),
            FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(hbox),
            btn_new = gtk_button_new_with_label(_("New...")),
            FALSE, FALSE, 4);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(rmi.dialog)->vbox),
            hbox, FALSE, FALSE, 4);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(rmi.dialog)->vbox),
            gtk_hseparator_new(), TRUE, TRUE, 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(rmi.dialog)->vbox),
            rmi.notebook = gtk_notebook_new(), TRUE, TRUE, 4);

    gtk_notebook_set_show_tabs(GTK_NOTEBOOK(rmi.notebook), FALSE);
    gtk_notebook_set_show_border(GTK_NOTEBOOK(rmi.notebook), FALSE);

    rmi.repo_edits = NULL;

    /* Populate combo box and pages in notebook. */
    for(i = 0, curr = _repo_list; curr; curr = curr->next, i++)
    {
        RepoData *rd = (RepoData*)curr->data;
        RepoEditInfo *rei = repoman_dialog_add_repo(&rmi, g_strdup(rd->name));

        /* Initialize fields with data from the RepoData object. */
        gtk_entry_set_text(GTK_ENTRY(rei->txt_url), rd->url);
        gtk_entry_set_text(GTK_ENTRY(rei->txt_cache_dir),
                rd->cache_dir);
        hildon_controlbar_set_value(
                HILDON_CONTROLBAR(rei->num_dl_zoom_steps),
                rd->dl_zoom_steps);
        hildon_controlbar_set_value(
                HILDON_CONTROLBAR(rei->num_view_zoom_steps),
                rd->view_zoom_steps);
        if(rd == _curr_repo)
            curr_repo_index = i;
    }

    /* Connect signals. */
    g_signal_connect(G_OBJECT(btn_rename), "clicked",
            G_CALLBACK(repoman_dialog_rename), &rmi);
    g_signal_connect(G_OBJECT(btn_delete), "clicked",
            G_CALLBACK(repoman_dialog_delete), &rmi);
    g_signal_connect(G_OBJECT(btn_new), "clicked",
            G_CALLBACK(repoman_dialog_new), &rmi);
    g_signal_connect(G_OBJECT(rmi.cmb_repos), "changed",
            G_CALLBACK(repoman_dialog_select), &rmi);
    gtk_combo_box_set_active(GTK_COMBO_BOX(rmi.cmb_repos), curr_repo_index);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(rmi.notebook), curr_repo_index);

    gtk_widget_show_all(rmi.dialog);

    while(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(rmi.dialog)))
    {
        /* Iterate through repos and verify each. */
        gboolean verified = TRUE;
        gint i;
        GList *curr;
        gchar *old_curr_repo_name = NULL;
        for(i = 0, curr = rmi.repo_edits;
                verified && curr; curr = curr->next, i++)
        {
            RepoEditInfo *rei = curr->data;
            /* Verify that cache directory exists or can be created. */
            GnomeVFSURI *map_dir_uri = gnome_vfs_uri_new(gtk_entry_get_text(
                        GTK_ENTRY(rei->txt_cache_dir)));
            if(!gnome_vfs_uri_exists(map_dir_uri))
            {
                GnomeVFSURI *parent, *curr_uri;
                GList *list = NULL;

                list = g_list_prepend(list, curr_uri = map_dir_uri);
                while(GNOME_VFS_ERROR_NOT_FOUND
                        == gnome_vfs_make_directory_for_uri(
                            parent = gnome_vfs_uri_get_parent(curr_uri), 0755))
                    list = g_list_prepend(list, curr_uri = parent);

                while(list != NULL)
                {
                    if(verified)
                    {
                        verified = (GNOME_VFS_OK
                                == gnome_vfs_make_directory_for_uri(
                                    (GnomeVFSURI*)list->data, 0755));
                    }
                    gnome_vfs_uri_unref((GnomeVFSURI*)list->data);
                    list = g_list_remove(list, list->data);
                }
                /* verified now equals result of last make-dir attempt. */
            }

            if(!verified)
            {
                /* Failed to create Map Cache directory. */
                gchar buffer[BUFFER_SIZE];
                snprintf(buffer, sizeof(buffer), "%s: %s",
                        _("Unable to create cache directory for repository"),
                        rei->name);
                gtk_combo_box_set_active(GTK_COMBO_BOX(rmi.cmb_repos), i);
                popup_error(rmi.dialog, buffer);
            }
        }
        if(!verified)
            continue;

        /* We're good to replace.  Remove old _repo_list menu items. */
        menu_maps_remove_repos();
        /* But keep the repo list in memory, in case downloads are using it. */
        _repo_list = NULL;

        /* Write new _repo_list. */
        _curr_repo = NULL;
        for(i = 0, curr = rmi.repo_edits; curr; curr = curr->next, i++)
        {
            RepoEditInfo *rei = curr->data;
            RepoData *rd = g_new(RepoData, 1);
            rd->name = g_strdup(rei->name);
            rd->url = g_strdup(gtk_entry_get_text(GTK_ENTRY(rei->txt_url)));
            rd->cache_dir = g_strdup(gtk_entry_get_text(
                    GTK_ENTRY(rei->txt_cache_dir)));
            rd->dl_zoom_steps = hildon_controlbar_get_value(
                    HILDON_CONTROLBAR(rei->num_dl_zoom_steps));
            rd->view_zoom_steps = hildon_controlbar_get_value(
                    HILDON_CONTROLBAR(rei->num_view_zoom_steps));
            _repo_list = g_list_append(_repo_list, rd);

            if(old_curr_repo_name && !strcmp(old_curr_repo_name, rd->name))
                _curr_repo = rd;
        }
        if(!_curr_repo)
            _curr_repo = (RepoData*)g_list_first(_repo_list)->data;
        menu_maps_add_repos();
        break;
    }

    gtk_widget_hide(rmi.dialog); /* Destroying causes a crash (!?!?!??!) */

    map_set_zoom(_zoom); /* make sure we're at an appropriate zoom level. */

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

typedef struct _MapmanInfo MapmanInfo;
struct _MapmanInfo {
    GtkWidget *dialog;
    GtkWidget *notebook;
    GtkWidget *tbl_area;

    /* The "Setup" tab. */
    GtkWidget *rad_download;
    GtkWidget *rad_delete;
    GtkWidget *chk_overwrite;
    GtkWidget *rad_by_area;
    GtkWidget *rad_by_route;
    GtkWidget *num_route_radius;

    /* The "Area" tab. */
    GtkWidget *txt_topleft_lat;
    GtkWidget *txt_topleft_lon;
    GtkWidget *txt_botright_lat;
    GtkWidget *txt_botright_lon;

    /* The "Zoom" tab. */
    GtkWidget *chk_zoom_levels[MAX_ZOOM];
};

static gboolean
mapman_by_area(gfloat start_lat, gfloat start_lon,
        gfloat end_lat, gfloat end_lon, MapmanInfo *mapman_info,
        gboolean is_deleting, gboolean is_overwriting)
{
    guint start_unitx, start_unity, end_unitx, end_unity;
    guint num_maps = 0;
    guint i;
    gchar buffer[80];
    GtkWidget *confirm;
    printf("%s()\n", __PRETTY_FUNCTION__);

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
                    GTK_TOGGLE_BUTTON(mapman_info->chk_zoom_levels[i])))
        {
            guint start_tilex, start_tiley, end_tilex, end_tiley;
            start_tilex = unit2ztile(start_unitx, i);
            start_tiley = unit2ztile(start_unity, i);
            end_tilex = unit2ztile(end_unitx, i);
            end_tiley = unit2ztile(end_unity, i);
            num_maps += (end_tilex - start_tilex + 1)
                * (end_tiley - start_tiley + 1);
        }
    }

    if(is_deleting)
    {
        snprintf(buffer, sizeof(buffer), "%s %d %s", _("Confirm DELETION of"),
                num_maps, _("maps"));
    }
    else
    {
        snprintf(buffer, sizeof(buffer),
                "%s %d %s\n(%s %.2f MB)\n", _("Confirm download of"),
                num_maps, _("maps"), _("up to about"),
                num_maps * (strstr(_curr_repo->url, "%s") ? 18e-3 : 6e-3));
    }
    confirm = hildon_note_new_confirmation(
            GTK_WINDOW(mapman_info->dialog), buffer);

    if(GTK_RESPONSE_OK != gtk_dialog_run(GTK_DIALOG(confirm)))
    {
        gtk_widget_destroy(confirm);
        vprintf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
        return FALSE;
    }
    for(i = 0; i < MAX_ZOOM; i++)
    {
        if(gtk_toggle_button_get_active(
                    GTK_TOGGLE_BUTTON(mapman_info->chk_zoom_levels[i])))
        {
            guint start_tilex, start_tiley, end_tilex, end_tiley;
            guint tilex, tiley;
            start_tilex = unit2ztile(start_unitx, i);
            start_tiley = unit2ztile(start_unity, i);
            end_tilex = unit2ztile(end_unitx, i);
            end_tiley = unit2ztile(end_unity, i);
            for(tiley = start_tiley; tiley <= end_tiley; tiley++)
                for(tilex = start_tilex; tilex <= end_tilex; tilex++)
                    map_initiate_download(tilex, tiley, i,
                            is_deleting ? 0 :
                                      (is_overwriting
                                        ? -INITIAL_DOWNLOAD_RETRIES
                                        : INITIAL_DOWNLOAD_RETRIES));
        }
    }
    gtk_widget_destroy(confirm);
    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
mapman_by_route(MapmanInfo *mapman_info,
        gboolean is_deleting, gboolean is_overwriting)
{
    GtkWidget *confirm;
    guint prev_tilex, prev_tiley, num_maps = 0, i;
    Point *curr;
    gchar buffer[80];
    guint radius = hildon_number_editor_get_value(
            HILDON_NUMBER_EDITOR(mapman_info->num_route_radius));
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(!_route.head)
    {
        popup_error(mapman_info->dialog, "No route is loaded.");
        return TRUE;
    }

    /* First, get the number of maps to download. */
    for(i = 0; i < MAX_ZOOM; i++)
    {
        if(gtk_toggle_button_get_active(
                    GTK_TOGGLE_BUTTON(mapman_info->chk_zoom_levels[i])))
        {
            prev_tilex = 0;
            prev_tiley = 0;
            for(curr = _route.head - 1; ++curr != _route.tail; )
            {
                if(curr->unity)
                {
                    guint tilex = unit2ztile(curr->unitx, i);
                    guint tiley = unit2ztile(curr->unity, i);
                    if(tilex != prev_tilex || tiley != prev_tiley)
                    {
                        if(prev_tiley)
                            num_maps += (abs((gint)tilex - prev_tilex) + 1)
                                * (abs((gint)tiley - prev_tiley) + 1) - 1;
                        prev_tilex = tilex;
                        prev_tiley = tiley;
                    }
                }
            }
        }
    }
    num_maps *= 0.625 * pow(radius + 1, 1.85);

    if(is_deleting)
    {
        snprintf(buffer, sizeof(buffer), "%s %s %d %s",
                _("Confirm DELETION of"), _("about"),
                num_maps, _("maps"));
    }
    else
    {
        snprintf(buffer, sizeof(buffer),
                "%s %s %d %s\n(%s %.2f MB)\n", _("Confirm download of"),
                _("about"),
                num_maps, _("maps"), _("up to about"),
                num_maps * (strstr(_curr_repo->url, "%s") ? 18e-3 : 6e-3));
    }
    confirm = hildon_note_new_confirmation(
            GTK_WINDOW(mapman_info->dialog), buffer);

    if(GTK_RESPONSE_OK != gtk_dialog_run(GTK_DIALOG(confirm)))
    {
        gtk_widget_destroy(confirm);
        vprintf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
        return FALSE;
    }

    /* Now, do the actual download. */
    for(i = 0; i < MAX_ZOOM; i++)
    {
        if(gtk_toggle_button_get_active(
                    GTK_TOGGLE_BUTTON(mapman_info->chk_zoom_levels[i])))
        {
            prev_tilex = 0;
            prev_tiley = 0;
            for(curr = _route.head - 1; ++curr != _route.tail; )
            {
                if(curr->unity)
                {
                    guint tilex = unit2ztile(curr->unitx, i);
                    guint tiley = unit2ztile(curr->unity, i);
                    if(tilex != prev_tilex || tiley != prev_tiley)
                    {
                        guint minx, miny, maxx, maxy, x, y;
                        if(prev_tiley != 0)
                        {
                            minx = MIN(tilex, prev_tilex) - radius;
                            miny = MIN(tiley, prev_tiley) - radius;
                            maxx = MAX(tilex, prev_tilex) + radius;
                            maxy = MAX(tiley, prev_tiley) + radius;
                        }
                        else
                        {
                            minx = tilex - radius;
                            miny = tiley - radius;
                            maxx = tilex + radius;
                            maxy = tiley + radius;
                        }
                        for(x = minx; x <= maxx; x++)
                            for(y = miny; y <= maxy; y++)
                                map_initiate_download(x, y, i,
                                        is_deleting ? 0 :
                                              (is_overwriting
                                                ? -INITIAL_DOWNLOAD_RETRIES
                                                : INITIAL_DOWNLOAD_RETRIES));
                        prev_tilex = tilex;
                        prev_tiley = tiley;
                    }
                }
            }
        }
    }
    _route_dl_radius = radius;
    gtk_widget_destroy(confirm);
    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static void mapman_clear(GtkWidget *widget, MapmanInfo *mapman_info)
{
    guint i;
    printf("%s()\n", __PRETTY_FUNCTION__);
    if(gtk_notebook_get_current_page(GTK_NOTEBOOK(mapman_info->notebook)))
        /* This is the second page (the "Zoom" page) - clear the checks. */
        for(i = 0; i < MAX_ZOOM; i++)
            gtk_toggle_button_set_active(
                    GTK_TOGGLE_BUTTON(mapman_info->chk_zoom_levels[i]), FALSE);
    else
    {
        /* This is the first page (the "Area" page) - clear the text fields. */
        gtk_entry_set_text(GTK_ENTRY(mapman_info->txt_topleft_lat), "");
        gtk_entry_set_text(GTK_ENTRY(mapman_info->txt_topleft_lon), "");
        gtk_entry_set_text(GTK_ENTRY(mapman_info->txt_botright_lat), "");
        gtk_entry_set_text(GTK_ENTRY(mapman_info->txt_botright_lon), "");
    }
    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

static void mapman_update_state(GtkWidget *widget, MapmanInfo *mapman_info)
{
    printf("%s()\n", __PRETTY_FUNCTION__);
    gtk_widget_set_sensitive( mapman_info->chk_overwrite,
            gtk_toggle_button_get_active(
                GTK_TOGGLE_BUTTON(mapman_info->rad_download)));

    if(gtk_toggle_button_get_active(
                GTK_TOGGLE_BUTTON(mapman_info->rad_by_area)))
        gtk_widget_show(mapman_info->tbl_area);
    else if(gtk_notebook_get_n_pages(GTK_NOTEBOOK(mapman_info->notebook)) == 3)
        gtk_widget_hide(mapman_info->tbl_area);

    gtk_widget_set_sensitive(mapman_info->num_route_radius,
            gtk_toggle_button_get_active(
                GTK_TOGGLE_BUTTON(mapman_info->rad_by_route)));
    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

static gboolean
menu_cb_maps_select(GtkAction *action, gpointer new_repo)
{
    printf("%s()\n", __PRETTY_FUNCTION__);
    _curr_repo = new_repo;
    map_force_redraw();
    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_mapman(GtkAction *action)
{
    GtkWidget *dialog;
    GtkWidget *vbox;
    GtkWidget *hbox;
    GtkWidget *table;
    GtkWidget *label;
    GtkWidget *button;
    GtkWidget *lbl_gps_lat;
    GtkWidget *lbl_gps_lon;
    GtkWidget *lbl_center_lat;
    GtkWidget *lbl_center_lon;
    MapmanInfo mapman_info;
    gchar buffer[80];
    gfloat lat, lon;
    guint i;
    printf("%s()\n", __PRETTY_FUNCTION__);

    mapman_info.dialog = dialog = gtk_dialog_new_with_buttons(_("Manage Maps"),
            GTK_WINDOW(_window), GTK_DIALOG_MODAL,
            GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
            NULL);

    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area),
            button = gtk_button_new_with_label(_("Clear")));
    g_signal_connect(G_OBJECT(button), "clicked",
                      G_CALLBACK(mapman_clear), &mapman_info);

    /* Clear button. */
    gtk_dialog_add_button(GTK_DIALOG(dialog),
            GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
            mapman_info.notebook = gtk_notebook_new(), TRUE, TRUE, 0);

    /* Setup page. */
    gtk_notebook_append_page(GTK_NOTEBOOK(mapman_info.notebook),
            vbox = gtk_vbox_new(FALSE, 2),
            label = gtk_label_new(_("Setup")));
    gtk_notebook_set_tab_label_packing(
            GTK_NOTEBOOK(mapman_info.notebook), vbox,
            FALSE, FALSE, GTK_PACK_START);

    gtk_box_pack_start(GTK_BOX(vbox),
            hbox = gtk_hbox_new(FALSE, 4),
            FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox),
            mapman_info.rad_download
                    = gtk_radio_button_new_with_label(NULL,_("Download Maps")),
            FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox),
            label = gtk_alignment_new(0.f, 0.5f, 0.f, 0.f),
            FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(label),
            mapman_info.chk_overwrite
                    = gtk_check_button_new_with_label(_("Overwrite"))),

    gtk_box_pack_start(GTK_BOX(vbox),
            mapman_info.rad_delete
                    = gtk_radio_button_new_with_label_from_widget(
                        GTK_RADIO_BUTTON(mapman_info.rad_download),
                        _("Delete Maps")),
            FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vbox),
            gtk_hseparator_new(),
            FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vbox),
            mapman_info.rad_by_area
                    = gtk_radio_button_new_with_label(NULL,
                        _("By Area (see tab)")),
            FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox),
            hbox = gtk_hbox_new(FALSE, 4),
            FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox),
            mapman_info.rad_by_route
                    = gtk_radio_button_new_with_label_from_widget(
                        GTK_RADIO_BUTTON(mapman_info.rad_by_area),
                        _("Along Route - Radius (tiles):")),
            FALSE, FALSE, 0);
    gtk_widget_set_sensitive(mapman_info.rad_by_route, _route.head != NULL);
    gtk_box_pack_start(GTK_BOX(hbox),
            mapman_info.num_route_radius = hildon_number_editor_new(0, 100),
            FALSE, FALSE, 0);
    hildon_number_editor_set_value(
            HILDON_NUMBER_EDITOR(mapman_info.num_route_radius),
            _route_dl_radius);


    /* Zoom page. */
    gtk_notebook_append_page(GTK_NOTEBOOK(mapman_info.notebook),
            table = gtk_table_new(5, 5, FALSE),
            label = gtk_label_new(_("Zoom")));
    gtk_notebook_set_tab_label_packing(
            GTK_NOTEBOOK(mapman_info.notebook), table,
            FALSE, FALSE, GTK_PACK_START);
    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new(
                _("Zoom Levels to Download: (0 -> most detail)")),
            0, 4, 0, 1, GTK_FILL, 0, 4, 0);
    gtk_misc_set_alignment(GTK_MISC(label), 0.f, 0.5f);
    for(i = 0; i < MAX_ZOOM; i++)
    {
        snprintf(buffer, sizeof(buffer), "%d", i);
        gtk_table_attach(GTK_TABLE(table),
                mapman_info.chk_zoom_levels[i]
                        = gtk_check_button_new_with_label(buffer),
                i % 4, i % 4 + 1, i / 4 + 1, i / 4 + 2,
                GTK_EXPAND | GTK_FILL, 0, 4, 0);
    }

    /* Area page. */
    gtk_notebook_append_page(GTK_NOTEBOOK(mapman_info.notebook),
        mapman_info.tbl_area = gtk_table_new(3, 4, FALSE),
        label = gtk_label_new(_("Area")));

    /* Label Columns. */
    gtk_table_attach(GTK_TABLE(mapman_info.tbl_area),
            label = gtk_label_new(_("Latitude")),
            1, 2, 0, 1, GTK_FILL, 0, 4, 0);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(mapman_info.tbl_area),
            label = gtk_label_new(_("Longitude")),
            2, 3, 0, 1, GTK_FILL, 0, 4, 0);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);

    /* GPS. */
    gtk_table_attach(GTK_TABLE(mapman_info.tbl_area),
            label = gtk_label_new(_("GPS Location")),
            0, 1, 1, 2, GTK_FILL, 0, 4, 0);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(mapman_info.tbl_area),
            lbl_gps_lat = gtk_label_new(""),
            1, 2, 1, 2, GTK_FILL, 0, 4, 0);
    gtk_label_set_selectable(GTK_LABEL(lbl_gps_lat), TRUE);
    gtk_misc_set_alignment(GTK_MISC(lbl_gps_lat), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(mapman_info.tbl_area),
            lbl_gps_lon = gtk_label_new(""),
            2, 3, 1, 2, GTK_FILL, 0, 4, 0);
    gtk_label_set_selectable(GTK_LABEL(lbl_gps_lon), TRUE);
    gtk_misc_set_alignment(GTK_MISC(lbl_gps_lon), 1.f, 0.5f);

    /* Center. */
    gtk_table_attach(GTK_TABLE(mapman_info.tbl_area),
            label = gtk_label_new(_("View Center")),
            0, 1, 2, 3, GTK_FILL, 0, 4, 0);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(mapman_info.tbl_area),
            lbl_center_lat = gtk_label_new(""),
            1, 2, 2, 3, GTK_FILL, 0, 4, 0);
    gtk_label_set_selectable(GTK_LABEL(lbl_center_lat), TRUE);
    gtk_misc_set_alignment(GTK_MISC(lbl_center_lat), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(mapman_info.tbl_area),
            lbl_center_lon = gtk_label_new(""),
            2, 3, 2, 3, GTK_FILL, 0, 4, 0);
    gtk_label_set_selectable(GTK_LABEL(lbl_center_lon), TRUE);
    gtk_misc_set_alignment(GTK_MISC(lbl_center_lon), 1.f, 0.5f);

    /* default values for Top Left and Bottom Right are defined by the
     * rectangle of the current and the previous Center */

    /* Top Left. */
    gtk_table_attach(GTK_TABLE(mapman_info.tbl_area),
            label = gtk_label_new(_("Top-Left")),
            0, 1, 3, 4, GTK_FILL, 0, 4, 0);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(mapman_info.tbl_area),
            mapman_info.txt_topleft_lat = gtk_entry_new(),
            1, 2, 3, 4, GTK_EXPAND | GTK_FILL, 0, 4, 0);
    gtk_entry_set_alignment(GTK_ENTRY(mapman_info.txt_topleft_lat), 1.f);
    gtk_table_attach(GTK_TABLE(mapman_info.tbl_area),
            mapman_info.txt_topleft_lon = gtk_entry_new(),
            2, 3, 3, 4, GTK_EXPAND | GTK_FILL, 0, 4, 0);
    gtk_entry_set_alignment(GTK_ENTRY(mapman_info.txt_topleft_lon), 1.f);

    /* Bottom Right. */
    gtk_table_attach(GTK_TABLE(mapman_info.tbl_area),
            label = gtk_label_new(_("Bottom-Right")),
            0, 1, 4, 5, GTK_FILL, 0, 4, 0);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(mapman_info.tbl_area),
            mapman_info.txt_botright_lat = gtk_entry_new(),
            1, 2, 4, 5, GTK_EXPAND | GTK_FILL, 0, 4, 0);
    gtk_entry_set_alignment(GTK_ENTRY(mapman_info.txt_botright_lat), 1.f);
    gtk_table_attach(GTK_TABLE(mapman_info.tbl_area),
            mapman_info.txt_botright_lon = gtk_entry_new(),
            2, 3, 4, 5, GTK_EXPAND | GTK_FILL, 0, 4, 0);
    gtk_entry_set_alignment(GTK_ENTRY(mapman_info.txt_botright_lon), 1.f);

    /* Default action is to download by area. */
    gtk_toggle_button_set_active(
            GTK_TOGGLE_BUTTON(mapman_info.rad_by_area), TRUE);

    /* Initialize fields.  Do no use g_ascii_formatd; these strings will be
     * output (and parsed) as locale-dependent. */

    snprintf(buffer, sizeof(buffer), "%.06f", _gps.latitude);
    gtk_label_set_text(GTK_LABEL(lbl_gps_lat), buffer);
    snprintf(buffer, sizeof(buffer), "%.06f", _gps.longitude);
    gtk_label_set_text(GTK_LABEL(lbl_gps_lon), buffer);

    unit2latlon(_center.unitx, _center.unity, lat, lon);
    snprintf(buffer, sizeof(buffer), "%.06f", lat);
    gtk_label_set_text(GTK_LABEL(lbl_center_lat), buffer);
    snprintf(buffer, sizeof(buffer), "%.06f", lon);
    gtk_label_set_text(GTK_LABEL(lbl_center_lon), buffer);

    /* Initialize to the bounds of the screen. */
    unit2latlon(x2unit(0), y2unit(0), lat, lon);
    snprintf(buffer, sizeof(buffer), "%.06f", lat);
    gtk_entry_set_text(GTK_ENTRY(mapman_info.txt_topleft_lat), buffer);
    snprintf(buffer, sizeof(buffer), "%.06f", lon);
    gtk_entry_set_text(GTK_ENTRY(mapman_info.txt_topleft_lon), buffer);

    unit2latlon(x2unit(_screen_width_pixels), y2unit(_screen_height_pixels),
            lat, lon);
    snprintf(buffer, sizeof(buffer), "%.06f", lat);
    gtk_entry_set_text(GTK_ENTRY(mapman_info.txt_botright_lat), buffer);
    snprintf(buffer, sizeof(buffer), "%.06f", lon);
    gtk_entry_set_text(GTK_ENTRY(mapman_info.txt_botright_lon), buffer);

    gtk_toggle_button_set_active(
            GTK_TOGGLE_BUTTON(mapman_info.chk_zoom_levels[_zoom]), TRUE);

    gtk_widget_show_all(dialog);

    mapman_update_state(NULL, &mapman_info);

    /* Connect signals. */
    if(*_curr_repo->url)
    {
        g_signal_connect(G_OBJECT(mapman_info.rad_download), "clicked",
                          G_CALLBACK(mapman_update_state), &mapman_info);
        gtk_widget_set_sensitive(mapman_info.rad_download, TRUE);
    }
    else
    {
        gtk_widget_set_sensitive(mapman_info.rad_download, FALSE);
        popup_error(dialog,
                _("NOTE: You must set a Map URI in the Repository Manager in "
                    "order to download maps."));
    }
    g_signal_connect(G_OBJECT(mapman_info.rad_delete), "clicked",
                      G_CALLBACK(mapman_update_state), &mapman_info);
    g_signal_connect(G_OBJECT(mapman_info.rad_by_area), "clicked",
                      G_CALLBACK(mapman_update_state), &mapman_info);
    g_signal_connect(G_OBJECT(mapman_info.rad_by_route), "clicked",
                      G_CALLBACK(mapman_update_state), &mapman_info);

    while(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(dialog)))
    {
        gboolean is_deleting = gtk_toggle_button_get_active(
                GTK_TOGGLE_BUTTON(mapman_info.rad_delete));
        gboolean is_overwriting = gtk_toggle_button_get_active(
                GTK_TOGGLE_BUTTON(mapman_info.chk_overwrite));
        if(gtk_toggle_button_get_active(
                    GTK_TOGGLE_BUTTON(mapman_info.rad_by_route)))
        {
            if(mapman_by_route(&mapman_info, is_deleting, is_overwriting))
                break;
        }
        else
        {
            const gchar *text;
            gchar *error_check;
            gfloat start_lat, start_lon, end_lat, end_lon;

            text = gtk_entry_get_text(GTK_ENTRY(mapman_info.txt_topleft_lat));
            start_lat = strtof(text, &error_check);
            if(text == error_check) {
                popup_error(dialog, _("Invalid Top-Left Latitude"));
                continue;
            }

            text = gtk_entry_get_text(GTK_ENTRY(mapman_info.txt_topleft_lon));
            start_lon = strtof(text, &error_check);
            if(text == error_check) {
                popup_error(dialog, _("Invalid Top-Left Longitude"));
                continue;
            }

            text = gtk_entry_get_text(GTK_ENTRY(mapman_info.txt_botright_lat));
            end_lat = strtof(text, &error_check);
            if(text == error_check) {
                popup_error(dialog, _("Invalid Bottom-Right Latitude"));
                continue;
            }

            text = gtk_entry_get_text(GTK_ENTRY(mapman_info.txt_botright_lon));
            end_lon = strtof(text, &error_check);
            if(text == error_check) {
                popup_error(dialog,_("Invalid Bottom-Right Longitude"));
                continue;
            }

            if(mapman_by_area(start_lat, start_lon, end_lat, end_lon,
                        &mapman_info, is_deleting, is_overwriting))
                break;
        }
    }

    gtk_widget_hide(dialog); /* Destroying causes a crash (!?!?!??!) */

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
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

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
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
            popup_error(_window,
                    _("Cannot enable GPS until a GPS Receiver MAC "
                    "is set in the Settings dialog box."));
            gtk_check_menu_item_set_active(
                    GTK_CHECK_MENU_ITEM(_menu_enable_gps_item), FALSE);
        }
    }
    else
    {
        if(_conn_state > RCVR_OFF)
            set_conn_state(RCVR_OFF);
        rcvr_disconnect();
        track_add(0, FALSE);
        _speed_excess = FALSE;
    }
    map_move_mark();
    gps_show_info();
    gtk_widget_set_sensitive(GTK_WIDGET(_menu_gps_details_item), _enable_gps);
    gtk_widget_set_sensitive(GTK_WIDGET(_menu_gps_reset_item), _enable_gps);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_auto_download(GtkAction *action)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if((_auto_download = gtk_check_menu_item_get_active(
            GTK_CHECK_MENU_ITEM(_menu_auto_download_item))))
    {
        if(!*_curr_repo->url)
            popup_error(_window,
                _("NOTE: You must set a Map URI in the Repository Manager in "
                    "order to download maps."));
        map_force_redraw();
    }

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_gps_reset(GtkAction *action)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    reset_bluetooth();

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_gps_details(GtkAction *action)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    gps_details();

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
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

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_help(GtkAction *action)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    ossohelp_show(_osso, HELP_ID_INTRO, OSSO_HELP_SHOW_DIALOG);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
cmenu_cb_loc_show_latlon(GtkAction *action)
{
    gchar buffer[80], tmp1[15], tmp2[15];
    guint unitx, unity;
    gfloat lat, lon;
    printf("%s()\n", __PRETTY_FUNCTION__);

    unitx = x2unit(_cmenu_position_x);
    unity = y2unit(_cmenu_position_y);
    unit2latlon(unitx, unity, lat, lon);
    deg_format(lat, tmp1);
    deg_format(lon, tmp2);

    snprintf(buffer, sizeof(buffer),
            "%s: %s\n"
            "%s: %s",
            _("Latitude"), tmp1,
            _("Longitude"), tmp2);

    MACRO_BANNER_SHOW_INFO(_window, buffer);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
cmenu_cb_loc_clip_latlon(GtkAction *action)
{
    gchar buffer[80], tmp1[15], tmp2[15];
    guint unitx, unity;
    gfloat lat, lon;
    printf("%s()\n", __PRETTY_FUNCTION__);

    unitx = x2unit(_cmenu_position_x);
    unity = y2unit(_cmenu_position_y);
    unit2latlon(unitx, unity, lat, lon);
    deg_format(lat, tmp1);
    deg_format(lon, tmp2);

    snprintf(buffer, sizeof(buffer), "%s, %s", tmp1, tmp2);

    gtk_clipboard_set_text(
            gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), buffer, -1);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
cmenu_cb_loc_route_to(GtkAction *action)
{
    gchar buffer[80];
    gchar strlat[32];
    gchar strlon[32];
    guint unitx, unity;
    gfloat lat, lon;
    printf("%s()\n", __PRETTY_FUNCTION__);

    unitx = x2unit(_cmenu_position_x);
    unity = y2unit(_cmenu_position_y);
    unit2latlon(unitx, unity, lat, lon);

    g_ascii_formatd(strlat, 32, "%.06f", lat);
    g_ascii_formatd(strlon, 32, "%.06f", lon);
    snprintf(buffer, sizeof(buffer), "%s, %s", strlat, strlon);

    route_download(NULL, buffer, TRUE);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
cmenu_cb_loc_distance_to(GtkAction *action)
{
    gchar buffer[80];
    guint unitx, unity;
    gfloat lat, lon;
    printf("%s()\n", __PRETTY_FUNCTION__);

    unitx = x2unit(_cmenu_position_x);
    unity = y2unit(_cmenu_position_y);
    unit2latlon(unitx, unity, lat, lon);

    snprintf(buffer, sizeof(buffer), "%s: %.02f %s", _("Distance to Location"),
            calculate_distance(_gps.latitude, _gps.longitude, lat, lon),
            UNITS_TEXT[_units]);
    MACRO_BANNER_SHOW_INFO(_window, buffer);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static WayPoint *
find_nearest_visible_waypoint(guint unitx, guint unity)
{
    WayPoint *wcurr;
    WayPoint *wnear;
    guint nearest_squared;
    Point pos = { unitx, unity };
    printf("%s()\n", __PRETTY_FUNCTION__);

    wcurr = wnear = _visible_way_first;
    if(wcurr && wcurr != _visible_way_last)
    {
        nearest_squared = DISTANCE_SQUARED(pos, *(wcurr->point));

        while(wcurr++ != _visible_way_last)
        {
            guint test_squared = DISTANCE_SQUARED(pos, *(wcurr->point));
            if(test_squared < nearest_squared)
            {
                wnear = wcurr;
                nearest_squared = test_squared;
            }
        }
    }
    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return wnear;
}

static gboolean
cmenu_cb_way_show_latlon(GtkAction *action)
{
    gchar buffer[80];
    gfloat lat, lon;
    WayPoint *way;
    printf("%s()\n", __PRETTY_FUNCTION__);

    way = find_nearest_visible_waypoint(
            x2unit(_cmenu_position_x),
            y2unit(_cmenu_position_y));

    if(way)
    {
        unit2latlon(way->point->unitx, way->point->unity, lat, lon);

        snprintf(buffer, sizeof(buffer),
                "%s: %.06f\n"
                "%s: %.06f",
                _("Latitude"), lat,
                _("Longitude"), lon);

        MACRO_BANNER_SHOW_INFO(_window, buffer);
    }
    else
    {
        MACRO_BANNER_SHOW_INFO(_window, _("No waypoints are visible."));
    }

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
cmenu_cb_way_show_desc(GtkAction *action)
{
    WayPoint *way;
    printf("%s()\n", __PRETTY_FUNCTION__);

    way = find_nearest_visible_waypoint(
            x2unit(_cmenu_position_x),
            y2unit(_cmenu_position_y));

    if(way)
    {
        MACRO_BANNER_SHOW_INFO(_window, way->desc);
    }
    else
    {
        MACRO_BANNER_SHOW_INFO(_window, _("No waypoints are visible."));
    }

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
cmenu_cb_way_clip_latlon(GtkAction *action)
{
    gchar buffer[80];
    gfloat lat, lon;
    WayPoint *way;
    printf("%s()\n", __PRETTY_FUNCTION__);

    way = find_nearest_visible_waypoint(
            x2unit(_cmenu_position_x),
            y2unit(_cmenu_position_y));

    if(way)
    {
        unit2latlon(way->point->unitx, way->point->unity, lat, lon);

        snprintf(buffer, sizeof(buffer), "%.06f, %.06f", lat, lon);

        gtk_clipboard_set_text(
                gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), buffer, -1);
    }
    else
    {
        MACRO_BANNER_SHOW_INFO(_window, _("No waypoints are visible."));
    }

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
cmenu_cb_way_clip_desc(GtkAction *action)
{
    WayPoint *way;
    printf("%s()\n", __PRETTY_FUNCTION__);

    way = find_nearest_visible_waypoint(
            x2unit(_cmenu_position_x),
            y2unit(_cmenu_position_y));

    if(way)
        gtk_clipboard_set_text(
                gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), way->desc, -1);
    else
    {
        MACRO_BANNER_SHOW_INFO(_window, _("No waypoints are visible."));
    }

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
cmenu_cb_way_route_to(GtkAction *action)
{
    WayPoint *way;
    printf("%s()\n", __PRETTY_FUNCTION__);

    way = find_nearest_visible_waypoint(
            x2unit(_cmenu_position_x),
            y2unit(_cmenu_position_y));

    if(way)
    {
        gchar buffer[80];
        gchar strlat[32];
        gchar strlon[32];
        gfloat lat, lon;
        printf("%s()\n", __PRETTY_FUNCTION__);

        unit2latlon(way->point->unitx, way->point->unity, lat, lon);
        g_ascii_formatd(strlat, 32, "%.06f", lat);
        g_ascii_formatd(strlon, 32, "%.06f", lon);
        snprintf(buffer, sizeof(buffer), "%s, %s", strlat, strlon);

        route_download(NULL, buffer, TRUE);
    }
    else
    {
        MACRO_BANNER_SHOW_INFO(_window, _("No waypoints are visible."));
    }

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
cmenu_cb_way_distance_to(GtkAction *action)
{
    WayPoint *way;
    printf("%s()\n", __PRETTY_FUNCTION__);

    way = find_nearest_visible_waypoint(
            x2unit(_cmenu_position_x),
            y2unit(_cmenu_position_y));

    if(way)
    {
        gchar buffer[80];
        gfloat lat, lon;
        printf("%s()\n", __PRETTY_FUNCTION__);

        unit2latlon(way->point->unitx, way->point->unity, lat, lon);

        snprintf(buffer, sizeof(buffer), "%s: %.02f %s",
                _("Distance to Waypoint"),
                calculate_distance(_gps.latitude, _gps.longitude, lat, lon),
                UNITS_TEXT[_units]);
        MACRO_BANNER_SHOW_INFO(_window, buffer);
    }
    else
    {
        MACRO_BANNER_SHOW_INFO(_window, _("No waypoints are visible."));
    }

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
cmenu_cb_way_delete(GtkAction *action)
{
    WayPoint *way;
    printf("%s()\n", __PRETTY_FUNCTION__);

    way = find_nearest_visible_waypoint(
            x2unit(_cmenu_position_x),
            y2unit(_cmenu_position_y));

    if(way)
    {
        gchar buffer[BUFFER_SIZE];
        GtkWidget *confirm;

        snprintf(buffer, sizeof(buffer), "%s:\n%s\n",
                _("Confirm delete of waypoint"), way->desc);
        confirm = hildon_note_new_confirmation(GTK_WINDOW(_window), buffer);

        if(GTK_RESPONSE_OK == gtk_dialog_run(GTK_DIALOG(confirm)))
        {
            while(way++ != _route.wtail)
                way[-1] = *way;
            _route.wtail--;
            map_force_redraw();
        }
        gtk_widget_destroy(confirm);
    }
    else
    {
        MACRO_BANNER_SHOW_INFO(_window, _("No waypoints are visible."));
    }

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
category_delete(GtkWidget *widget, DeletePOI *dpoi)
{
    GtkWidget *dialog;
    guint i;
    gchar *buffer;
    printf("%s()\n", __PRETTY_FUNCTION__);

    buffer = g_strdup_printf("%s\n\t%s\n%s",
            _("Delete category?"),
            dpoi->txt_label,
            _("WARNING: All POIs in that category will also be deleted!"));
    dialog = hildon_note_new_confirmation (GTK_WINDOW(_window), buffer);
    g_free(buffer);
    i = gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (GTK_WIDGET (dialog));

    if(i == GTK_RESPONSE_OK)
    {
        /* delete dpoi->poi_id */
        if(SQLITE_OK != sqlite_exec_printf(_db,
                    "delete from poi where cat_id = %d",
                    NULL, NULL, NULL,
                    dpoi->id))
        {
            MACRO_BANNER_SHOW_INFO(_window, _("Problem deleting POI"));
            return FALSE;
        }

        if(SQLITE_OK != sqlite_exec_printf(_db,
                    "delete from category where cat_id = %d",
                    NULL, NULL, NULL,
                    dpoi->id))
        {
            MACRO_BANNER_SHOW_INFO(_window, _("Problem deleting category"));
            return FALSE;
        }

        gtk_widget_hide_all(dpoi->dialog);
        map_force_redraw();
    }

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
category_dialog(guint cat_id)
{
    gchar **pszResult;
    gint nRow, nColumn;
    gchar *cat_label = NULL, *cat_desc = NULL;
    guint cat_enabled;
    GtkWidget *dialog;
    GtkWidget *table;
    GtkWidget *label;
    GtkWidget *txt_label;
    GtkWidget *txt_desc;
    GtkWidget *btn_delete = NULL;
    GtkWidget *txt_scroll;
    GtkWidget *chk_enabled;
    GtkTextBuffer *desc_txt;
    GtkTextIter begin, end;
    gboolean results = TRUE;
    DeletePOI dpoi = {NULL, NULL, 0};
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(cat_id > 0)
    {
        if(SQLITE_OK != sqlite_get_table_printf(_db,
                    "select c.label, c.desc, c.enabled"
                    " from category c"
                    " where c.cat_id = %d",
                    &pszResult, &nRow, &nColumn, NULL,
                    cat_id))
        {
            vprintf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
            return FALSE;
        }

        if(nRow == 0)
            return FALSE;

        cat_label = g_strdup(pszResult[1 * nColumn + 0]);
        cat_desc  = g_strdup(pszResult[1 * nColumn + 1]);
        cat_enabled = atoi(pszResult[1 * nColumn + 2]);
        sqlite_free_table(pszResult);

        dialog = gtk_dialog_new_with_buttons(_("Edit Category"),
            GTK_WINDOW(_window), GTK_DIALOG_MODAL,
            GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
            NULL);

        gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area),
                btn_delete = gtk_button_new_with_label(_("Delete")));

        dpoi.dialog = dialog;
        dpoi.txt_label = g_strdup_printf("%s", cat_label);
        dpoi.id = cat_id;

        g_signal_connect(G_OBJECT(btn_delete), "clicked",
                          G_CALLBACK(category_delete), &dpoi);

        gtk_dialog_add_button(GTK_DIALOG(dialog),
                GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT);
    }
    else
    {
        cat_enabled = 1;
        cat_label = g_strdup("");
        cat_id = 0;
        cat_desc = g_strdup("");

        dialog = gtk_dialog_new_with_buttons(_("Add Category"),
            GTK_WINDOW(_window), GTK_DIALOG_MODAL,
            GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
            GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
            NULL);
    }

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
            table = gtk_table_new(6, 4, FALSE), TRUE, TRUE, 0);

    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new(_("Label: ")),
            0, 1, 0, 1, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(table),
            txt_label = gtk_entry_new(),
            1, 2, 0, 1, GTK_EXPAND | GTK_FILL, 0, 2, 4);

    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new(_("Desc.: ")),
            0, 1, 1, 2, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);

    txt_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(txt_scroll),
                                   GTK_SHADOW_IN);
    gtk_table_attach(GTK_TABLE(table),
            txt_scroll,
            1, 2, 1, 2, GTK_EXPAND | GTK_FILL, 0, 2, 4);

    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(txt_scroll),
                                 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    txt_desc = gtk_text_view_new ();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(txt_desc), GTK_WRAP_WORD);

    gtk_container_add(GTK_CONTAINER(txt_scroll), txt_desc);
    gtk_widget_show(txt_scroll);
    gtk_widget_set_size_request(GTK_WIDGET(txt_scroll), 400, 60);

    desc_txt = gtk_text_view_get_buffer(GTK_TEXT_VIEW(txt_desc));

    gtk_table_attach(GTK_TABLE(table),
            chk_enabled = gtk_check_button_new_with_label(
                _("Enabled")),
            0, 2, 2, 3, GTK_EXPAND | GTK_FILL, 0, 2, 4);

    /* label */
    gtk_entry_set_text(GTK_ENTRY(txt_label), cat_label);

    /* desc */
    gtk_text_buffer_set_text(desc_txt, cat_desc, -1);

    /* enabled */
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chk_enabled),
            (cat_enabled == 1 ? TRUE : FALSE));

    g_free(cat_label);
    cat_label = NULL;
    g_free(cat_desc);
    cat_desc = NULL;

    gtk_widget_show_all(dialog);

    while(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(dialog)))
    {
        if(strlen(gtk_entry_get_text(GTK_ENTRY(txt_label))))
            cat_label = g_strdup(gtk_entry_get_text(GTK_ENTRY(txt_label)));
        else
        {
            popup_error(dialog, _("Please specify a name for the category."));
            continue;
        }

        gtk_text_buffer_get_iter_at_offset(desc_txt, &begin,0 );
        gtk_text_buffer_get_end_iter (desc_txt, &end);
        cat_desc = gtk_text_buffer_get_text(desc_txt, &begin, &end, TRUE);

        cat_enabled = (gtk_toggle_button_get_active(
                GTK_TOGGLE_BUTTON(chk_enabled)) ? 1 : 0);

        if(cat_id > 0)
        {
            /* edit category */
            if(SQLITE_OK != sqlite_exec_printf(_db,
                        "update category set label = %Q, desc = %Q, "
                        "enabled = %d where poi_id = %d",
                        NULL, NULL, NULL,
                        cat_label, cat_desc, cat_enabled, cat_id))
            {
                MACRO_BANNER_SHOW_INFO(_window,_("Problem updating category"));
                results = FALSE;
            }
        }
        else
        {
            /* add category */
            if(SQLITE_OK != sqlite_exec_printf(_db,
                        "insert into category (label, desc, enabled) "
                        "values (%Q, %Q, %d)",
                        NULL, NULL, NULL,
                        cat_label, cat_desc, cat_enabled))
            {
                MACRO_BANNER_SHOW_INFO(_window, _("Problem adding category"));
                results = FALSE;
            }
        }
        break;
    }

    g_free(cat_label);
    g_free(cat_desc);
    g_free(dpoi.txt_label);

    g_object_unref (desc_txt);

    gtk_widget_hide_all(dialog);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return results;
}

static void
category_toggled (GtkCellRendererToggle *cell,
           gchar                 *path,
           gpointer               data)
{
    GtkTreeIter iter;
    gboolean cat_enabled;
    guint cat_id;
    printf("%s()\n", __PRETTY_FUNCTION__);

    GtkTreeModel *model = GTK_TREE_MODEL(data);
    if( !gtk_tree_model_get_iter_from_string(model, &iter, path) )
        return;

    gtk_tree_model_get(model, &iter, CAT_ENABLED, &cat_enabled, -1);
    gtk_tree_model_get(model, &iter, CAT_ID, &cat_id, -1);

    cat_enabled ^= 1;

    if(SQLITE_OK != sqlite_exec_printf(_db,
                "update category set enabled = %d where cat_id = %d",
                NULL, NULL, NULL,
                (cat_enabled ? 1 : 0), cat_id))
    {
        MACRO_BANNER_SHOW_INFO(_window, _("Problem updating Category"));
    }
    else
        gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                   CAT_ENABLED, cat_enabled, -1);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
}

static GtkListStore*
generate_store()
{
    guint i;
    GtkTreeIter iter;
    GtkListStore *store;
    gchar **pszResult;
    gint nRow, nColumn;
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(SQLITE_OK != sqlite_get_table(_db,
                "select c.cat_id, c.enabled, c.label, c.desc"
                " from category c "
                " order by c.label",
                &pszResult, &nRow, &nColumn, NULL))
    {
        vprintf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
        return NULL;
    }

    store = gtk_list_store_new(CAT_NUM_COLUMNS,
                               G_TYPE_UINT,
                               G_TYPE_BOOLEAN,
                               G_TYPE_STRING,
                               G_TYPE_STRING);

    for(i = 1; i < nRow + 1; i++)
    {
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                CAT_ID, atoi(pszResult[i * nColumn + 0]),
                CAT_ENABLED,
                (atoi(pszResult[i * nColumn + 1]) == 1 ? TRUE : FALSE),
                CAT_LABEL, pszResult[i * nColumn + 2],
                CAT_DESCRIPTION, pszResult[i * nColumn + 3],
                -1);
    }

    sqlite_free_table(pszResult);
    vprintf("%s(): return %p\n", __PRETTY_FUNCTION__, store);
    return store;
}

static gboolean
category_add(GtkWidget *widget, GtkWidget *tree_view)
{
    GtkListStore *store;
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(category_dialog(0))
    {
        store = generate_store();
        gtk_tree_view_set_model(
                GTK_TREE_VIEW(tree_view),
                GTK_TREE_MODEL(store));
        g_object_unref(G_OBJECT(store));
    }
    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
category_edit(GtkWidget *widget, GtkWidget *tree_view)
{
    GtkTreeIter iter;
    GtkTreeModel *store;
    GtkTreeSelection *selection;
    printf("%s()\n", __PRETTY_FUNCTION__);

    store = gtk_tree_view_get_model(GTK_TREE_VIEW(tree_view));
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_view));
    if(gtk_tree_selection_get_selected(selection, &store, &iter))
    {
        GValue val;
        memset(&val, 0, sizeof(val));
        gtk_tree_model_get_value(store, &iter, 0, &val);
        if(category_dialog(g_value_get_uint(&val)))
        {
            GtkListStore *new_store = generate_store();
            gtk_tree_view_set_model(
                    GTK_TREE_VIEW(tree_view),
                    GTK_TREE_MODEL(new_store));
            g_object_unref(G_OBJECT(new_store));
        }
    }
    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
category_list()
{
    GtkWidget *dialog;
    GtkWidget *tree_view;
    GtkWidget *sw;
    GtkWidget *btn_edit;
    GtkWidget *btn_add;
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;
    GtkListStore *store;
    printf("%s()\n", __PRETTY_FUNCTION__);

    dialog = gtk_dialog_new_with_buttons(_("Category List"),
            GTK_WINDOW(_window), GTK_DIALOG_MODAL,
            GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
            NULL);

    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area),
            btn_edit = gtk_button_new_with_label(_("Edit")));

    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area),
            btn_add = gtk_button_new_with_label(_("Add")));

    store = generate_store();

    if(!store)
        return TRUE;

    sw = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW (sw),
                  GTK_POLICY_NEVER,
                  GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
            sw, TRUE, TRUE, 0);

    tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    /* Maemo-related? */
    g_object_set(tree_view, "allow-checkbox-mode", FALSE, NULL);
    gtk_container_add (GTK_CONTAINER (sw), tree_view);

    gtk_tree_selection_set_mode(
            gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_view)),
            GTK_SELECTION_SINGLE);
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree_view), TRUE);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(
            "ID", renderer, "text", CAT_ID);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
    gtk_tree_view_column_set_max_width (column, 1);

    renderer = gtk_cell_renderer_toggle_new();
    g_signal_connect (renderer, "toggled",
            G_CALLBACK (category_toggled), store);
    column = gtk_tree_view_column_new_with_attributes(
            _("Enabled"), renderer, "active", CAT_ENABLED);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);

    g_object_unref(G_OBJECT(store));

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(
            _("Label"), renderer, "text", CAT_LABEL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(
            _("Desc."), renderer, "text", CAT_DESCRIPTION);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);

    gtk_window_set_default_size(GTK_WINDOW(dialog), 500, 300);
    gtk_widget_show_all(dialog);

    g_signal_connect(G_OBJECT(btn_edit), "clicked",
            G_CALLBACK(category_edit), tree_view);

    g_signal_connect(G_OBJECT(btn_add), "clicked",
            G_CALLBACK(category_add), tree_view);

    while(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(dialog)))
    {
        break;
    }
    gtk_widget_destroy(dialog);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_category(GtkAction *action)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(category_list())
        map_force_redraw();

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
poi_delete(GtkWidget *widget, DeletePOI *dpoi)
{
    GtkWidget *dialog;
    guint i;
    gchar *buffer;
    printf("%s()\n", __PRETTY_FUNCTION__);

    buffer = g_strdup_printf("%s\n%s", _("Delete POI?"), dpoi->txt_label);
    dialog = hildon_note_new_confirmation (GTK_WINDOW(_window), buffer);
    g_free(buffer);
    i = gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (GTK_WIDGET (dialog));

    if(i == GTK_RESPONSE_OK)
    {
        if(SQLITE_OK != sqlite_exec_printf(_db,
                    "delete from poi where poi_id = %d",
                    NULL, NULL, NULL,
                    dpoi->id))
        {
            MACRO_BANNER_SHOW_INFO(_window, _("Problem deleting POI"));
        }
        else
        {
            gtk_widget_hide_all(dpoi->dialog);
            map_force_redraw();
        }
    }

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static guint
poi_list(gchar **pszResult, gint nRow, gint nColumn)
{
    GtkWidget *dialog;
    GtkWidget *list;
    GtkWidget *sw;
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;
    GtkListStore *store;
    GtkTreeIter iter;
    guint i, row = -1;
    gchar tmp1[15], tmp2[15];
    printf("%s()\n", __PRETTY_FUNCTION__);

    dialog = gtk_dialog_new_with_buttons(_("Select POI"),
            GTK_WINDOW(_window), GTK_DIALOG_MODAL,
            GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
            GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
            NULL);

    store = gtk_list_store_new(POI_NUM_COLUMNS,
                               G_TYPE_INT,
                               G_TYPE_STRING,
                               G_TYPE_STRING,
                               G_TYPE_STRING,
                               G_TYPE_STRING);

    for(i = 1; i < nRow + 1; i++)
    {
        gtk_list_store_append(store, &iter);
        deg_format(g_ascii_strtod(pszResult[i * nColumn + 0], NULL),
                tmp1);
        deg_format(g_ascii_strtod(pszResult[i * nColumn + 1], NULL),
                tmp2);
        gtk_list_store_set(store, &iter,
                POI_INDEX, i,
                POI_LATITUDE, tmp1,
                POI_LONGITUDE, tmp2,
                POI_LABEL, pszResult[i * nColumn + 2],
                POI_CATEGORY, pszResult[i * nColumn + 6],
                -1);
    }

    gtk_window_set_default_size(GTK_WINDOW(dialog), 500, 300);

    sw = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw),
                   GTK_SHADOW_ETCHED_IN);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                  GTK_POLICY_NEVER,
                  GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
            sw, TRUE, TRUE, 0);

    list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    g_object_unref(G_OBJECT(store));
    gtk_container_add (GTK_CONTAINER (sw), list);

    gtk_tree_selection_set_mode(
            gtk_tree_view_get_selection(GTK_TREE_VIEW(list)),
            GTK_SELECTION_SINGLE);
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(list), TRUE);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(
            "POI_INDEX", renderer, "text", POI_INDEX);
    gtk_tree_view_append_column(GTK_TREE_VIEW(list), column);
    gtk_tree_view_column_set_max_width (column, 1);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(
            _("Latitude"), renderer, "text", POI_LATITUDE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(list), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(
            _("Longitude"), renderer, "text", POI_LONGITUDE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(list), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(
            _("Label"), renderer, "text", POI_LABEL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(list), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(
            _("Category"), renderer, "text", POI_CATEGORY);
    gtk_tree_view_append_column(GTK_TREE_VIEW(list), column);

    gtk_widget_show_all(dialog);

    while(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(dialog)))
    {
        GtkTreeIter iter;
        if(gtk_tree_selection_get_selected(
                    gtk_tree_view_get_selection(GTK_TREE_VIEW(list)),
                    NULL, &iter))
        {
            gtk_tree_model_get(GTK_TREE_MODEL(store),
                &iter, 0, &row, -1);
            break;
        }
        else
            popup_error(dialog, _("Select one POI from the list."));
    }

    gtk_widget_destroy(dialog);

    vprintf("%s(): return %d\n", __PRETTY_FUNCTION__, row);
    return row;
}

static void
poi_populate_cat_combo(GtkWidget *cmb_category, guint cat_id)
{
    gchar **pszResult;
    gint nRow, nColumn, row;
    guint i, catindex = 0;
    gint n_children;
    printf("%s()\n", __PRETTY_FUNCTION__);

    n_children = gtk_tree_model_iter_n_children(
            gtk_combo_box_get_model(GTK_COMBO_BOX(cmb_category)), NULL);

    for(i = 0; i < n_children; i++)
        gtk_combo_box_remove_text(GTK_COMBO_BOX(cmb_category), 0);

    if(SQLITE_OK == sqlite_get_table(_db,
                "select c.label, c.cat_id from category c order by c.label",
                &pszResult, &nRow, &nColumn, NULL))
    {
        for(row=1; row<nRow+1; row++)
        {
            gtk_combo_box_append_text(GTK_COMBO_BOX(cmb_category),
                pszResult[row*nColumn]);
            if(atoi(pszResult[row*nColumn+1]) == cat_id)
                catindex = row - 1;
        }
        sqlite_free_table(pszResult);
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(cmb_category), catindex);
    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

typedef struct _PoiCategoryEditInfo PoiCategoryEditInfo;
struct _PoiCategoryEditInfo
{
    GtkWidget *cmb_category;
    guint cat_id;
};

static gboolean
poi_edit_cat(GtkWidget *widget, PoiCategoryEditInfo *data)
{
    printf("%s()\n", __PRETTY_FUNCTION__);
    if(category_list())
        poi_populate_cat_combo(data->cmb_category, data->cat_id);
    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
poi_dialog(guint action)
{
    gchar **pszResult;
    gint nRow, nColumn, rowindex;
    gchar *poi_label = NULL;
    gchar *poi_category = NULL;
    gchar *poi_desc = NULL;
    guint unitx, unity, cat_id = 0, poi_id = 0;
    gfloat lat, lon, lat1, lon1, lat2, lon2, tmp;
    gchar slat1[10], slon1[10];
    gchar slat2[10], slon2[10], tmp1[15], tmp2[15];
    gchar *p_latlon, *p_label = NULL, *p_desc = NULL;
    GtkWidget *dialog;
    GtkWidget *table;
    GtkWidget *label;
    GtkWidget *txt_label;
    GtkWidget *cmb_category;
    GtkWidget *txt_desc;
    GtkWidget *btn_delete = NULL;
    GtkWidget *btn_catedit;
    GtkWidget *hbox;
    GtkWidget *txt_scroll;
    GtkTextBuffer *desc_txt;
    GtkTextIter begin, end;
    DeletePOI dpoi = {NULL, NULL, 0};
    PoiCategoryEditInfo pcedit;
    printf("%s()\n", __PRETTY_FUNCTION__);

    unitx = x2unit(_cmenu_position_x);
    unity = y2unit(_cmenu_position_y);
    unit2latlon(unitx, unity, lat, lon);

    if(action == ACTION_EDIT_POI)
    {
        unitx = x2unit(_cmenu_position_x - (3 * _draw_line_width));
        unity = y2unit(_cmenu_position_y - (3 * _draw_line_width));
        unit2latlon(unitx, unity, lat1, lon1);

        unitx = x2unit(_cmenu_position_x + (3 * _draw_line_width));
        unity = y2unit(_cmenu_position_y + (3 * _draw_line_width));
        unit2latlon(unitx, unity, lat2, lon2);

        if(lat1 > lat2)
        {
            tmp = lat2;
            lat2 = lat1;
            lat1 = tmp;
        }
        if(lon1 > lon2)
        {
            tmp = lon2;
            lon2 = lon1;
            lon1 = tmp;
        }

        g_ascii_dtostr(slat1, sizeof(slat1), lat1);
        g_ascii_dtostr(slon1, sizeof(slon1), lon1);
        g_ascii_dtostr(slat2, sizeof(slat2), lat2);
        g_ascii_dtostr(slon2, sizeof(slon2), lon2);

        if(SQLITE_OK != sqlite_get_table_printf(_db,
                    "select p.lat, p.lon, p.label, p.desc, p.cat_id,"
                    " p.poi_id, c.label"
                    " from poi p, category c "
                    " where p.lat between %s and %s "
                    " and p.lon between %s and %s "
                    " and c.enabled = 1 and p.cat_id = c.cat_id",
                    &pszResult, &nRow, &nColumn, NULL,
                    slat1, slat2, slon1, slon2))
        {
            vprintf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
            return FALSE;
        }

        if(nRow == 0)
            return FALSE;

        if(nRow > 1)
        {
            rowindex = poi_list(pszResult, nRow, nColumn);
            if(rowindex == -1)
                return FALSE;
        }
        else
            rowindex = 1;

        deg_format(
            g_ascii_strtod(pszResult[rowindex * nColumn + 0], NULL),
            tmp1);
        deg_format(
            g_ascii_strtod(pszResult[rowindex * nColumn + 1], NULL),
            tmp2);
        p_latlon = g_strdup_printf("%s, %s", tmp1, tmp2);
        p_label = g_strdup(pszResult[rowindex * nColumn + 2]);
        p_desc = g_strdup(pszResult[rowindex * nColumn + 3]);
        cat_id = atoi(pszResult[rowindex * nColumn + 4]);
        poi_id = atoi(pszResult[rowindex * nColumn + 5]);
        sqlite_free_table(pszResult);

        dialog = gtk_dialog_new_with_buttons(_("Edit POI"),
            GTK_WINDOW(_window), GTK_DIALOG_MODAL,
            GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
            NULL);

        gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area),
                btn_delete = gtk_button_new_with_label(_("Delete")));

        dpoi.dialog = dialog;
        dpoi.txt_label = g_strdup(p_label);
        dpoi.id = poi_id;

        g_signal_connect(G_OBJECT(btn_delete), "clicked",
                          G_CALLBACK(poi_delete), &dpoi);

        gtk_dialog_add_button(GTK_DIALOG(dialog),
                GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT);
    }
    else
    {
        deg_format(lat, tmp1);
        deg_format(lon, tmp2);
        p_latlon = g_strdup_printf("%s, %s", tmp1, tmp2);

        if(SQLITE_OK == sqlite_get_table(_db,
                    "select ifnull(max(poi_id) + 1,1) from poi",
            &pszResult, &nRow, &nColumn, NULL))
        {
            p_label = g_strdup_printf("Point%06d", atoi(pszResult[nColumn]));
            sqlite_free_table(pszResult);
        }

        p_desc = g_strdup("");
        cat_id = 0;
        poi_id = 0;

        dialog = gtk_dialog_new_with_buttons(_("Add POI"),
            GTK_WINDOW(_window), GTK_DIALOG_MODAL,
            GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
            GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
            NULL);
    }

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
            table = gtk_table_new(6, 4, FALSE), TRUE, TRUE, 0);

    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new(_("Lat, Lon: ")),
            0, 1, 0, 1, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);

    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new(p_latlon),
            1, 3, 0, 1, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0f, 0.5f);

    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new(_("Label: ")),
            0, 1, 1, 2, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(table),
            txt_label = gtk_entry_new(),
            1, 2, 1, 2, GTK_EXPAND | GTK_FILL, 0, 2, 4);

    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new(_("Category: ")),
            0, 1, 3, 4, GTK_FILL, 0, 2, 4);
    gtk_table_attach(GTK_TABLE(table),
            hbox = gtk_hbox_new(FALSE, 4),
            1, 2, 3, 4, GTK_EXPAND | GTK_FILL, 0, 2, 4);
    gtk_box_pack_start(GTK_BOX(hbox),
            cmb_category = gtk_combo_box_new_text(),
            FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(hbox),
            btn_catedit = gtk_button_new_with_label(_("Edit Categories...")),
            FALSE, FALSE, 4);

    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new(_("Desc.: ")),
            0, 1, 5, 6, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);

    txt_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(txt_scroll),
                                   GTK_SHADOW_IN);
    gtk_table_attach(GTK_TABLE(table),
            txt_scroll,
            1, 2, 5, 6, GTK_EXPAND | GTK_FILL, 0, 2, 4);

    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(txt_scroll),
                                 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    txt_desc = gtk_text_view_new ();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(txt_desc), GTK_WRAP_WORD);

    gtk_container_add(GTK_CONTAINER(txt_scroll), txt_desc);
    gtk_widget_show(txt_scroll);
    gtk_widget_set_size_request(GTK_WIDGET(txt_scroll), 400, 60);

    desc_txt = gtk_text_view_get_buffer (GTK_TEXT_VIEW (txt_desc));

    /* label */
    gtk_entry_set_text(GTK_ENTRY(txt_label), p_label);

    /* category */
    poi_populate_cat_combo(cmb_category, cat_id);

    /* poi_desc */
    gtk_text_buffer_set_text(desc_txt, p_desc, -1);

    /* Connect Signals */
    pcedit.cmb_category = cmb_category;
    pcedit.cat_id = cat_id;
    g_signal_connect(G_OBJECT(btn_catedit), "clicked",
            G_CALLBACK(poi_edit_cat), &pcedit);
    gtk_widget_show_all(dialog);

    while(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(dialog)))
    {
        if(strlen(gtk_entry_get_text(GTK_ENTRY(txt_label))))
            poi_label = g_strdup(gtk_entry_get_text(
                        GTK_ENTRY(txt_label)));
        else
        {
            popup_error(dialog, _("Please specify a name for the POI."));
            continue;
        }

        gtk_text_buffer_get_iter_at_offset(desc_txt, &begin,0 );
        gtk_text_buffer_get_end_iter (desc_txt, &end);
        poi_desc = gtk_text_buffer_get_text(desc_txt, &begin, &end, TRUE);

        poi_category = gtk_combo_box_get_active_text(
                GTK_COMBO_BOX(cmb_category));

        if(SQLITE_OK == sqlite_get_table_printf(_db,
                    "select cat_id from category where label = %Q",
                    &pszResult, &nRow, &nColumn, NULL,
                    poi_category))
        {
            if(action == ACTION_EDIT_POI)
            {
                /* edit poi */
                if(SQLITE_OK != sqlite_exec_printf(_db,
                            "update poi set label = %Q, desc = %Q, "
                            "cat_id = %s where poi_id = %d",
                            NULL, NULL, NULL,
                            poi_label, poi_desc, pszResult[nColumn], poi_id))
                {
                    MACRO_BANNER_SHOW_INFO(_window, _("Problem updating POI"));
                }
                else
                    map_render_poi();
            }
            else
            {
                /* add poi */
                g_ascii_dtostr(slat1, sizeof(slat1), lat);
                g_ascii_dtostr(slon1, sizeof(slon1), lon);
                if(SQLITE_OK != sqlite_exec_printf(_db,
                            "insert into poi (lat, lon, label, desc, cat_id)"
                            " values (%s, %s, %Q, %Q, %s)",
                            NULL, NULL, NULL,
                            slat1, slon1, poi_label, poi_desc,
                            pszResult[nColumn]))
                {
                    MACRO_BANNER_SHOW_INFO(_window, _("Problem adding POI"));
                }
                else
                    map_render_poi();
            }

            sqlite_free_table(pszResult);
        }
        break;
    }

    g_object_unref (desc_txt);
    g_free(poi_label);
    g_free(poi_category);
    g_free(poi_desc);

    g_free(p_latlon);
    g_free(p_label);
    g_free(p_desc);

    gtk_widget_hide_all(dialog);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
cmenu_cb_add_poi(GtkAction *action)
{
    gboolean results;
    printf("%s()\n", __PRETTY_FUNCTION__);

    results = poi_dialog(ACTION_ADD_POI);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return results;
}

static gboolean
cmenu_cb_edit_poi(GtkAction *action)
{
    gboolean results;
    printf("%s()\n", __PRETTY_FUNCTION__);

    results = poi_dialog(ACTION_EDIT_POI);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return results;
}

/****************************************************************************
 * ABOVE: CALLBACKS *********************************************************
 ****************************************************************************/

