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

#ifndef MAEMO_MAPPER_DEFINES
#define MAEMO_MAPPER_DEFINES

#include <libintl.h>

#define _(String) gettext(String)

#ifndef DEBUG
#define printf(...)
#endif

/* Set the below if to determine whether to get verbose output. */
#if 1
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

#define PI   (3.14159265358979323846)

#define EARTH_RADIUS (3443.91847)

/* BT dbus service location */
#define BASE_PATH                "/org/bluez"
#define BASE_INTERFACE           "org.bluez"
//#define ADAPTER_PATH             BASE_PATH
#define ADAPTER_INTERFACE        BASE_INTERFACE ".Adapter"
#define MANAGER_PATH             BASE_PATH
#define MANAGER_INTERFACE        BASE_INTERFACE ".Manager"
#define ERROR_INTERFACE          BASE_INTERFACE ".Error"
#define SECURITY_INTERFACE       BASE_INTERFACE ".Security"
#define RFCOMM_INTERFACE         BASE_INTERFACE ".RFCOMM"
#define BLUEZ_DBUS               BASE_INTERFACE

#define LIST_ADAPTERS            "ListAdapters"
#define LIST_BONDINGS            "ListBondings"
//#define CREATE_BONDING           "CreateBonding"
#define GET_REMOTE_NAME          "GetRemoteName"
#define GET_REMOTE_SERVICE_CLASSES "GetRemoteServiceClasses"

#define BTCOND_PATH              "/com/nokia/btcond/request"
#define BTCOND_BASE              "com.nokia.btcond"
#define BTCOND_INTERFACE         BTCOND_BASE ".request"
#define BTCOND_REQUEST           BTCOND_INTERFACE
#define BTCOND_CONNECT           "rfcomm_connect"
#define BTCOND_DISCONNECT        "rfcomm_disconnect"
#define BTCOND_DBUS              BTCOND_BASE


/** MAX_ZOOM defines the largest map zoom level we will download.
 * (MAX_ZOOM - 1) is the largest map zoom level that the user can zoom to.
 */
#define MIN_ZOOM (0)
#define MAX_ZOOM (20)

#define TILE_SIZE_PIXELS (256)
#define TILE_HALFDIAG_PIXELS (181)
#define TILE_SIZE_P2 (8)

#define ARRAY_CHUNK_SIZE (1024)

#define BUFFER_SIZE (2048)
#define APRS_BUFFER_SIZE (4096)
#define APRS_MAX_COMMENT  80
#define APRS_CONVERSE_MODE "k"

#define GPSD_PORT_DEFAULT (2947)

#define NUM_DOWNLOAD_THREADS (4)
#define WORLD_SIZE_UNITS (2 << (MAX_ZOOM + TILE_SIZE_P2))

#define HOURGLASS_SEPARATION (7)

#define deg2rad(deg) ((deg) * (PI / 180.0))
#define rad2deg(rad) ((rad) * (180.0 / PI))

#define tile2pixel(TILE) ((TILE) << TILE_SIZE_P2)
#define pixel2tile(PIXEL) ((PIXEL) >> TILE_SIZE_P2)
#define tile2unit(TILE) ((TILE) << (TILE_SIZE_P2 + _zoom))
#define unit2tile(unit) ((unit) >> (TILE_SIZE_P2 + _zoom))
#define tile2zunit(TILE, ZOOM) ((TILE) << (TILE_SIZE_P2 + (ZOOM)))
#define unit2ztile(unit, ZOOM) ((unit) >> (TILE_SIZE_P2 + (ZOOM)))

#define pixel2unit(PIXEL) ((PIXEL) << _zoom)
#define unit2pixel(PIXEL) ((PIXEL) >> _zoom)
#define pixel2zunit(PIXEL, ZOOM) ((PIXEL) << (ZOOM))
#define unit2zpixel(PIXEL, ZOOM) ((PIXEL) >> (ZOOM))

#define unit2buf_full(UNITX, UNITY, BUFX, BUFY, CENTER, MATRIX) { \
    gfloat fdevx, fdevy; \
    gdk_pixbuf_rotate_vector(&fdevx, &fdevy, (MATRIX), \
            (gint)((UNITX) - (CENTER).unitx), \
            (gint)((UNITY) - (CENTER).unity)); \
    (BUFX) = unit2pixel((gint)(fdevx)) + _view_halfwidth_pixels; \
    (BUFY) = unit2pixel((gint)(fdevy)) + _view_halfheight_pixels; \
}

#define unit2buf(UNITX, UNITY, BUFX, BUFY) \
    (unit2buf_full(UNITX, UNITY, BUFX, BUFY, _center, _map_rotate_matrix))

#define pixel2buf_full(PIXELX, PIXELY, BUFX, BUFY, CENTER, ZOOM, MATRIX) { \
    gfloat fdevx, fdevy; \
    gdk_pixbuf_rotate_vector(&fdevx, &fdevy, MATRIX, \
            (gint)((PIXELX) - unit2zpixel((CENTER).unitx, (ZOOM))), \
            (gint)((PIXELY) - unit2zpixel((CENTER).unity, (ZOOM)))); \
    (BUFX) = fdevx + _view_halfwidth_pixels; \
    (BUFY) = fdevy + _view_halfheight_pixels; \
}

#define pixel2buf(PIXELX, PIXELY, BUFX, BUFY) \
    (pixel2buf_full(PIXELX, PIXELY, BUFX, BUFY, _center, _zoom, \
                    _map_rotate_matrix))


#define unit2screen_full(UNITX, UNITY, SCREENX, SCREENY, CENTER, MATRIX) { \
    unit2buf_full(UNITX, UNITY, SCREENX, SCREENY, CENTER, MATRIX); \
    (SCREENX) = (SCREENX) + _map_offset_devx; \
    (SCREENY) = (SCREENY) + _map_offset_devy; \
}

#define unit2screen(UNITX, UNITY, SCREENX, SCREENY) \
    (unit2screen_full(UNITX, UNITY, SCREENX, SCREENY, _center, \
                      _map_rotate_matrix))

#define pixel2screen_full(PIXELX, PIXELY, SCREENX, SCREENY, CENTER, MATRIX) { \
    pixel2buf_full(PIXELX, PIXELY, SCREENX, SCREENY, CENTER, ZOOM, MATRIX); \
    (SCREENX) = (SCREENX) + _map_offset_devx; \
    (SCREENY) = (SCREENY) + _map_offset_devy; \
}

#define pixel2screen(PIXELX, PIXELY, SCREENX, SCREENY) \
    (pixel2screen_full(PIXELX, PIXELY, SCREENX, SCREENY, _center, \
                       _map_rotate_matrix))

#define buf2unit_full(BUFX, BUFY, UNITX, UNITY, CENTER, MATRIX) { \
    gfloat funitx, funity; \
    gdk_pixbuf_rotate_vector(&funitx, &funity, MATRIX, \
            pixel2unit((gint)((BUFX) - _view_halfwidth_pixels)), \
            pixel2unit((gint)((BUFY) - _view_halfheight_pixels))); \
    (UNITX) = (CENTER).unitx + (gint)funitx; \
    (UNITY) = (CENTER).unity + (gint)funity; \
}

#define buf2unit(BUFX, BUFY, UNITX, UNITY) \
    (buf2unit_full(BUFX, BUFY, UNITX, UNITY, _center, _map_reverse_matrix))

#define buf2pixel_full(BUFX, BUFY, PIXELX, PIXELY, CENTER, MATRIX) { \
    gfloat fpixelx, fpixely; \
    gdk_pixbuf_rotate_vector(&fpixelx, &fpixely, MATRIX, \
            (gint)(BUFX) - _view_halfwidth_pixels, \
            (gint)(BUFY) - _view_halfheight_pixels); \
    (PIXELX) = unit2pixel((CENTER).unitx) + (gint)fpixelx; \
    (PIXELY) = unit2pixel((CENTER).unity) + (gint)fpixely; \
}

#define buf2pixel(BUFX, BUFY, PIXELX, PIXELY) \
    (buf2pixel_full(BUFX, BUFY, PIXELX, PIXELY, _center, _map_reverse_matrix))

#define screen2unit_full(SCREENX, SCREENY, UNITX, UNITY, CENTER, MATRIX) ( \
    buf2unit_full((SCREENX) - _map_offset_devx, (SCREENY) - _map_offset_devy, \
        UNITX, UNITY, CENTER, MATRIX))

#define screen2unit(SCREENX, SCREENY, UNITX, UNITY) \
    (screen2unit_full(SCREENX, SCREENY, UNITX, UNITY, _center, \
                      _map_reverse_matrix))

#define screen2pixel_full(SCREENX, SCREENY, PIXELX, PIXELY, CENTER, MATRIX) ( \
    buf2pixel_full((SCREENX) - _map_offset_devx, (SCREENY) - _map_offset_devy,\
        PIXELX, PIXELY, CENTER, MATRIX))

#define screen2pixel(SCREENX, SCREENY, PIXELX, PIXELY) ( \
    screen2pixel_full(SCREENX, SCREENY, PIXELX, PIXELY, \
        _center,_map_reverse_matrix))

/* Pans are done 64 pixels at a time. */
#define PAN_PIXELS (64)
#define ROTATE_DEGREES (30)

#define INITIAL_DOWNLOAD_RETRIES (3)

#define CONFIG_DIR_NAME "~/.maemo-mapper/"
#define CONFIG_PATH_DB_FILE "paths.db"

#define REPO_DEFAULT_NAME "OpenStreet"
#define REPO_DEFAULT_CACHE_BASE "~/MyDocs/.documents/Maps/"
#define REPO_DEFAULT_CACHE_DIR REPO_DEFAULT_CACHE_BASE"OpenStreet.db"
#define REPO_DEFAULT_MAP_URI "http://tile.openstreetmap.org/%0d/%d/%d.png"
#define REPO_DEFAULT_DL_ZOOM_STEPS (2)
#define REPO_DEFAULT_VIEW_ZOOM_STEPS (1)
#define REPO_DEFAULT_MIN_ZOOM (4)
#define REPO_DEFAULT_MAX_ZOOM (20)

#define XML_DATE_FORMAT "%FT%T"

#define HELP_ID_PREFIX "help_maemomapper_"
#define HELP_ID_INTRO HELP_ID_PREFIX"intro"
#define HELP_ID_GETSTARTED HELP_ID_PREFIX"getstarted"
#define HELP_ID_ABOUT HELP_ID_PREFIX"about"
#define HELP_ID_SETTINGS HELP_ID_PREFIX"settings"
#define HELP_ID_NEWREPO HELP_ID_PREFIX"newrepo"
#define HELP_ID_REPOMAN HELP_ID_PREFIX"repoman"
#define HELP_ID_MAPMAN HELP_ID_PREFIX"mapman"
#define HELP_ID_DOWNROUTE HELP_ID_PREFIX"downroute"
#define HELP_ID_DOWNPOI HELP_ID_PREFIX"downpoi"
#define HELP_ID_BROWSEPOI HELP_ID_PREFIX"browsepoi"
#define HELP_ID_POILIST HELP_ID_PREFIX"poilist"
#define HELP_ID_POICAT HELP_ID_PREFIX"poicat"

#define MERCATOR_SPAN (-6.28318377773622)
#define MERCATOR_TOP (3.14159188886811)
#define latlon2unit(lat, lon, unitx, unity) { \
    gdouble tmp; \
    unitx = (lon + 180.0) * (WORLD_SIZE_UNITS / 360.0) + 0.5; \
    tmp = sin(deg2rad(lat)); \
    unity = 0.5 + (WORLD_SIZE_UNITS / MERCATOR_SPAN) \
        * (log((1.0 + tmp) / (1.0 - tmp)) * 0.5 - MERCATOR_TOP); \
}

#define unit2latlon(unitx, unity, lat, lon) { \
    (lon) = ((unitx) * (360.0 / WORLD_SIZE_UNITS)) - 180.0; \
    (lat) = (360.0 * (atan(exp(((unity) * (MERCATOR_SPAN / WORLD_SIZE_UNITS)) \
                     + MERCATOR_TOP)))) * (1.0 / PI) - 90.0; \
}

#define MACRO_PATH_INIT(path) { \
    (path).head = (path).tail = g_new(Point, ARRAY_CHUNK_SIZE); \
    *((path).tail) = _point_null; \
    (path).cap = (path).head + ARRAY_CHUNK_SIZE; \
    (path).whead = g_new(WayPoint, ARRAY_CHUNK_SIZE); \
    (path).wtail = (path).whead - 1; \
    (path).wcap = (path).whead + ARRAY_CHUNK_SIZE; \
}

#define MACRO_PATH_FREE(path) if((path).head) { \
    WayPoint *curr; \
    g_free((path).head); \
    (path).head = (path).tail = (path).cap = NULL; \
    for(curr = (path).whead - 1; curr++ != (path).wtail; ) \
        g_free(curr->desc); \
    g_free((path).whead); \
    (path).whead = (path).wtail = (path).wcap = NULL; \
}

#define MACRO_PATH_INCREMENT_TAIL(route) { \
    if(++(route).tail == (route).cap) \
        path_resize(&(route), (route).cap - (route).head + ARRAY_CHUNK_SIZE);\
}

#define MACRO_PATH_INCREMENT_WTAIL(route) { \
    if(++(route).wtail == (route).wcap) \
        path_wresize(&(route), \
                (route).wcap - (route).whead + ARRAY_CHUNK_SIZE); \
}

#define DISTANCE_SQUARED(a, b) \
   ((guint64)((((gint64)(b).unitx)-(a).unitx)*(((gint64)(b).unitx)-(a).unitx))\
  + (guint64)((((gint64)(b).unity)-(a).unity)*(((gint64)(b).unity)-(a).unity)))

#if OLD_MAP
#define MACRO_QUEUE_DRAW_AREA() \
    gtk_widget_queue_draw_area( \
            _map_widget, \
            0, 0, \
            _view_width_pixels, \
            _view_height_pixels)
#else
#define MACRO_QUEUE_DRAW_AREA()
#endif

/* Render all on-map metadata an annotations, including POI and paths. */
#ifdef INCLUDE_APRS
#define MACRO_MAP_RENDER_DATA() { \
    if(_show_poi) \
        map_render_poi(); \
    if(_show_paths > 0) \
        map_render_paths(); \
    if(_aprs_enable) \
    	map_render_aprs(); \
}
#else
#define MACRO_MAP_RENDER_DATA() { \
    if(_show_poi) \
        map_render_poi(); \
    if(_show_paths > 0) \
        map_render_paths(); \
}
#endif //INCLUDE_APRS

#define UNBLANK_SCREEN(MOVING, APPROACHING_WAYPOINT) { \
    /* Check if we need to unblank the screen. */ \
    switch(_unblank_option) \
    { \
        case UNBLANK_NEVER: \
            break; \
        case UNBLANK_WAYPOINT: \
            if(APPROACHING_WAYPOINT) \
            { \
                printf("Unblanking screen...\n"); \
                osso_display_state_on(_osso); \
                osso_display_blanking_pause(_osso); \
            } \
            break; \
        default: \
        case UNBLANK_FULLSCREEN: \
            if(!_fullscreen) \
                break; \
        case UNBLANK_WHEN_MOVING: \
            if(!(MOVING)) \
                break; \
        case UNBLANK_WITH_GPS: \
            printf("Unblanking screen...\n"); \
            osso_display_state_on(_osso); \
            osso_display_blanking_pause(_osso); \
    } \
}

#define LL_FMT_LEN 20
#define lat_format(A, B) deg_format((A), (B), 'S', 'N')
#define lon_format(A, B) deg_format((A), (B), 'W', 'E')

#define TRACKS_MASK 0x00000001
#define ROUTES_MASK 0x00000002

#define MACRO_BANNER_SHOW_INFO(A, S) { \
    gchar *my_macro_buffer = g_markup_printf_escaped( \
            "<span size='%s'>%s</span>", \
            INFO_FONT_ENUM_TEXT[_info_font_size], (S)); \
    hildon_banner_show_information_with_markup(A, NULL, my_macro_buffer); \
    g_free(my_macro_buffer); \
}

#define MAPDB_EXISTS(map_repo) ( ((map_repo)->is_sqlite) \
    ? ((map_repo)->sqlite_db != NULL) \
    : ((map_repo)->gdbm_db != NULL) )
#define _voice_synth_path "/usr/bin/flite"

#define STR_EMPTY(s) (!s || s[0] == '\0')

/* Sanitize SQLite's sqlite3_column_text: */
#define sqlite3_column_str(stmt, col) \
    ((const gchar *)sqlite3_column_text(stmt, col))

#ifdef DEBUG

#ifdef g_mutex_lock
#undef g_mutex_lock
#endif
#define g_mutex_lock(mutex) \
    G_STMT_START { \
        struct timespec ts0, ts1; \
        long ms_diff; \
        clock_gettime(CLOCK_MONOTONIC, &ts0); \
        G_THREAD_CF (mutex_lock,     (void)0, (mutex)); \
        clock_gettime(CLOCK_MONOTONIC, &ts1); \
        ms_diff = (ts1.tv_sec - ts0.tv_sec) * 1000 + \
                  (ts1.tv_nsec - ts0.tv_nsec) / 1000000; \
        if (ms_diff > 300) \
            g_warning("%s: %ld wait for %s", G_STRLOC, ms_diff, #mutex); \
    } G_STMT_END

#endif /* DEBUG */

#endif /* ifndef MAEMO_MAPPER_DEFINES */
