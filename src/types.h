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
 *
 *  
 * Parts of this code have been ported from Xastir by Rob Williams (10 Aug 2008):
 * 
 *  * XASTIR, Amateur Station Tracking and Information Reporting
 * Copyright (C) 1999,2000  Frank Giannandrea
 * Copyright (C) 2000-2007  The Xastir Group
 * 
 */

#ifndef MAEMO_MAPPER_TYPES_H
#define MAEMO_MAPPER_TYPES_H


#ifdef HAVE_CONFIG_H
#    include "config.h"
#endif

#include <time.h>
#include <gdbm.h>
#include <gtk/gtk.h>
#include <libgnomevfs/gnome-vfs.h>

#define _(String) gettext(String)

/* #define MAPDB_SQLITE */

#ifdef MAPDB_SQLITE
#include "sqlite3.h"
#endif

// Latitude and longitude string formats.
#define CONVERT_HP_NORMAL       0
#define CONVERT_HP_NOSP         1
#define CONVERT_LP_NORMAL       2
#define CONVERT_LP_NOSP         3
#define CONVERT_DEC_DEG         4
#define CONVERT_UP_TRK          5
#define CONVERT_DMS_NORMAL      6
#define CONVERT_VHP_NOSP        7
#define CONVERT_DMS_NORMAL_FORMATED      8
#define CONVERT_HP_NORMAL_FORMATED       9
#define CONVERT_DEC_DEG_N       10

#define MAX_DEVICE_BUFFER       4096

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
    REPOTYPE_WMS        /* "service=wms" */
} RepoType;

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
    CUSTOM_ACTION_TOGGLE_LAYERS,
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
#ifdef INCLUDE_APRS
    COLORABLE_APRS_STATION,
#endif // INCLUDE_APRS
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
    UK_OSGB,
    UK_NGR, // 8 char grid ref
    UK_NGR6,// 6 char grid ref
    IARU_LOC,
    DEG_FORMAT_ENUM_COUNT
} DegFormat;

typedef struct _CoordFormatSetup CoordFormatSetup;
struct _CoordFormatSetup 
{
	gchar    *name;
	gchar    *short_field_1;
	gchar    *long_field_1;
	gchar    *short_field_2;
	gchar    *long_field_2;
	gboolean field_2_in_use;
} _CoordFormatSetup;

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
    RepoData *layers;
    gint8 layer_level;
    gboolean layer_enabled;
    gboolean layer_was_enabled; /* needed for ability to temporarily toggle layers on and off */
    gint layer_refresh_interval;
    gint layer_refresh_countdown;
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
    gint8 layer_level;
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


#ifdef INCLUDE_APRS

// --------------------------------------------------------------------------------------
// Start of APRS Types - Code taken from Xastir code 25 March 2008 by Rob Williams
// Modification made to fit in with Maemo mapper
// --------------------------------------------------------------------------------------

typedef struct _TWriteBuffer TWriteBuffer;
struct _TWriteBuffer
{
    int    write_in_pos;                          /* current write buffer input pos          */
    int    write_out_pos;                         /* current write buffer output pos         */
    GMutex* write_lock;                      /* Lock for writing the port data          */
    char   device_write_buffer[MAX_DEVICE_BUFFER];/* write buffer for this port              */
    int    errors;
};


typedef enum
{
	TNC_CONNECTION_BT,
	TNC_CONNECTION_FILE
} TTncConnection;

typedef enum
{
	APRS_PORT_INET,
	APRS_PORT_TTY,
	APRS_PORT_COUNT
} TAprsPort;

// We should probably be using APRS_DF in extract_bearing_NRQ()
// and extract_omnidf() functions.  We aren't currently.
/* Define APRS Types */
enum APRS_Types {
    APRS_NULL,
    APRS_MSGCAP,
    APRS_FIXED,
    APRS_DOWN,      // Not used anymore
    APRS_MOBILE,
    APRS_DF,
    APRS_OBJECT,
    APRS_ITEM,
    APRS_STATUS,
    APRS_WX1,
    APRS_WX2,
    APRS_WX3,
    APRS_WX4,
    APRS_WX5,
    APRS_WX6,
    QM_WX,
    PEET_COMPLETE,
    RSWX200,
    GPS_RMC,
    GPS_GGA,
    GPS_GLL,
    STATION_CALL_DATA,
    OTHER_DATA,
    APRS_MICE,
    APRS_GRID,
    DALLAS_ONE_WIRE,
    DAVISMETEO
};


/* Define Record Types */
#define NORMAL_APRS     'N'
#define MOBILE_APRS     'M'
#define DF_APRS         'D'
#define DOWN_APRS       'Q'
#define NORMAL_GPS_RMC  'C'
#define NORMAL_GPS_GGA  'A'
#define NORMAL_GPS_GLL  'L'

/* define RECORD ACTIVES */
#define RECORD_ACTIVE    'A'
#define RECORD_NOTACTIVE 'N'
#define RECORD_CLOSED     'C'

/* define data from info type */
#define DATA_VIA_LOCAL 'L'
#define DATA_VIA_TNC   'T'
#define DATA_VIA_NET   'I'
#define DATA_VIA_FILE  'F'


/* define Heard info type */
#define VIA_TNC         'Y'
#define NOT_VIA_TNC     'N'

/* define Message types */
#define MESSAGE_MESSAGE  'M'
#define MESSAGE_BULLETIN 'B'
#define MESSAGE_NWS      'W'


// trail flag definitions
#define MAX_CALLSIGN 9
#define MAX_TIME             20
#define MAX_LONG             12
#define MAX_LAT              11
#define MAX_ALTITUDE         10         //-32808.4 to 300000.0? feet
#define MAX_SPEED             9         /* ?? 3 in knots */
#define MAX_COURSE            7         /* ?? */
#define MAX_POWERGAIN         7
#define MAX_STATION_TIME     10         /* 6+1 */
#define MAX_SAT               4
#define MAX_DISTANCE         10
#define MAX_WXSTATION        50
#define MAX_TEMP            100
#define MAX_MULTIPOINTS 35

#define MAX_DEVICE_BUFFER 4096

/* define max size of info field */
#define MAX_INFO_FIELD_SIZE 256

// Number of times to send killed objects/items before ceasing to
// transmit them.
#define MAX_KILLED_OBJECT_RETRANSMIT 20

// Check entire station list at this rate for objects/items that
// might need to be transmitted via the decaying algorithm.  This is
// the start rate, which gets doubled on each transmit.
#define OBJECT_CHECK_RATE 20


#define MAX_MESSAGE_LENGTH  100
#define MAX_MESSAGE_ORDER    10


#define CHECKMALLOC(m)  if (!m) { fprintf(stderr, "***** Malloc Failed *****\n"); exit(0); }


#define STATION_REMOVE_CYCLE 300    /* check station remove in seconds (every 5 minutes) */
#define MESSAGE_REMOVE_CYCLE 600    /* check message remove in seconds (every 10 minutes) */
#define IN_VIEW_MIN         600l    /* margin for off-screen stations, with possible trails on screen, in minutes */
#define TRAIL_POINT_MARGIN   30l    /* margin for off-screen trails points, for segment to be drawn, in minutes */
#define TRAIL_MAX_SPEED      900    /* max. acceptible speed for drawing trails, in mph */
#define MY_TRAIL_COLOR      0x16    /* trail color index reserved for my station */
#define TRAIL_ECHO_TIME       30    /* check for delayed echos during last 30 minutes */
/* MY_TRAIL_DIFF_COLOR changed to user configurable my_trail_diff_color  */


typedef struct { 
    int digis;
    int wxs;
    int other_mobiles;
    int mobiles_in_motion;
    int homes;
    int total;
} aloha_stats;


// station flag definitions.  We have 16 bits available here as
// "flag" in "DataRow" is defined as a short.
//
#define ST_OBJECT       0x01    // station is an object
#define ST_ITEM         0x02    // station is an item
#define ST_ACTIVE       0x04    // station is active (deleted objects are
                                // inactive)
#define ST_MOVING       0x08    // station is moving
#define ST_DIRECT       0x10    // heard direct (not via digis)
#define ST_VIATNC       0x20    // station heard via TNC
#define ST_3RD_PT       0x40    // third party traffic (not used yet)
#define ST_MSGCAP       0x80    // message capable (not used yet)
#define ST_STATUS       0x100   // got real status message
#define ST_INVIEW       0x200   // station is in current screen view
#define ST_MYSTATION    0x400   // station is owned by my call-SSID
#define ST_MYOBJITEM    0x800   // object/item owned by me


#define TR_LOCAL        0x01    // heard direct (not via digis)
#define TR_NEWTRK       0x02    // start new track


enum AprsAreaObjectTypes {
    AREA_OPEN_CIRCLE     = 0x0,
    AREA_LINE_LEFT       = 0x1,
    AREA_OPEN_ELLIPSE    = 0x2,
    AREA_OPEN_TRIANGLE   = 0x3,
    AREA_OPEN_BOX        = 0x4,
    AREA_FILLED_CIRCLE   = 0x5,
    AREA_LINE_RIGHT      = 0x6,
    AREA_FILLED_ELLIPSE  = 0x7,
    AREA_FILLED_TRIANGLE = 0x8,
    AREA_FILLED_BOX      = 0x9,
    AREA_MAX             = 0x9,
    AREA_NONE            = 0xF
};



enum AprsAreaObjectColors {
    AREA_BLACK_HI  = 0x0,
    AREA_BLUE_HI   = 0x1,
    AREA_GREEN_HI  = 0x2,
    AREA_CYAN_HI   = 0x3,
    AREA_RED_HI    = 0x4,
    AREA_VIOLET_HI = 0x5,
    AREA_YELLOW_HI = 0x6,
    AREA_GRAY_HI   = 0x7,
    AREA_BLACK_LO  = 0x8,
    AREA_BLUE_LO   = 0x9,
    AREA_GREEN_LO  = 0xA,
    AREA_CYAN_LO   = 0xB,
    AREA_RED_LO    = 0xC,
    AREA_VIOLET_LO = 0xD,
    AREA_YELLOW_LO = 0xE,
    AREA_GRAY_LO   = 0xF
};


typedef struct {
    unsigned type : 4;
    unsigned color : 4;
    unsigned sqrt_lat_off : 8;
    unsigned sqrt_lon_off : 8;
    unsigned corridor_width : 16;
} AprsAreaObject;

typedef struct {
    char aprs_type;
    char aprs_symbol;
    char special_overlay;
    AprsAreaObject area_object;
} APRS_Symbol;

// Struct for holding track data.  Keeps a dynamically allocated
// doubly-linked list of track points.  The first record should have its
// "prev" pointer set to NULL and the last record should have its "next"
// pointer set to NULL.  If no track storage exists then the pointers to
// these structs in the DataRow struct should be NULL.
typedef struct _AprsTrackRow{
    long    trail_long_pos;     // coordinate of trail point
    long    trail_lat_pos;      // coordinate of trail point
    time_t  sec;                // date/time of position
    long    speed;              // in 0.1 km/h   undefined: -1
    int     course;             // in degrees    undefined: -1
    long    altitude;           // in 0.1 m      undefined: -99999
    char    flag;               // several flags, see below
    struct  _AprsTrackRow *prev;    // pointer to previous record in list
    struct  _AprsTrackRow *next;    // pointer to next record in list
} AprsTrackRow;



// Struct for holding current weather data.
// This struct is pointed to by the DataRow structure.
// An empty string indicates undefined data.
typedef struct {                //                      strlen
    time_t  wx_sec_time;
    int     wx_storm;           // Set to one if severe storm
    char    wx_time[MAX_TIME];
    char    wx_course[4];       // in °                     3
    char    wx_speed[4];        // in mph                   3
    time_t  wx_speed_sec_time;
    char    wx_gust[4];         // in mph                   3
    char    wx_hurricane_radius[4];  //nautical miles       3
    char    wx_trop_storm_radius[4]; //nautical miles       3
    char    wx_whole_gale_radius[4]; // nautical miles      3
    char    wx_temp[5];         // in °F                    3
    char    wx_rain[10];        // in hundredths inch/h     3
    char    wx_rain_total[10];  // in hundredths inch
    char    wx_snow[6];         // in inches/24h            3
    char    wx_prec_24[10];     // in hundredths inch/day   3
    char    wx_prec_00[10];     // in hundredths inch       3
    char    wx_hum[5];          // in %                     3
    char    wx_baro[10];        // in hPa                   6
    char    wx_fuel_temp[5];    // in °F                    3
    char    wx_fuel_moisture[5];// in %                     2
    char    wx_type;
    char    wx_station[MAX_WXSTATION];
} AprsWeatherRow;


// Struct for holding comment/status data.  Will keep a dynamically
// allocated list of text.  Every different comment field will be
// stored in a separate line.
typedef struct _AprsCommentRow{
    char   *text_ptr;           // Ptr to the comment text
    time_t sec_heard;           // Latest timestamp for this comment/status
    struct _AprsCommentRow *next;   // Ptr to next record or NULL
} AprsCommentRow;




// Struct for holding multipoint data.
typedef struct _AprsMultipointRow{
    long multipoints[MAX_MULTIPOINTS][2];
} AprsMultipointRow;

typedef struct _AprsDisplayData
{
    gchar *call_sign; // call sign or name index or object/item
                                    // name
    gdouble coord_lon;
    gdouble coord_lat;

    gchar   *coord_lat_lon;
    gchar   *path;
    
    gchar   *comment;
    gchar   *status;
} AprsDisplayData;

typedef struct _AprsDataRow {

    struct _AprsDataRow *n_next;    // pointer to next element in name ordered list
    struct _AprsDataRow *n_prev;    // pointer to previous element in name ordered
                                // list
    struct _AprsDataRow *t_newer;   // pointer to next element in time ordered
                                // list (newer)
    struct _AprsDataRow *t_older;   // pointer to previous element in time ordered
                                // list (older)

    
    char call_sign[MAX_CALLSIGN+1]; // call sign or name index or object/item
                                    // name
    char *tactical_call_sign;   // Tactical callsign.  NULL if not assigned
    APRS_Symbol aprs_symbol;
    long coord_lon;             // Xastir coordinates 1/100 sec, 0 = 180°W
    long coord_lat;             // Xastir coordinates 1/100 sec, 0 =  90°N

    int  time_sn;               // serial number for making time index unique
    time_t sec_heard;           // time last heard, used also for time index
    time_t heard_via_tnc_last_time;
    time_t direct_heard;        // KC2ELS - time last heard direct

// Change into time_t structs?  It'd save us a bunch of space.
    char packet_time[MAX_TIME];
    char pos_time[MAX_TIME];

    short flag;                 // several flags, see below
    char pos_amb;               // Position ambiguity, 0 = none,
                                // 1 = 0.1 minute...

    unsigned int error_ellipse_radius; // Degrades precision for this
                                // station, from 0 to 65535 cm or
                                // 655.35 meters.  Assigned when we
                                // decode each type of packet.
                                // Default is 6.0 meters (600 cm)
                                // unless we know the GPS position
                                // is augmented, or is degraded by
                                // less precision in the packet.

    unsigned int lat_precision;	// In 100ths of a second latitude
    unsigned int lon_precision;	// In 100ths of a second longitude

    int trail_color;            // trail color (when assigned)
    char record_type;
    //char data_via;              // L local, T TNC, I internet, F file

// Change to char's to save space?
    int  heard_via_tnc_port;    // Current this will always be 0, but keep for future
    TAprsPort  last_port_heard;       // Current this will always be 0, but keep for future
    unsigned int  num_packets;
    char *node_path_ptr;        // Pointer to path string
    char altitude[MAX_ALTITUDE]; // in meters (feet gives better resolution ??)
    char speed[MAX_SPEED+1];    // in knots (same as nautical miles/hour)
    char course[MAX_COURSE+1];
    char bearing[MAX_COURSE+1];
    char NRQ[MAX_COURSE+1];
    char power_gain[MAX_POWERGAIN+1];   // Holds the phgd values
    char signal_gain[MAX_POWERGAIN+1];  // Holds the shgd values (for DF'ing)

    AprsWeatherRow *weather_data;   // Pointer to weather data or NULL
 
    AprsCommentRow *status_data;    // Ptr to status records or NULL
    AprsCommentRow *comment_data;   // Ptr to comment records or NULL

    // Below two pointers are NULL if only one position has been received
    AprsTrackRow *oldest_trackpoint; // Pointer to oldest track point in
                                 // doubly-linked list
    AprsTrackRow *newest_trackpoint; // Pointer to newest track point in
                                 // doubly-linked list

    // When the station is an object, it can include coordinates
    // of related points. Currently these are being used to draw
    // outlines of NWS severe weather watches and warnings, and
    // storm regions. The coordinates are stored here in Xastir
    // coordinate form. Element [x][0] is the latitude, and 
    // element [x][1] is the longitude.  --KG4NBB
    //
    // Is there anything preventing a multipoint string from being
    // in other types of packets, in the comment field?  --WE7U
    //
    int num_multipoints;
    char type;      // from '0' to '9'
    char style;     // from 'a' to 'z'
    AprsMultipointRow *multipoint_data;


///////////////////////////////////////////////////////////////////////
// Optional stuff for Objects/Items only (I think, needs to be
// checked).  These could be moved into an ObjectRow structure, with
// only a NULL pointer here if not an object/item.
///////////////////////////////////////////////////////////////////////
 
    char origin[MAX_CALLSIGN+1]; // call sign originating an object
    short object_retransmit;     // Number of times to retransmit object.
                                 // -1 = forever
                                 // Used currently to stop sending killed
                                 // objects.
    time_t last_transmit_time;   // Time we last transmitted an object/item.
                                 // Used to implement decaying transmit time
                                 // algorithm
    short transmit_time_increment; // Seconds to add to transmit next time
                                   // around.  Used to implement decaying
                                   // transmit time algorithm
//    time_t last_modified_time;   // Seconds since the object/item
                                 // was last modified.  We'll
                                 // eventually use this for
                                 // dead-reckoning.
    char signpost[5+1];          // Holds signpost data
    int  df_color;
    char sats_visible[MAX_SAT];
    char probability_min[10+1];  // Holds prob_min (miles)
    char probability_max[10+1];  // Holds prob_max (miles)

} AprsDataRow;

typedef enum
{
    APRSPOI_SELECTED,
    APRSPOI_CALLSIGN,
//    APRSPOI_LAT,
//    APRSPOI_LON,
//    APRSPOI_LATLON,
//    APRSPOI_BEARING,
//    APRSPOI_DISTANCE,
//    APRSPOI_PATH,
//    APRSPOI_COMMENT,
//    APRSPOI_STATUS,
    APRSPOI_NUM_COLUMNS
} APRSPOIList;



typedef struct _AprsStationList{
    struct  _AprsStationList *next;    // pointer to next record in list
    AprsDataRow *station;
} AprsStationList;




// --------------------------------------------------------------------------------------
// End of APRS Types - Code taken from Xastir code 25 March 2008 by Rob Williams M1BGT
// Modification made to fit in with Maemo mapper
// --------------------------------------------------------------------------------------
#endif // INCLUDE_APRS


#endif /* ifndef MAEMO_MAPPER_TYPES_H */
