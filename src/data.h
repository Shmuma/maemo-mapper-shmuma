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

#ifndef MAEMO_MAPPER_DATA_H
#define MAEMO_MAPPER_DATA_H

#ifdef HAVE_CONFIG_H
#    include "config.h"
#endif

#include <libosso.h>
#include "types.h"

/* Constants regarding enums and defaults. */
extern gchar *UNITS_ENUM_TEXT[UNITS_ENUM_COUNT];
extern gdouble UNITS_CONVERT[UNITS_ENUM_COUNT];
extern gchar *UNBLANK_ENUM_TEXT[UNBLANK_ENUM_COUNT];
extern gchar *INFO_FONT_ENUM_TEXT[INFO_FONT_ENUM_COUNT];
extern gchar *ROTATE_DIR_ENUM_TEXT[ROTATE_DIR_ENUM_COUNT];
extern gint ROTATE_DIR_ENUM_DEGREES[ROTATE_DIR_ENUM_COUNT];
extern gchar *CUSTOM_ACTION_ENUM_TEXT[CUSTOM_ACTION_ENUM_COUNT];
extern gchar *CUSTOM_KEY_GCONF[CUSTOM_KEY_ENUM_COUNT];
extern gchar *CUSTOM_KEY_ICON[CUSTOM_KEY_ENUM_COUNT];
extern CustomAction CUSTOM_KEY_DEFAULT[CUSTOM_KEY_ENUM_COUNT];
extern gchar *COLORABLE_GCONF[COLORABLE_ENUM_COUNT];
extern GdkColor COLORABLE_DEFAULT[COLORABLE_ENUM_COUNT];
extern CoordFormatSetup DEG_FORMAT_ENUM_TEXT[DEG_FORMAT_ENUM_COUNT];
extern gchar *SPEED_LOCATION_ENUM_TEXT[SPEED_LOCATION_ENUM_COUNT];
extern gchar *GPS_RCVR_ENUM_TEXT[GPS_RCVR_ENUM_COUNT];

/** The main GtkContainer of the application. */
extern GtkWidget *_window;

/** The main OSSO context of the application. */
extern osso_context_t *_osso;

/** The widget that provides the visual display of the map. */
extern GtkWidget *_map_widget;

/** The backing pixmap of _map_widget. */
extern GdkPixmap *_map_pixmap;

/** The backing pixmap of _map_widget. */
extern GdkPixbuf *_map_pixbuf;

/** The context menu for the map. */
extern GtkMenu *_map_cmenu;

extern gint _map_offset_devx;
extern gint _map_offset_devy;

extern gint _map_rotate_angle;
extern gfloat _map_rotate_matrix[4];
extern gfloat _map_reverse_matrix[4];

extern GtkWidget *_gps_widget;
extern GtkWidget *_text_lat;
extern GtkWidget *_text_lon;
extern GtkWidget *_text_speed;
extern GtkWidget *_text_alt;
extern GtkWidget *_sat_panel;
extern GtkWidget *_text_time;
extern GtkWidget *_heading_panel;

/** GPS data. */
extern Point _pos;
extern const Point _point_null;

extern GpsData _gps;
extern GpsSatelliteData _gps_sat[12];
extern gboolean _satdetails_on;

extern gboolean _is_first_time;


/** VARIABLES FOR MAINTAINING STATE OF THE CURRENT VIEW. */

/** The "zoom" level defines the resolution of a pixel, from 0 to MAX_ZOOM.
 * Each pixel in the current view is exactly (1 << _zoom) "units" wide. */
extern gint _zoom; /* zoom level, from 0 to MAX_ZOOM. */
extern Point _center; /* current center location, X. */

extern gint _next_zoom;
extern Point _next_center;
extern gint _next_map_rotate_angle;
extern GdkPixbuf *_redraw_wait_icon;
extern GdkRectangle _redraw_wait_bounds;

extern gint _map_correction_unitx;
extern gint _map_correction_unity;

/** CACHED SCREEN INFORMATION THAT IS DEPENDENT ON THE CURRENT VIEW. */
extern gint _view_width_pixels;
extern gint _view_height_pixels;
extern gint _view_halfwidth_pixels;
extern gint _view_halfheight_pixels;


/** The current track and route. */
extern Path _track;
extern Path _route;
extern gint _track_index_last_saved;

/** THE GdkGC OBJECTS USED FOR DRAWING. */
extern GdkGC *_gc[COLORABLE_ENUM_COUNT];
extern GdkColor _color[COLORABLE_ENUM_COUNT];

/** BANNERS. */
extern GtkWidget *_connect_banner;
extern GtkWidget *_fix_banner;
extern GtkWidget *_waypoint_banner;
extern GtkWidget *_download_banner;

/** DOWNLOAD PROGRESS. */
extern gboolean _conic_is_connected;
extern GMutex *_mapdb_mutex;
extern GMutex *_mouse_mutex;
extern volatile gint _num_downloads;
extern gint _curr_download;
extern GHashTable *_mut_exists_table;
extern GTree *_mut_priority_tree;
extern GMutex *_mut_priority_mutex;
extern GThreadPool *_mut_thread_pool;
extern GThreadPool *_mrt_thread_pool;
extern gboolean _refresh_map_after_download;

/** CONFIGURATION INFORMATION. */
extern GpsRcvrInfo _gri;
extern ConnState _gps_state;
extern gchar *_route_dir_uri;
extern gchar *_track_file_uri;
extern CenterMode _center_mode;
extern gboolean _center_rotate;
extern gboolean _fullscreen;
extern gboolean _enable_gps;
extern gboolean _enable_tracking;
extern gboolean _gps_info;
extern gchar *_route_dl_url;
extern gint _route_dl_radius;
extern gchar *_poi_dl_url;
extern gint _show_paths;
extern gboolean _show_zoomlevel;
extern gboolean _show_scale;
extern gboolean _show_comprose;
extern gboolean _show_velvec;
extern gboolean _show_poi;
extern gboolean _auto_download;
extern gint _auto_download_precache;
extern gint _lead_ratio;
extern gboolean _lead_is_fixed;
extern gint _center_ratio;
extern gint _draw_width;
extern gint _rotate_sens;
extern gint _ac_min_speed;
extern RotateDir _rotate_dir;
extern gboolean _enable_announce;
extern gint _announce_notice_ratio;
extern gboolean _enable_voice;
extern GSList *_loc_list;
extern GtkListStore *_loc_model;
extern UnitType _units;
extern CustomAction _action[CUSTOM_KEY_ENUM_COUNT];
extern gint _degformat;
extern gboolean _speed_limit_on;
extern gint _speed_limit;
extern gboolean _speed_excess;
extern SpeedLocation _speed_location;
extern UnblankOption _unblank_option;
extern InfoFontSize _info_font_size;
extern gboolean _save_full_gpx;

extern GList *_repo_list;
extern RepoData *_curr_repo;

/** POI */
extern gchar *_poi_db_filename;
extern gchar *_poi_db_dirname;
extern gint _poi_zoom;
extern gboolean _poi_enabled;

/** The singleton auto-route-download data. */
extern AutoRouteDownloadData _autoroute_data;


/*********************
 * BELOW: MENU ITEMS *
 *********************/

/* Menu items for the "Route" submenu. */
extern GtkWidget *_menu_route_open_item;
extern GtkWidget *_menu_route_download_item;
extern GtkWidget *_menu_route_save_item;
extern GtkWidget *_menu_route_distnext_item;
extern GtkWidget *_menu_route_distlast_item;
extern GtkWidget *_menu_route_reset_item;
extern GtkWidget *_menu_route_clear_item;

/* Menu items for the "Track" submenu. */
extern GtkWidget *_menu_track_open_item;
extern GtkWidget *_menu_track_save_item;
extern GtkWidget *_menu_track_insert_break_item;
extern GtkWidget *_menu_track_insert_mark_item;
extern GtkWidget *_menu_track_distlast_item;
extern GtkWidget *_menu_track_distfirst_item;
extern GtkWidget *_menu_track_clear_item;
extern GtkWidget *_menu_track_enable_tracking_item;

/* Menu items for the "POI" submenu. */
extern GtkWidget *_menu_poi_item;
extern GtkWidget *_menu_poi_import_item;
extern GtkWidget *_menu_poi_download_item;
extern GtkWidget *_menu_poi_browse_item;
extern GtkWidget *_menu_poi_categories_item;

/* Menu items for the "Maps" submenu. */
extern GtkWidget *_menu_maps_submenu;
extern GtkWidget *_menu_layers_submenu;
extern GtkWidget *_menu_maps_mapman_item;
extern GtkWidget *_menu_maps_auto_download_item;
extern GtkWidget *_menu_maps_repoman_item;
extern GtkWidget *_menu_maps_repodown_item;

/* Menu items for the "View" submenu. */
extern GtkWidget *_menu_view_zoom_in_item;
extern GtkWidget *_menu_view_zoom_out_item;

extern GtkWidget *_menu_view_rotate_clock_item;
extern GtkWidget *_menu_view_rotate_counter_item;
extern GtkWidget *_menu_view_rotate_reset_item;
extern GtkWidget *_menu_view_rotate_auto_item;

extern GtkWidget *_menu_view_pan_up_item;
extern GtkWidget *_menu_view_pan_down_item;
extern GtkWidget *_menu_view_pan_left_item;
extern GtkWidget *_menu_view_pan_right_item;
extern GtkWidget *_menu_view_pan_north_item;
extern GtkWidget *_menu_view_pan_south_item;
extern GtkWidget *_menu_view_pan_west_item;
extern GtkWidget *_menu_view_pan_east_item;

extern GtkWidget *_menu_view_fullscreen_item;

extern GtkWidget *_menu_view_show_zoomlevel_item;
extern GtkWidget *_menu_view_show_scale_item;
extern GtkWidget *_menu_view_show_comprose_item;
extern GtkWidget *_menu_view_show_routes_item;
extern GtkWidget *_menu_view_show_tracks_item;
extern GtkWidget *_menu_view_show_velvec_item;
extern GtkWidget *_menu_view_show_poi_item;

extern GtkWidget *_menu_view_ac_latlon_item;
extern GtkWidget *_menu_view_ac_lead_item;
extern GtkWidget *_menu_view_ac_none_item;

extern GtkWidget *_menu_view_goto_latlon_item;
extern GtkWidget *_menu_view_goto_address_item;
extern GtkWidget *_menu_view_goto_gps_item;
extern GtkWidget *_menu_view_goto_nextway_item;
extern GtkWidget *_menu_view_goto_nearpoi_item;

/* Menu items for the "GPS" submenu. */
extern GtkWidget *_menu_gps_enable_item;
extern GtkWidget *_menu_gps_show_info_item;
extern GtkWidget *_menu_gps_details_item;
extern GtkWidget *_menu_gps_reset_item;

/* Menu items for the other menu items. */
extern GtkWidget *_menu_settings_item;
extern GtkWidget *_menu_help_item;
extern GtkWidget *_menu_about_item;
extern GtkWidget *_menu_close_item;

/*********************
 * ABOVE: MENU ITEMS *
 *********************/


/*****************************
 * BELOW: CONTEXT MENU ITEMS *
 *****************************/

extern gboolean _mouse_is_dragging;
extern gboolean _mouse_is_down;
extern gint _cmenu_position_x;
extern gint _cmenu_position_y;

/* Menu items for the "Location" context menu. */
extern GtkWidget *_cmenu_loc_show_latlon_item;
extern GtkWidget *_cmenu_loc_route_to_item;
extern GtkWidget *_cmenu_loc_distance_to_item;
extern GtkWidget *_cmenu_loc_download_poi_item;
extern GtkWidget *_cmenu_loc_browse_poi_item;
extern GtkWidget *_cmenu_loc_add_route_item;
extern GtkWidget *_cmenu_loc_add_way_item;
extern GtkWidget *_cmenu_loc_add_poi_item;
extern GtkWidget *_cmenu_loc_set_gps_item;
extern GtkWidget *_cmenu_loc_apply_correction_item;

/* Menu items for the "Waypoint" context menu. */
extern GtkWidget *_cmenu_way_show_latlon_item;
extern GtkWidget *_cmenu_way_show_desc_item;
extern GtkWidget *_cmenu_way_clip_latlon_item;
extern GtkWidget *_cmenu_way_clip_desc_item;
extern GtkWidget *_cmenu_way_route_to_item;
extern GtkWidget *_cmenu_way_distance_to_item;
extern GtkWidget *_cmenu_way_delete_item;
extern GtkWidget *_cmenu_way_add_poi_item;
extern GtkWidget *_cmenu_way_goto_nextway_item;

/* Menu items for the "POI" context menu. */
extern GtkWidget *_cmenu_poi_submenu;
extern GtkWidget *_cmenu_poi_edit_poi_item;
extern GtkWidget *_cmenu_poi_route_to_item;
extern GtkWidget *_cmenu_poi_distance_to_item;
extern GtkWidget *_cmenu_poi_add_route_item;
extern GtkWidget *_cmenu_poi_add_way_item;
extern GtkWidget *_cmenu_poi_goto_nearpoi_item;

/*****************************
 * ABOVE: CONTEXT MENU ITEMS *
 *****************************/

#ifdef INCLUDE_APRS

extern gboolean _aprs_enable;
extern gboolean _aprs_inet_enable;
extern gboolean _aprs_tty_enable;
extern gchar *  _aprs_server;
extern gchar *  _aprs_tty_port;
extern guint     _aprs_server_port;
extern gint     _aprs_std_pos_hist;
extern gint     _aprs_max_stations;
extern gchar *  _aprs_inet_server_validation;
extern gchar *  _aprs_beacon_comment;
extern gboolean _aprs_transmit_compressed_posit;
extern gint     _aprs_inet_beacon_interval;
extern gint     _aprs_tty_beacon_interval;
extern gchar *  _aprs_inet_beacon_comment;
extern gchar    _aprs_beacon_group;
extern gchar    _aprs_beacon_symbol;

extern gboolean _aprs_enable_inet_tx;
extern gboolean _aprs_enable_tty_tx;
extern gchar *  _aprs_mycall;
extern gchar *  _aprs_beacon_comment;

extern gboolean _aprs_show_new_station_alert;
extern time_t   _aprs_sec_remove;
extern GtkWidget *_menu_enable_aprs_inet_item;
extern GtkWidget *_menu_enable_aprs_tty_item;
extern GtkWidget *_menu_list_aprs_stations_item;
extern GtkWidget *_menu_list_aprs_messages_item;
extern GtkWidget *_menu_aprs_settings_item;
extern ConnState _aprs_inet_state;
extern ConnState _aprs_tty_state;
extern gchar *  _aprs_tnc_bt_mac;
extern TTncConnection _aprs_tnc_method;
extern gint     _aprs_server_auto_filter_km;
extern gboolean _aprs_server_auto_filter_on;
//extern GtkWidget *_aprs_connect_banner;
extern gchar *  _aprs_unproto_path;

#endif // INCLUDE_APRS

#endif /* ifndef MAEMO_MAPPER_DATA_H */

