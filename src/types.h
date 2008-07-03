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

#ifndef MAEMO_MAPPER_TYPES_H
#define MAEMO_MAPPER_TYPES_H

#include <time.h>
#include <gdbm.h>
#include <gtk/gtk.h>
#include <libgnomevfs/gnome-vfs.h>

#define _(String) gettext(String)

/* #define MAPDB_SQLITE */

#ifdef MAPDB_SQLITE
#include "sqlite3.h"
#endif

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

/** This enumerated type defines the supported types of repositories. */
typedef enum
{
    REPOTYPE_NONE, /* No URL set. */
    REPOTYPE_XYZ, /* x=%d, y=%d, and zoom=%d */
    REPOTYPE_XYZ_SIGNED, /* x=%d, y=%d, and zoom=%d-2 */
    REPOTYPE_XYZ_INV, /* zoom=%0d, x=%d, y=%d */
    REPOTYPE_QUAD_QRST, /* t=%s   (%s = {qrst}*) */
    REPOTYPE_QUAD_ZERO, /* t=%0s  (%0s = {0123}*) */
    REPOTYPE_WMS,       /* "service=wms" */
    REPOTYPE_YANDEX     /* "maps.yandex" */
} RepoType;


/* Possible World units schemes. There was only one for long type:
   google. */
typedef enum
{
    UNITSTYPE_GOOGLE,
    UNITSTYPE_YANDEX,
} UnitsType;


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
    INSIDE_WPT,
    INSIDE_WPT_NAME,
    INSIDE_WPT_DESC,
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
    CAT_DESC,
    CAT_POI_CNT,
    CAT_NUM_COLUMNS
} CategoryList;

/** POI list **/
typedef enum
{
    POI_SELECTED,
    POI_POIID,
    POI_CATID,
    POI_LAT,
    POI_LON,
    POI_LATLON,
    POI_BEARING,
    POI_DISTANCE,
    POI_LABEL,
    POI_DESC,
    POI_CLABEL,
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

typedef enum
{
    UNBLANK_WITH_GPS,
    UNBLANK_WHEN_MOVING,
    UNBLANK_FULLSCREEN,
    UNBLANK_WAYPOINT,
    UNBLANK_NEVER,
    UNBLANK_ENUM_COUNT
} UnblankOption;

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

/** This enum defines the possible font sizes. */
typedef enum
{
    ROTATE_DIR_UP,
    ROTATE_DIR_RIGHT,
    ROTATE_DIR_DOWN,
    ROTATE_DIR_LEFT,
    ROTATE_DIR_ENUM_COUNT
} RotateDir;

/** This enum defines all of the key-customizable actions. */
typedef enum
{
    CUSTOM_ACTION_PAN_NORTH,
    CUSTOM_ACTION_PAN_WEST,
    CUSTOM_ACTION_PAN_SOUTH,
    CUSTOM_ACTION_PAN_EAST,
    CUSTOM_ACTION_PAN_UP,
    CUSTOM_ACTION_PAN_DOWN,
    CUSTOM_ACTION_PAN_LEFT,
    CUSTOM_ACTION_PAN_RIGHT,
    CUSTOM_ACTION_RESET_VIEW_ANGLE,
    CUSTOM_ACTION_ROTATE_CLOCKWISE,
    CUSTOM_ACTION_ROTATE_COUNTERCLOCKWISE,
    CUSTOM_ACTION_TOGGLE_AUTOCENTER,
    CUSTOM_ACTION_TOGGLE_AUTOROTATE,
    CUSTOM_ACTION_ZOOM_IN,
    CUSTOM_ACTION_ZOOM_OUT,
    CUSTOM_ACTION_TOGGLE_FULLSCREEN,
    CUSTOM_ACTION_TOGGLE_TRACKING,
    CUSTOM_ACTION_TOGGLE_TRACKS,
    CUSTOM_ACTION_TOGGLE_SCALE,
    CUSTOM_ACTION_TOGGLE_POI,
    CUSTOM_ACTION_CHANGE_REPO,
    CUSTOM_ACTION_ROUTE_DISTNEXT,
    CUSTOM_ACTION_ROUTE_DISTLAST,
    CUSTOM_ACTION_TRACK_BREAK,
    CUSTOM_ACTION_TRACK_CLEAR,
    CUSTOM_ACTION_TRACK_DISTLAST,
    CUSTOM_ACTION_TRACK_DISTFIRST,
    CUSTOM_ACTION_TOGGLE_GPS,
    CUSTOM_ACTION_TOGGLE_GPSINFO,
    CUSTOM_ACTION_TOGGLE_SPEEDLIMIT,
    CUSTOM_ACTION_RESET_BLUETOOTH,
    CUSTOM_ACTION_ENUM_COUNT
} CustomAction;

/** This enum defines all of the customizable keys. */
typedef enum
{
    CUSTOM_KEY_UP,
    CUSTOM_KEY_LEFT,
    CUSTOM_KEY_DOWN,
    CUSTOM_KEY_RIGHT,
    CUSTOM_KEY_SELECT,
    CUSTOM_KEY_INCREASE,
    CUSTOM_KEY_DECREASE,
    CUSTOM_KEY_FULLSCREEN,
    CUSTOM_KEY_ESC,
    CUSTOM_KEY_ENUM_COUNT
} CustomKey;

/** This enum defines all of the colorable objects. */
typedef enum
{
    COLORABLE_MARK,
    COLORABLE_MARK_VELOCITY,
    COLORABLE_MARK_OLD,
    COLORABLE_TRACK,
    COLORABLE_TRACK_MARK,
    COLORABLE_TRACK_BREAK,
    COLORABLE_ROUTE,
    COLORABLE_ROUTE_WAY,
    COLORABLE_ROUTE_BREAK,
    COLORABLE_POI,
    COLORABLE_ENUM_COUNT
} Colorable;

typedef enum
{
    DDPDDDDD,
    DD_MMPMMM,
    DD_MM_SSPS,
    DDPDDDDD_NSEW,
    DD_MMPMMM_NSEW,
    DD_MM_SSPS_NSEW,
    NSEW_DDPDDDDD,
    NSEW_DD_MMPMMM,
    NSEW_DD_MM_SSPS,
    DEG_FORMAT_ENUM_COUNT
} DegFormat;

typedef enum
{
    SPEED_LOCATION_BOTTOM_LEFT,
    SPEED_LOCATION_BOTTOM_RIGHT,
    SPEED_LOCATION_TOP_RIGHT,
    SPEED_LOCATION_TOP_LEFT,
    SPEED_LOCATION_ENUM_COUNT
} SpeedLocation;

typedef enum
{
    MAP_UPDATE_ADD,
    MAP_UPDATE_OVERWRITE,
    MAP_UPDATE_AUTO,
    MAP_UPDATE_DELETE,
    MAP_UPDATE_ENUM_COUNT
} MapUpdateType;

typedef enum
{
    GPS_RCVR_BT,
    GPS_RCVR_GPSD,
    GPS_RCVR_FILE,
    GPS_RCVR_ENUM_COUNT
} GpsRcvrType;

/** A general definition of a point in the Maemo Mapper unit system. */
typedef struct _Point Point;
struct _Point {
    gint unitx;
    gint unity;
    time_t time;
    gint altitude;
};

/** A WayPoint, which is a Point with a description. */
typedef struct _WayPoint WayPoint;
struct _WayPoint {
    Point *point;
    gchar *desc;
};

/** A Path is a set of PathPoints and WayPoints. */
typedef struct _Path Path;
struct _Path {
    Point *head; /* points to first element in array; NULL if empty. */
    Point *tail; /* points to last element in array. */
    Point *cap; /* points after last slot in array. */
    WayPoint *whead; /* points to first element in array; NULL if empty. */
    WayPoint *wtail; /* points to last element in array. */
    WayPoint *wcap; /* points after last slot in array. */
};

/** Data used during the SAX parsing operation. */
typedef struct _SaxData SaxData;
struct _SaxData {
    SaxState state;
    SaxState prev_state;
    gint unknown_depth;
    gboolean at_least_one_trkpt;
    GString *chars;
};

typedef struct _PathSaxData PathSaxData;
struct _PathSaxData {
    SaxData sax_data;
    Path path;
};

/** Data to describe a POI. */
typedef struct _PoiInfo PoiInfo;
struct _PoiInfo {
    gint poi_id;
    gint cat_id;
    gdouble lat;
    gdouble lon;
    gchar *label;
    gchar *desc;
    gchar *clabel;
};

typedef struct _PoiSaxData PoiSaxData;
struct _PoiSaxData {
    SaxData sax_data;
    GList *poi_list;
    PoiInfo *curr_poi;
};

/** Data regarding a map repository. */
typedef struct _RepoData RepoData;
struct _RepoData {
    gchar *name;
    gchar *url;
    gchar *db_filename;
    gchar *db_dirname;
    gint dl_zoom_steps;
    gint view_zoom_steps;
    gboolean double_size;
    gboolean nextable;
    gint min_zoom;
    gint max_zoom;
    RepoType type;
    UnitsType units;
    gint world_size;
#ifdef MAPDB_SQLITE
    sqlite3 *db;
    sqlite3_stmt *stmt_map_select;
    sqlite3_stmt *stmt_map_exists;
    sqlite3_stmt *stmt_map_update;
    sqlite3_stmt *stmt_map_insert;
    sqlite3_stmt *stmt_map_delete;
    sqlite3_stmt *stmt_dup_select;
    sqlite3_stmt *stmt_dup_exists;
    sqlite3_stmt *stmt_dup_insert;
    sqlite3_stmt *stmt_dup_increm;
    sqlite3_stmt *stmt_dup_decrem;
    sqlite3_stmt *stmt_dup_delete;
    sqlite3_stmt *stmt_trans_begin;
    sqlite3_stmt *stmt_trans_commit;
    sqlite3_stmt *stmt_trans_rollback;
#else
    GDBM_FILE db;
#endif
    GtkWidget *menu_item;
};

/** GPS Data and Satellite **/
typedef struct _GpsData GpsData;
struct _GpsData {
    gint fix;
    gint fixquality;
    gdouble lat;
    gdouble lon;
    gfloat speed;    /* in knots */
    gfloat maxspeed;    /* in knots */
    gfloat heading;
    gfloat hdop;
    gfloat pdop;
    gfloat vdop;
    gint satinview;
    gint satinuse;
    gint satforfix[12];
};

typedef struct _GpsSatelliteData GpsSatelliteData;
struct _GpsSatelliteData {
    gint prn;
    gint elevation;
    gint azimuth;
    gint snr;
};

/** Data used for rendering the entire screen. */
typedef struct _MapRenderTask MapRenderTask;
struct _MapRenderTask
{
    RepoData *repo;
    Point new_center;
    gint old_offsetx;
    gint old_offsety;
    gint screen_width_pixels;
    gint screen_height_pixels;
    gint zoom;
    gint rotate_angle;
    gboolean smooth_pan;
    GdkPixbuf *pixbuf;
};

/** Data used for rendering the entire screen. */
typedef struct _MapOffsetArgs MapOffsetArgs;
struct _MapOffsetArgs
{
    gfloat old_center_offset_devx;
    gfloat old_center_offset_devy;
    gfloat new_center_offset_devx;
    gfloat new_center_offset_devy;
    gint rotate_angle;
    gfloat percent_complete;
};

typedef struct _ThreadLatch ThreadLatch;
struct _ThreadLatch
{
    gboolean is_open;
    gboolean is_done_adding_tasks;
    gint num_tasks;
    gint num_done;
    GMutex *mutex;
    GCond *cond;
};

/** Data used during the asynchronous progress update phase of automatic map
 * downloading. */
typedef struct _MapUpdateTask MapUpdateTask;
struct _MapUpdateTask
{
    gint priority;
    gint tilex;
    gint tiley;
    ThreadLatch *refresh_latch;
    GdkPixbuf *pixbuf;
    RepoData *repo;
    gint8 update_type;
    gint8 zoom;
    gint8 vfs_result;
    gint8 batch_id;
};

/** Data used during the asynchronous automatic route downloading operation. */
typedef struct _AutoRouteDownloadData AutoRouteDownloadData;
struct _AutoRouteDownloadData {
    gboolean enabled;
    gboolean in_progress;
    gchar *source_url;
    gchar *dest;
    gboolean avoid_highways;
};

/** Data to describe the GPS connection. */
typedef struct _GpsRcvrInfo GpsRcvrInfo;
struct _GpsRcvrInfo {
    GpsRcvrType type;
    gchar *bt_mac;
    gchar *file_path;
    gchar *gpsd_host;
    gint gpsd_port;
};

typedef struct _BrowseInfo BrowseInfo;
struct _BrowseInfo {
    GtkWidget *dialog;
    GtkWidget *txt;
};

#endif /* ifndef MAEMO_MAPPER_TYPES_H */
