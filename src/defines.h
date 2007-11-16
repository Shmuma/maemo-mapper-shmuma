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

#define PI   (3.14159265358979323846f)

#define EARTH_RADIUS (3440.06479f)

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

#define NUM_DOWNLOAD_THREADS (4)
#define MAX_PIXBUF_DUP_SIZE (137)
#define WORLD_SIZE_UNITS (2 << (MAX_ZOOM + TILE_SIZE_P2))

#define HOURGLASS_SEPARATION (7)

#define deg2rad(deg) ((deg) * (PI / 180.f))
#define rad2deg(rad) ((rad) * (180.f / PI))

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
    (BUFX) = unit2pixel((gint)(fdevx)) + _screen_halfwidth_pixels; \
    (BUFY) = unit2pixel((gint)(fdevy)) + _screen_halfheight_pixels; \
}

#define unit2buf(UNITX, UNITY, BUFX, BUFY) \
    (unit2buf_full(UNITX, UNITY, BUFX, BUFY, _center, _map_rotate_matrix))

#define pixel2buf_full(PIXELX, PIXELY, BUFX, BUFY, CENTER, ZOOM, MATRIX) { \
    gfloat fdevx, fdevy; \
    gdk_pixbuf_rotate_vector(&fdevx, &fdevy, MATRIX, \
            (gint)((PIXELX) - unit2zpixel((CENTER).unitx, (ZOOM))), \
            (gint)((PIXELY) - unit2zpixel((CENTER).unity, (ZOOM)))); \
    (BUFX) = fdevx + _screen_halfwidth_pixels; \
    (BUFY) = fdevy + _screen_halfheight_pixels; \
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
            pixel2unit((gint)((BUFX) - _screen_halfwidth_pixels)), \
            pixel2unit((gint)((BUFY) - _screen_halfheight_pixels))); \
    (UNITX) = funitx + CENTER.unitx; \
    (UNITY) = funity + CENTER.unity; \
}

#define buf2unit(BUFX, BUFY, UNITX, UNITY) \
    (buf2unit_full(BUFX, BUFY, UNITX, UNITY, _center, _map_reverse_matrix))

#define buf2pixel_full(BUFX, BUFY, PIXELX, PIXELY, CENTER, MATRIX) { \
    gfloat fpixelx, fpixely; \
    gdk_pixbuf_rotate_vector(&fpixelx, &fpixely, MATRIX, \
            (gint)(BUFX) - _screen_halfwidth_pixels, \
            (gint)(BUFY) - _screen_halfheight_pixels); \
    (PIXELX) = fpixelx + unit2pixel(CENTER.unitx); \
    (PIXELY) = fpixely + unit2pixel(CENTER.unity); \
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
#define GCONF_KEY_ROUTE_LOCATIONS GCONF_KEY_PREFIX"/route_locations"
#define GCONF_KEY_REPOSITORIES GCONF_KEY_PREFIX"/repositories"
#define GCONF_KEY_CURRREPO GCONF_KEY_PREFIX"/curr_repo"
#define GCONF_KEY_GPS_INFO GCONF_KEY_PREFIX"/gps_info"
#define GCONF_KEY_ROUTE_DL_URL GCONF_KEY_PREFIX"/route_dl_url"
#define GCONF_KEY_ROUTE_DL_RADIUS GCONF_KEY_PREFIX"/route_dl_radius"
#define GCONF_KEY_POI_DL_URL GCONF_KEY_PREFIX"/poi_dl_url"
#define GCONF_KEY_DEG_FORMAT GCONF_KEY_PREFIX"/deg_format"

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
#define HELP_ID_REPOMAN HELP_ID_PREFIX"repoman"
#define HELP_ID_MAPMAN HELP_ID_PREFIX"mapman"
#define HELP_ID_DOWNROUTE HELP_ID_PREFIX"downroute"
#define HELP_ID_DOWNPOI HELP_ID_PREFIX"downpoi"
#define HELP_ID_BROWSEPOI HELP_ID_PREFIX"browsepoi"
#define HELP_ID_POILIST HELP_ID_PREFIX"poilist"
#define HELP_ID_POICAT HELP_ID_PREFIX"poicat"

#define MERCATOR_SPAN (-6.28318377773622f)
#define MERCATOR_TOP (3.14159188886811f)
#define latlon2unit(lat, lon, unitx, unity) { \
    gfloat tmp; \
    unitx = (lon + 180.f) * (WORLD_SIZE_UNITS / 360.f) + 0.5f; \
    tmp = sinf(deg2rad(lat)); \
    unity = 0.5f + (WORLD_SIZE_UNITS / MERCATOR_SPAN) \
        * (logf((1.f + tmp) / (1.f - tmp)) * 0.5f - MERCATOR_TOP); \
}

#define unit2latlon(unitx, unity, lat, lon) { \
    (lon) = ((unitx) * (360.f / WORLD_SIZE_UNITS)) - 180.f; \
    (lat) = (360.f * (atanf(expf(((unity) \
                                  * (MERCATOR_SPAN / WORLD_SIZE_UNITS)) \
                     + MERCATOR_TOP)))) * (1.f / PI) - 90.f; \
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

#define MACRO_QUEUE_DRAW_AREA() \
    gtk_widget_queue_draw_area( \
            _map_widget, \
            0, 0, \
            _screen_width_pixels, \
            _screen_height_pixels)

/* Render all on-map metadata an annotations, including POI and paths. */
#define MACRO_MAP_RENDER_DATA() { \
    if(_show_poi) \
        map_render_poi(); \
    if(_show_paths > 0) \
        map_render_paths(); \
}

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
            if(!MOVING) \
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

#define g_timeout_add(I, F, D) g_timeout_add_full(G_PRIORITY_DEFAULT_IDLE, \
          (I), (F), (D), NULL)

#define MACRO_BANNER_SHOW_INFO(A, S) { \
    gchar *my_macro_buffer = g_markup_printf_escaped( \
            "<span size='%s'>%s</span>", \
            INFO_FONT_ENUM_TEXT[_info_font_size], (S)); \
    hildon_banner_show_information_with_markup(A, NULL, my_macro_buffer); \
    g_free(my_macro_buffer); \
}

#endif /* ifndef MAEMO_MAPPER_DEFINES */