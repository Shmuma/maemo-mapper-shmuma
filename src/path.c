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

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <osso-helplib.h>
#include <bt-dbus.h>
#include <hildon-widgets/hildon-note.h>
#include <hildon-widgets/hildon-banner.h>
#include <hildon-widgets/hildon-system-sound.h>
#include <hildon-widgets/hildon-input-mode-hint.h>
#include <sqlite3.h>

#include "types.h"
#include "data.h"
#include "defines.h"

#include "display.h"
#include "gdk-pixbuf-rotate.h"
#include "gpx.h"
#include "main.h"
#include "path.h"
#include "poi.h"
#include "util.h"

typedef struct _OriginToggleInfo OriginToggleInfo;
struct _OriginToggleInfo {
    GtkWidget *rad_use_gps;
    GtkWidget *rad_use_route;
    GtkWidget *rad_use_text;
    GtkWidget *chk_avoid_highways;
    GtkWidget *chk_auto;
    GtkWidget *txt_from;
    GtkWidget *txt_to;
};

/* _near_point is the route point to which we are closest. */
static Point *_near_point = NULL;
static gint64 _near_point_dist_squared = INT64_MAX;

/* _next_way is what we currently interpret to be the next waypoint. */
static WayPoint *_next_way;
static gint64 _next_way_dist_squared = INT64_MAX;

/* _next_wpt is the route point immediately following _next_way. */
static Point *_next_wpt = NULL;
static gint64 _next_wpt_dist_squared = INT64_MAX;

static gfloat _initial_distance_from_waypoint = -1.f;
static WayPoint *_initial_distance_waypoint = NULL;

static sqlite3 *_path_db = NULL;
static sqlite3_stmt *_track_stmt_select = NULL;
static sqlite3_stmt *_track_stmt_delete_path = NULL;
static sqlite3_stmt *_track_stmt_delete_way = NULL;
static sqlite3_stmt *_track_stmt_insert_path = NULL;
static sqlite3_stmt *_track_stmt_insert_way = NULL;
static sqlite3_stmt *_route_stmt_select = NULL;
static sqlite3_stmt *_route_stmt_delete_path = NULL;
static sqlite3_stmt *_route_stmt_delete_way = NULL;
static sqlite3_stmt *_route_stmt_insert_path = NULL;
static sqlite3_stmt *_route_stmt_insert_way = NULL;
static sqlite3_stmt *_path_stmt_trans_begin = NULL;
static sqlite3_stmt *_path_stmt_trans_commit = NULL;
static sqlite3_stmt *_path_stmt_trans_rollback = NULL;

static gchar *_last_spoken_phrase;

void
path_resize(Path *path, gint size)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(path->head + size != path->cap)
    {
        Point *old_head = path->head;
        WayPoint *curr;
        path->head = g_renew(Point, old_head, size);
        path->cap = path->head + size;
        if(path->head != old_head)
        {
            path->tail = path->head + (path->tail - old_head);

            /* Adjust all of the waypoints. */
            for(curr = path->whead - 1; curr++ != path->wtail; )
                curr->point = path->head + (curr->point - old_head);
        }
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

void
path_wresize(Path *path, gint wsize)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(path->whead + wsize != path->wcap)
    {
        WayPoint *old_whead = path->whead;
        path->whead = g_renew(WayPoint, old_whead, wsize);
        path->wtail = path->whead + (path->wtail - old_whead);
        path->wcap = path->whead + wsize;
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

static void
read_path_from_db(Path *path, sqlite3_stmt *select_stmt)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    MACRO_PATH_INIT(*path);
    while(SQLITE_ROW == sqlite3_step(select_stmt))
    {
        const gchar *desc;

        MACRO_PATH_INCREMENT_TAIL(*path);
        path->tail->unitx = sqlite3_column_int(select_stmt, 0);
        path->tail->unity = sqlite3_column_int(select_stmt, 1);
        path->tail->time = sqlite3_column_int(select_stmt, 2);
        path->tail->altitude = sqlite3_column_int(select_stmt, 3);

        desc = sqlite3_column_text(select_stmt, 4);
        if(desc)
        {
            MACRO_PATH_INCREMENT_WTAIL(*path);
            path->wtail->point = path->tail;
            path->wtail->desc = g_strdup(desc);
        }
    }
    sqlite3_reset(select_stmt);

    /* If the last point isn't null, then add another null point. */
    if(path->tail->unity)
    {
        MACRO_PATH_INCREMENT_TAIL(*path);
        *path->tail = _point_null;
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

/* Returns the new next_update_index. */
static gint
write_path_to_db(Path *path,
        sqlite3_stmt *delete_path_stmt,
        sqlite3_stmt *delete_way_stmt,
        sqlite3_stmt *insert_path_stmt,
        sqlite3_stmt *insert_way_stmt,
        gint index_last_saved)
{
    Point *curr;
    WayPoint *wcurr;
    gint num;
    gboolean success = TRUE;
    printf("%s(%d)\n", __PRETTY_FUNCTION__, index_last_saved);

    /* Start transaction. */
    sqlite3_step(_path_stmt_trans_begin);
    sqlite3_reset(_path_stmt_trans_begin);

    if(index_last_saved == 0)
    {
        /* Replace the whole thing, so delete the table first. */
        if(SQLITE_DONE != sqlite3_step(delete_way_stmt)
                || SQLITE_DONE != sqlite3_step(delete_path_stmt))
        {
            gchar buffer[BUFFER_SIZE];
            snprintf(buffer, sizeof(buffer), "%s\n%s",
                    _("Failed to write to path database. "
                        "Tracks and routes may not be saved."),
                    sqlite3_errmsg(_path_db));
            popup_error(_window, buffer);
            success = FALSE;
        }
        sqlite3_reset(delete_way_stmt);
        sqlite3_reset(delete_path_stmt);
    }

    for(num = index_last_saved, curr = path->head + num, wcurr = path->whead;
            success && ++curr <= path->tail; ++num)
    {
        /* If this is the last point, and it is null, don't write it. */
        if(curr == path->tail && !curr->unity)
            break;

        /* Insert the path point. */
        if(SQLITE_OK != sqlite3_bind_int(insert_path_stmt, 1, curr->unitx)
        || SQLITE_OK != sqlite3_bind_int(insert_path_stmt, 2, curr->unity)
        || SQLITE_OK != sqlite3_bind_int(insert_path_stmt, 3, curr->time)
        || SQLITE_OK != sqlite3_bind_int(insert_path_stmt, 4, curr->altitude)
        || SQLITE_DONE != sqlite3_step(insert_path_stmt))
        {
            gchar buffer[BUFFER_SIZE];
            snprintf(buffer, sizeof(buffer), "%s\n%s",
                    _("Failed to write to path database. "
                        "Tracks and routes may not be saved."),
                    sqlite3_errmsg(_path_db));
            popup_error(_window, buffer);
            success = FALSE;
        }
        sqlite3_reset(insert_path_stmt);

        /* Now, check if curr is a waypoint. */
        if(success && wcurr <= path->wtail && wcurr->point == curr)
        {
            gint num = sqlite3_last_insert_rowid(_path_db);
            if(SQLITE_OK != sqlite3_bind_int(insert_way_stmt, 1, num)
            || SQLITE_OK != sqlite3_bind_text(insert_way_stmt, 2, wcurr->desc,
                -1, SQLITE_STATIC)
            || SQLITE_DONE != sqlite3_step(insert_way_stmt))
            {
                gchar buffer[BUFFER_SIZE];
                snprintf(buffer, sizeof(buffer), "%s\n%s",
                        _("Failed to write to path database. "
                            "Tracks and routes may not be saved."),
                        sqlite3_errmsg(_path_db));
                popup_error(_window, buffer);
                success = FALSE;
            }
            sqlite3_reset(insert_way_stmt);
            wcurr++;
        }
    }
    if(success)
    {
        sqlite3_step(_path_stmt_trans_commit);
        sqlite3_reset(_path_stmt_trans_commit);
        vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
        return num;
    }
    else
    {
        sqlite3_step(_path_stmt_trans_rollback);
        sqlite3_reset(_path_stmt_trans_rollback);
        vprintf("%s(): return 0\n", __PRETTY_FUNCTION__);
        return index_last_saved;
    }
}

void
path_save_route_to_db()
{
    if(_path_db)
    {
        write_path_to_db(&_route,
                          _route_stmt_delete_path,
                          _route_stmt_delete_way,
                          _route_stmt_insert_path,
                          _route_stmt_insert_way,
                          0);
    }
}

static void
path_save_track_to_db()
{
    if(_path_db)
    {
        write_path_to_db(&_track,
                          _track_stmt_delete_path,
                          _track_stmt_delete_way,
                          _track_stmt_insert_path,
                          _track_stmt_insert_way,
                          0);
    }
}

static void
path_update_track_in_db()
{
    if(_path_db)
    {
        _track_index_last_saved = write_path_to_db(&_track,
                          _track_stmt_delete_path,
                          _track_stmt_delete_way,
                          _track_stmt_insert_path,
                          _track_stmt_insert_way,
                          _track_index_last_saved);
    }
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
    gint64 near_dist_squared;
    printf("%s(%d)\n", __PRETTY_FUNCTION__, quick);

    /* If we have waypoints (_next_way != NULL), then determine the "next
     * waypoint", which is defined as the waypoint after the nearest point,
     * UNLESS we've passed that waypoint, in which case the waypoint after
     * that waypoint becomes the "next" waypoint. */
    if(_next_way)
    {
        /* First, set near_dist_squared with the new distance from
         * _near_point. */
        near = _near_point;
        near_dist_squared = DISTANCE_SQUARED(_pos, *near);

        /* Now, search _route for a closer point.  If quick is TRUE, then we'll
         * only search forward, only as long as we keep finding closer points.
         */
        for(curr = _near_point; curr++ != _route.tail; )
        {
            if(curr->unity)
            {
                gint64 dist_squared = DISTANCE_SQUARED(_pos, *curr);
                if(dist_squared <= near_dist_squared)
                {
                    near = curr;
                    near_dist_squared = dist_squared;
                }
                else if(quick)
                    break;
            }
        }

        /* Update _near_point. */
        _near_point = near;
        _near_point_dist_squared = near_dist_squared;

        for(wnext = wcurr = _next_way; wcurr < _route.wtail; wcurr++)
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
                     && (DISTANCE_SQUARED(_pos, *near) > _next_way_dist_squared
                      && DISTANCE_SQUARED(_pos, *_next_wpt)
                                                   < _next_wpt_dist_squared))))
            {
                wnext = wcurr + 1;
            }
            else
                break;
        }

        if(wnext == _route.wtail && (wnext->point < near
                || (wnext->point == near && quick
                    && (_next_wpt
                     && (DISTANCE_SQUARED(_pos, *near) > _next_way_dist_squared
                      &&DISTANCE_SQUARED(_pos, *_next_wpt)
                                                 < _next_wpt_dist_squared)))))
        {
            _next_way = NULL;
            _next_wpt = NULL;
            _next_way_dist_squared = INT64_MAX;
            _next_wpt_dist_squared = INT64_MAX;
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
            _next_way_dist_squared = DISTANCE_SQUARED(_pos, *wnext->point);
            if(_next_wpt)
                _next_wpt_dist_squared = DISTANCE_SQUARED(_pos, *_next_wpt);
        }
    }

    vprintf("%s(): return %d\n", __PRETTY_FUNCTION__, ret);
    return ret;
}

/**
 * Reset the _near_point data by searching the entire route for the nearest
 * route point and waypoint.
 */
void
route_find_nearest_point()
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    /* Initialize _near_point to first non-zero point. */
    _near_point = _route.head;
    while(!_near_point->unity && _near_point != _route.tail)
        _near_point++;

    /* Initialize _next_way. */
    if(_route.wtail < _route.whead
            || (_autoroute_data.enabled && _route.wtail == _route.whead))
        _next_way = NULL;
    else
        /* We have at least one waypoint. */
        _next_way = _autoroute_data.enabled ? _route.whead + 1 : _route.whead;
    _next_way_dist_squared = INT64_MAX;

    /* Initialize _next_wpt. */
    _next_wpt = NULL;
    _next_wpt_dist_squared = INT64_MAX;

    route_update_nears(FALSE);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

/**
 * Show the distance from the current GPS location to the given point,
 * following the route.  If point is NULL, then the distance is shown to the
 * next waypoint.
 */
gboolean
route_show_distance_to(Point *point)
{
    gchar buffer[80];
    gfloat lat1, lon1, lat2, lon2;
    gdouble sum = 0.0;
    printf("%s()\n", __PRETTY_FUNCTION__);

    /* If point is NULL, use the next waypoint. */
    if(point == NULL && _next_way)
        point = _next_way->point;

    /* If point is still NULL, return an error. */
    if(point == NULL)
    {
        printf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
        return FALSE;
    }

    unit2latlon(_pos.unitx, _pos.unity, lat1, lon1);
    if(point > _near_point)
    {
        Point *curr;
        /* Skip _near_point in case we have already passed it. */
        for(curr = _near_point + 1; curr <= point; ++curr)
        {
            if(curr->unity)
            {
                unit2latlon(curr->unitx, curr->unity, lat2, lon2);
                sum += calculate_distance(lat1, lon1, lat2, lon2);
                lat1 = lat2;
                lon1 = lon2;
            }
        }
    }
    else if(point < _near_point)
    {
        Point *curr;
        /* Skip _near_point in case we have already passed it. */
        for(curr = _near_point - 1; curr >= point; --curr)
        {
            if(curr->unity)
            {
                unit2latlon(curr->unitx, curr->unity, lat2, lon2);
                sum += calculate_distance(lat1, lon1, lat2, lon2);
                lat1 = lat2;
                lon1 = lon2;
            }
        }
    }
    else
    {
        /* Waypoint _is_ the nearest point. */
        unit2latlon(_near_point->unitx, _near_point->unity, lat2, lon2);
        sum += calculate_distance(lat1, lon1, lat2, lon2);
    }

    snprintf(buffer, sizeof(buffer), "%s: %.02f %s", _("Distance"),
            sum * UNITS_CONVERT[_units], UNITS_ENUM_TEXT[_units]);
    MACRO_BANNER_SHOW_INFO(_window, buffer);

    return TRUE;
    printf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
}

void
route_show_distance_to_next()
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(!route_show_distance_to(NULL))
    {
        MACRO_BANNER_SHOW_INFO(_window, _("There is no next waypoint."));
    }
    printf("%s(): return\n", __PRETTY_FUNCTION__);
}

void
route_show_distance_to_last()
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(_route.head != _route.tail)
    {
        /* Find last non-zero point. */
        Point *p;
        for(p = _route.tail; !p->unity; p--) { }
        route_show_distance_to(p);
    }
    else
    {
        MACRO_BANNER_SHOW_INFO(_window, _("The current route is empty."));
    }
    printf("%s(): return\n", __PRETTY_FUNCTION__);
}

static void
track_show_distance_from(Point *point)
{
    gchar buffer[80];
    gfloat lat1, lon1, lat2, lon2;
    gdouble sum = 0.0;
    Point *curr;
    unit2latlon(_pos.unitx, _pos.unity, lat1, lon1);

    /* Skip _track.tail because that should be _pos. */
    for(curr = _track.tail; curr > point; --curr)
    {
        if(curr->unity)
        {
            unit2latlon(curr->unitx, curr->unity, lat2, lon2);
            sum += calculate_distance(lat1, lon1, lat2, lon2);
            lat1 = lat2;
            lon1 = lon2;
        }
    }

    snprintf(buffer, sizeof(buffer), "%s: %.02f %s", _("Distance"),
            sum * UNITS_CONVERT[_units], UNITS_ENUM_TEXT[_units]);
    MACRO_BANNER_SHOW_INFO(_window, buffer);
}

void
track_show_distance_from_last()
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    /* Find last zero point. */
    if(_track.head != _track.tail)
    {
        Point *point;
        /* Find last zero point. */
        for(point = _track.tail; point->unity; point--) { }
        track_show_distance_from(point);
    }
    else
    {
        MACRO_BANNER_SHOW_INFO(_window, _("The current track is empty."));
    }
    printf("%s(): return\n", __PRETTY_FUNCTION__);
}

void
track_show_distance_from_first()
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    /* Find last zero point. */
    if(_track.head != _track.tail)
        track_show_distance_from(_track.head);
    else
    {
        MACRO_BANNER_SHOW_INFO(_window, _("The current track is empty."));
    }
    printf("%s(): return\n", __PRETTY_FUNCTION__);
}

static gboolean
route_download_and_setup(GtkWidget *parent, const gchar *source_url,
        const gchar *from, const gchar *to,
        gboolean avoid_highways, gint replace_policy)
{
    gchar *from_escaped;
    gchar *to_escaped;
    gchar *buffer;
    gchar *bytes;
    gint size;
    GnomeVFSResult vfs_result;
    printf("%s(%s, %s)\n", __PRETTY_FUNCTION__, from, to);

    from_escaped = gnome_vfs_escape_string(from);
    to_escaped = gnome_vfs_escape_string(to);
    buffer = g_strdup_printf(source_url, from_escaped, to_escaped);
    g_free(from_escaped);
    g_free(to_escaped);

    if(avoid_highways)
    {
        gchar *old = buffer;
        buffer = g_strconcat(old, "&avoid_highways=on", NULL);
        g_free(old);
    }

    /* Attempt to download the route from the server. */
    if(GNOME_VFS_OK != (vfs_result = gnome_vfs_read_entire_file(
                buffer, &size, &bytes)))
    {
        g_free(bytes);
        popup_error(parent, gnome_vfs_result_to_string(vfs_result));
        vprintf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
        return FALSE;
    }

    if(strncmp(bytes, "<?xml", strlen("<?xml")))
    {
        /* Not an XML document - must be bad locations. */
        popup_error(parent,
                _("Invalid source or destination."));
        g_free(bytes);
        vprintf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
        return FALSE;
    }
    /* Else, if GPS is enabled, replace the route, otherwise append it. */
    else if(gpx_path_parse(&_route, bytes, size, replace_policy))
    {
        path_save_route_to_db();

        /* Find the nearest route point, if we're connected. */
        route_find_nearest_point();

        map_force_redraw();

        MACRO_BANNER_SHOW_INFO(_window, _("Route Downloaded"));
        g_free(bytes);

        vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
        return TRUE;
    }
    popup_error(parent, _("Error parsing GPX file."));
    g_free(bytes);

    vprintf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
    return FALSE;
}

/**
 * This function is periodically run to download updated auto-route data
 * from the route source.
 */
static gboolean
auto_route_dl_idle()
{
    gchar latstr[32], lonstr[32], latlonstr[80];
    printf("%s(%f, %f, %s)\n", __PRETTY_FUNCTION__,
            _gps.lat, _gps.lon, _autoroute_data.dest);

    g_ascii_dtostr(latstr, 32, _gps.lat);
    g_ascii_dtostr(lonstr, 32, _gps.lon);
    snprintf(latlonstr, sizeof(latlonstr), "%s, %s", latstr, lonstr);

    route_download_and_setup(_window, _autoroute_data.source_url, latlonstr,
            _autoroute_data.dest, _autoroute_data.avoid_highways,0);

    _autoroute_data.in_progress = FALSE;

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return FALSE;
}

void
path_reset_route()
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    route_find_nearest_point();
    MACRO_MAP_RENDER_DATA();
    MACRO_QUEUE_DRAW_AREA();

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
}

/**
 * Add a point to the _track list.  This function is slightly overloaded,
 * since it is what houses the check for "have we moved
 * significantly": it also initiates the re-calculation of the _near_point
 * data, as well as calling osso_display_state_on() when we have the focus.
 *
 * If a non-zero time is given, then the current position (as taken from the
 * _pos variable) is appended to _track with the given time.  If time is zero,
 * then _point_null is appended to _track with time zero (this produces a
 * "break" in the track).
 */
gboolean
track_add(time_t time, gboolean newly_fixed)
{
    gboolean show_directions = TRUE;
    gint announce_thres_unsquared;
    gboolean ret = FALSE;
    printf("%s(%d, %d, %d, %d)\n", __PRETTY_FUNCTION__,
            (guint)time, newly_fixed, _pos.unitx, _pos.unity);

    if(!time)
    {
        /* This is a null point. */
        MACRO_PATH_INCREMENT_TAIL(_track);
        *_track.tail = _point_null;
    }
    else
    {
        gboolean moving = FALSE;
        gboolean approaching_waypoint = FALSE;

        announce_thres_unsquared = (20+_gps.speed) * _announce_notice_ratio*32;

        if(!_track.tail->unity || ((_pos.unitx - _track.tail->unitx)
                    * (_pos.unitx - _track.tail->unitx))
                + ((_pos.unity - _track.tail->unity)
                    * (_pos.unity - _track.tail->unity))
                > (_draw_width * _draw_width))
        {
            ret = TRUE;
            /* Update the nearest-waypoint data. */
            if(_route.head != _route.tail
                    && (newly_fixed ? (route_find_nearest_point(), TRUE)
                                    : route_update_nears(TRUE)))
            {
                /* Nearest waypoint has changed - re-render paths. */
                map_render_paths();
                MACRO_QUEUE_DRAW_AREA();
            }
            if(_show_paths & TRACKS_MASK)
            {
                /* Instead of calling map_render_paths(), we'll draw the new
                 * line ourselves and call gtk_widget_queue_draw_area(). */
                gint tx1, ty1, tx2, ty2;
                map_render_segment(_gc[COLORABLE_TRACK],
                        _gc[COLORABLE_TRACK_BREAK],
                        _track.tail->unitx, _track.tail->unity,
                        _pos.unitx, _pos.unity);
                if(_track.tail->unity)
                {
                    unit2screen(_track.tail->unitx, _track.tail->unity,
                            tx1, ty1);
                    unit2screen(_pos.unitx, _pos.unity, tx2, ty2);
                    gtk_widget_queue_draw_area(_map_widget,
                            MIN(tx1, tx2) - _draw_width,
                            MIN(ty1, ty2) - _draw_width,
                            abs(tx1 - tx2) + (2 * _draw_width),
                            abs(ty1 - ty2) + (2 * _draw_width));
                }
            }
            MACRO_PATH_INCREMENT_TAIL(_track);

            *_track.tail = _pos;

            if(_near_point_dist_squared > (2000 * 2000))
            {
                /* Prevent announcments from occurring. */
                announce_thres_unsquared = INT_MAX;

                if(_autoroute_data.enabled && !_autoroute_data.in_progress)
                {
                    MACRO_BANNER_SHOW_INFO(_window,
                            _("Recalculating directions..."));
                    _autoroute_data.in_progress = TRUE;
                    show_directions = FALSE;
                    g_idle_add((GSourceFunc)auto_route_dl_idle, NULL);
                }
                else
                {
                    /* Reset the route to try and find the nearest point. */
                    path_reset_route();
                }
            }

            /* Keep the display on. */
            moving = TRUE;
        }

        if(_initial_distance_waypoint
               && (_next_way != _initial_distance_waypoint
               ||  _next_way_dist_squared > (_initial_distance_from_waypoint
                                           * _initial_distance_from_waypoint)))
        {
            /* We've moved on to the next waypoint, or we're really far from
             * the current waypoint. */
            if(_waypoint_banner)
            {
                gtk_widget_destroy(_waypoint_banner);
                _waypoint_banner = NULL;
            }
            _initial_distance_from_waypoint = -1.f;
            _initial_distance_waypoint = NULL;
        }

        /* Check if we should announce upcoming waypoints. */
        if(_initial_distance_waypoint || _next_way_dist_squared
                    < (announce_thres_unsquared * announce_thres_unsquared))
        {
            if(show_directions)
            {
                if(!_initial_distance_waypoint)
                {
                    /* First time we're close enough to this waypoint. */
                    if(_enable_voice
                            /* And that we haven't already announced it. */
                            && strcmp(_next_way->desc, _last_spoken_phrase))
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
                            printf("%s %s\n", _voice_synth_path,
                                    _last_spoken_phrase);
                            execl(_voice_synth_path, _voice_synth_path,
                                    "-t", _last_spoken_phrase, (char *)NULL);
                            exit(0);
                        }
                    }
                    _initial_distance_from_waypoint
                        = sqrtf(_next_way_dist_squared);
                    _initial_distance_waypoint = _next_way;
                    if(_next_wpt && _next_wpt->unity != 0)
                    {
                        /* Create a banner for us the show progress. */
                        _waypoint_banner = hildon_banner_show_progress(
                                _window, NULL, _next_way->desc);
                    }
                    else
                    {
                        /* This is the last point in a segment, i.e.
                         * "Arrive at ..." - just announce. */
                        MACRO_BANNER_SHOW_INFO(_window, _next_way->desc);
                    }
                }
                else if(_waypoint_banner);
                {
                    /* We're already close to this waypoint. */
                    gdouble fraction = 1.f - (sqrtf(_next_way_dist_squared)
                            / _initial_distance_from_waypoint);
                    BOUND(fraction, 0.f, 1.f);
                    hildon_banner_set_fraction(
                            HILDON_BANNER(_waypoint_banner), fraction);
                }
            }
            approaching_waypoint = TRUE;
        }
        else if(_next_way_dist_squared > 2 * (_initial_distance_from_waypoint
                                            * _initial_distance_from_waypoint))
        {
            /* We're too far away now - destroy the banner. */
        }

        UNBLANK_SCREEN(moving, approaching_waypoint);
    }

    /* Maybe update the track database. */
    {
        static time_t last_track_db_update = 0;
        if(!time || (time - last_track_db_update > 60
                && _track.tail - _track.head + 1 > _track_index_last_saved))
        {
            path_update_track_in_db();
            last_track_db_update = time;
        }
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return ret;
}

void
track_clear()
{
    GtkWidget *confirm;

    confirm = hildon_note_new_confirmation(GTK_WINDOW(_window),
                            _("Really clear the track?"));

    if(GTK_RESPONSE_OK == gtk_dialog_run(GTK_DIALOG(confirm))) {
        MACRO_PATH_FREE(_track);
        MACRO_PATH_INIT(_track);
        path_save_track_to_db();
        map_force_redraw();
    }

    gtk_widget_destroy(confirm);
}

void
track_insert_break(gboolean temporary)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(_track.tail->unity)
    {
        gint x1, y1;
        unit2screen(_track.tail->unitx, _track.tail->unity, x1, y1);

        MACRO_PATH_INCREMENT_TAIL(_track);
        *_track.tail = _point_null;

        /* To mark a "waypoint" in a track, we'll add a (0, 0) point and then
         * another instance of the most recent track point. */
        if(temporary)
        {
            MACRO_PATH_INCREMENT_TAIL(_track);
            *_track.tail = _track.tail[-2];
        }

        /** Instead of calling map_render_paths(), we'll just add the waypoint
         * ourselves. */
        /* Make sure this circle will be visible. */
        if((x1 < _screen_width_pixels) && (y1 < _screen_height_pixels))
            gdk_draw_arc(_map_pixmap, _gc[COLORABLE_TRACK_BREAK],
                    FALSE, /* FALSE: not filled. */
                    x1 - _draw_width,
                    y1 - _draw_width,
                    2 * _draw_width,
                    2 * _draw_width,
                    0, /* start at 0 degrees. */
                    360 * 64);
    }
    else if(!temporary)
    {
        MACRO_BANNER_SHOW_INFO(_window, _("Break already inserted."));
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

/**
 * Cancel the current auto-route.
 */
void
cancel_autoroute()
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(_autoroute_data.enabled)
    {
        _autoroute_data.enabled = FALSE;
        g_free(_autoroute_data.source_url);
        _autoroute_data.source_url = NULL;
        g_free(_autoroute_data.dest);
        _autoroute_data.dest = NULL;
        _autoroute_data.in_progress = FALSE;
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

WayPoint *
find_nearest_waypoint(gint unitx, gint unity)
{
    WayPoint *wcurr;
    WayPoint *wnear;
    gint64 nearest_squared;
    Point pos = { unitx, unity, 0, INT_MIN };
    printf("%s()\n", __PRETTY_FUNCTION__);

    wcurr = wnear = _route.whead;
    if(wcurr && wcurr <= _route.wtail)
    {
        nearest_squared = DISTANCE_SQUARED(pos, *(wcurr->point));

        wnear = _route.whead;
        while(++wcurr <=  _route.wtail)
        {
            gint64 test_squared = DISTANCE_SQUARED(pos, *(wcurr->point));
            if(test_squared < nearest_squared)
            {
                wnear = wcurr;
                nearest_squared = test_squared;
            }
        }

        /* Only use the waypoint if it is within a 6*_draw_width square drawn
         * around the position. This is consistent with select_poi(). */
        if(abs(unitx - wnear->point->unitx) < pixel2unit(3 * _draw_width)
            && abs(unity - wnear->point->unity) < pixel2unit(3 * _draw_width))
            return wnear;
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return NULL;
}

static gboolean
origin_type_selected(GtkWidget *toggle,
        OriginToggleInfo *oti)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle)))
    {
        gtk_widget_set_sensitive(oti->txt_from, toggle == oti->rad_use_text);
        gtk_widget_set_sensitive(oti->chk_auto, toggle == oti->rad_use_gps);
    }
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
gboolean
route_download(gchar *to)
{
    static GtkWidget *dialog = NULL;
    static GtkWidget *table = NULL;
    static GtkWidget *label = NULL;
    static GtkWidget *txt_source_url = NULL;
    static OriginToggleInfo oti;
    printf("%s()\n", __PRETTY_FUNCTION__);

    conic_recommend_connected();

    if(dialog == NULL)
    {
        GtkEntryCompletion *from_comp;
        GtkEntryCompletion *to_comp;
        dialog = gtk_dialog_new_with_buttons(_("Download Route"),
                GTK_WINDOW(_window), GTK_DIALOG_MODAL,
                GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                NULL);

        /* Enable the help button. */
        ossohelp_dialog_help_enable(
                GTK_DIALOG(dialog), HELP_ID_DOWNROUTE, _osso);

        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
                table = gtk_table_new(2, 5, FALSE), TRUE, TRUE, 0);

        /* Source URL. */
        gtk_table_attach(GTK_TABLE(table),
                label = gtk_label_new(_("Source URL")),
                0, 1, 0, 1, GTK_FILL, 0, 2, 4);
        gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
        gtk_table_attach(GTK_TABLE(table),
                txt_source_url = gtk_entry_new(),
                1, 4, 0, 1, GTK_EXPAND | GTK_FILL, 0, 2, 4);
        gtk_entry_set_width_chars(GTK_ENTRY(txt_source_url), 25);

        /* Use GPS Location. */
        gtk_table_attach(GTK_TABLE(table),
                oti.rad_use_gps = gtk_radio_button_new_with_label(NULL,
                    _("Use GPS Location")),
                0, 2, 1, 2, GTK_FILL, 0, 2, 4);

        /* Use End of Route. */
        gtk_table_attach(GTK_TABLE(table),
               oti.rad_use_route = gtk_radio_button_new_with_label_from_widget(
                   GTK_RADIO_BUTTON(oti.rad_use_gps), _("Use End of Route")),
               0, 2, 2, 3, GTK_FILL, 0, 2, 4);

        gtk_table_attach(GTK_TABLE(table),
                gtk_vseparator_new(),
                2, 3, 1, 3, GTK_FILL, GTK_FILL, 2,4);

        /* Auto. */
        gtk_table_attach(GTK_TABLE(table),
                oti.chk_auto = gtk_check_button_new_with_label(
                    _("Auto-Update")),
                3, 4, 1, 2, GTK_FILL, 0, 2, 4);

        /* Avoid Highways. */
        gtk_table_attach(GTK_TABLE(table),
                oti.chk_avoid_highways = gtk_check_button_new_with_label(
                    _("Avoid Highways")),
                3, 4, 2, 3, GTK_FILL, 0, 2, 4);


        /* Origin. */
        gtk_table_attach(GTK_TABLE(table),
                oti.rad_use_text = gtk_radio_button_new_with_label_from_widget(
                    GTK_RADIO_BUTTON(oti.rad_use_gps), _("Origin")),
                0, 1, 3, 4, GTK_FILL, 0, 2, 4);
        gtk_table_attach(GTK_TABLE(table),
                oti.txt_from = gtk_entry_new(),
                1, 4, 3, 4, GTK_EXPAND | GTK_FILL, 0, 2, 4);
        gtk_entry_set_width_chars(GTK_ENTRY(oti.txt_from), 25);
        g_object_set(G_OBJECT(oti.txt_from), HILDON_AUTOCAP, FALSE, NULL);

        /* Destination. */
        gtk_table_attach(GTK_TABLE(table),
                label = gtk_label_new(_("Destination")),
                0, 1, 4, 5, GTK_FILL, 0, 2, 4);
        gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
        gtk_table_attach(GTK_TABLE(table),
                oti.txt_to = gtk_entry_new(),
                1, 4, 4, 5, GTK_EXPAND | GTK_FILL, 0, 2, 4);
        gtk_entry_set_width_chars(GTK_ENTRY(oti.txt_to), 25);
        g_object_set(G_OBJECT(oti.txt_to), HILDON_AUTOCAP, FALSE, NULL);


        /* Set up auto-completion. */
        from_comp = gtk_entry_completion_new();
        gtk_entry_completion_set_model(from_comp, GTK_TREE_MODEL(_loc_model));
        gtk_entry_completion_set_text_column(from_comp, 0);
        gtk_entry_set_completion(GTK_ENTRY(oti.txt_from), from_comp);

        to_comp = gtk_entry_completion_new();
        gtk_entry_completion_set_model(to_comp, GTK_TREE_MODEL(_loc_model));
        gtk_entry_completion_set_text_column(to_comp, 0);
        gtk_entry_set_completion(GTK_ENTRY(oti.txt_to), to_comp);


        g_signal_connect(G_OBJECT(oti.rad_use_gps), "toggled",
                          G_CALLBACK(origin_type_selected), &oti);
        g_signal_connect(G_OBJECT(oti.rad_use_route), "toggled",
                          G_CALLBACK(origin_type_selected), &oti);
        g_signal_connect(G_OBJECT(oti.rad_use_text), "toggled",
                          G_CALLBACK(origin_type_selected), &oti);

        //gtk_widget_set_sensitive(oti.chk_auto, FALSE);
    }

    /* Initialize fields. */

    gtk_entry_set_text(GTK_ENTRY(txt_source_url), _route_dl_url);
    if(to)
        gtk_entry_set_text(GTK_ENTRY(oti.txt_to), to);

    /* Use "End of Route" by default if they have a route. */
    if(_route.head != _route.tail)
    {
        /* There is no route, so make it the default. */
        gtk_widget_set_sensitive(oti.rad_use_route, TRUE);
        gtk_toggle_button_set_active(
                GTK_TOGGLE_BUTTON(oti.rad_use_route), TRUE);
        gtk_widget_grab_focus(oti.rad_use_route);
    }
    /* Else use "GPS Location" if they have GPS enabled. */
    else
    {
        /* There is no route, so desensitize "Use End of Route." */
        gtk_widget_set_sensitive(oti.rad_use_route, FALSE);
        if(_enable_gps)
        {
            gtk_toggle_button_set_active(
                    GTK_TOGGLE_BUTTON(oti.rad_use_gps), TRUE);
            gtk_widget_grab_focus(oti.rad_use_gps);
        }
        /* Else use text. */
        else
        {
            gtk_toggle_button_set_active(
                    GTK_TOGGLE_BUTTON(oti.rad_use_text), TRUE);
            gtk_widget_grab_focus(oti.txt_from);
        }
    }

    gtk_widget_show_all(dialog);

    while(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(dialog)))
    {
        gchar buffer[BUFFER_SIZE];
        const gchar *source_url, *from, *to;
        gboolean avoid_highways;

        source_url = gtk_entry_get_text(GTK_ENTRY(txt_source_url));
        if(!strlen(source_url))
        {
            popup_error(dialog, _("Please specify a source URL."));
            continue;
        }
        else
        {
            g_free(_route_dl_url);
            _route_dl_url = g_strdup(source_url);
        }

        if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(oti.rad_use_gps)))
        {
            gchar strlat[32];
            gchar strlon[32];
            g_ascii_formatd(strlat, 32, "%.06f", _gps.lat);
            g_ascii_formatd(strlon, 32, "%.06f", _gps.lon);
            snprintf(buffer, sizeof(buffer), "%s, %s", strlat, strlon);
            from = buffer;
        }
        else if(gtk_toggle_button_get_active(
                    GTK_TOGGLE_BUTTON(oti.rad_use_route)))
        {
            gchar strlat[32];
            gchar strlon[32];
            Point *p;
            gfloat lat, lon;

            /* Use last non-zero route point. */
            for(p = _route.tail; !p->unity; p--) { }

            unit2latlon(p->unitx, p->unity, lat, lon);
            g_ascii_formatd(strlat, 32, "%.06f", lat);
            g_ascii_formatd(strlon, 32, "%.06f", lon);
            snprintf(buffer, sizeof(buffer), "%s, %s", strlat, strlon);
            from = buffer;
        }
        else
        {
            from = gtk_entry_get_text(GTK_ENTRY(oti.txt_from));
        }

        if(!strlen(from))
        {
            popup_error(dialog, _("Please specify a start location."));
            continue;
        }

        to = gtk_entry_get_text(GTK_ENTRY(oti.txt_to));
        if(!strlen(to))
        {
            popup_error(dialog, _("Please specify an end location."));
            continue;
        }

        avoid_highways = gtk_toggle_button_get_active(
                GTK_TOGGLE_BUTTON(oti.chk_avoid_highways));
        if(route_download_and_setup(dialog, source_url, from, to,
                gtk_toggle_button_get_active(
                    GTK_TOGGLE_BUTTON(oti.chk_avoid_highways)),
                (gtk_toggle_button_get_active(
                    GTK_TOGGLE_BUTTON(oti.rad_use_gps)) ? 0 : 1)))
        {
            GtkTreeIter iter;

            /* Cancel any autoroute that might be occurring. */
            cancel_autoroute();

            if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(oti.chk_auto)))
            {
                _autoroute_data.source_url = g_strdup(source_url);
                _autoroute_data.dest = g_strdup(to);
                _autoroute_data.avoid_highways = avoid_highways;
                _autoroute_data.enabled = TRUE;
            }

            /* Save Origin in Route Locations list if not from GPS. */
            if(gtk_toggle_button_get_active(
                        GTK_TOGGLE_BUTTON(oti.rad_use_text))
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

            /* Success! Get out of the while loop. */
            break;
        }
        /* else let them try again. */
    }

    gtk_widget_hide(dialog); /* Destroying causes a crash (!?!?!??!) */

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

void
route_add_way_dialog(gint unitx, gint unity)
{
    gfloat lat, lon;
    gchar tmp1[LL_FMT_LEN], tmp2[LL_FMT_LEN], *p_latlon;
    static GtkWidget *dialog = NULL;
    static GtkWidget *table = NULL;
    static GtkWidget *label = NULL;
    static GtkWidget *txt_scroll = NULL;
    static GtkWidget *txt_desc = NULL;
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(dialog == NULL)
    {
        dialog = gtk_dialog_new_with_buttons(_("Add Waypoint"),
                GTK_WINDOW(_window), GTK_DIALOG_MODAL,
                GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                NULL);

        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
                table = gtk_table_new(2, 2, FALSE), TRUE, TRUE, 0);

        gtk_table_attach(GTK_TABLE(table),
                label = gtk_label_new(_("Lat, Lon:")),
                0, 1, 0, 1, GTK_FILL, 0, 2, 4);
        gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);

        unit2latlon(unitx, unity, lat, lon);
        lat_format(lat, tmp1);
        lon_format(lon, tmp2);
        p_latlon = g_strdup_printf("%s, %s", tmp1, tmp2);
        gtk_table_attach(GTK_TABLE(table),
                label = gtk_label_new(p_latlon),
                1, 2, 0, 1, GTK_FILL, 0, 2, 4);
        gtk_misc_set_alignment(GTK_MISC(label), 0.0f, 0.5f);
        g_free(p_latlon);

        gtk_table_attach(GTK_TABLE(table),
                label = gtk_label_new(_("Description")),
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
        gtk_widget_set_size_request(GTK_WIDGET(txt_scroll), 400, 60);
    }

    gtk_text_buffer_set_text(
            gtk_text_view_get_buffer(GTK_TEXT_VIEW(txt_desc)), "", 0);

    gtk_widget_show_all(dialog);

    while(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(dialog)))
    {
        GtkTextBuffer *tbuf;
        GtkTextIter ti1, ti2;
        gchar *desc;

        tbuf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(txt_desc));
        gtk_text_buffer_get_iter_at_offset(tbuf, &ti1, 0);
        gtk_text_buffer_get_end_iter(tbuf, &ti2);
        desc = gtk_text_buffer_get_text(tbuf, &ti1, &ti2, TRUE);

        if(*desc)
        {
            /* There's a description.  Add a waypoint. */
            MACRO_PATH_INCREMENT_TAIL(_route);
            _route.tail->unitx = unitx;
            _route.tail->unity = unity;
            _route.tail->time = 0;
            _route.tail->altitude = 0;

            MACRO_PATH_INCREMENT_WTAIL(_route);
            _route.wtail->point = _route.tail;
            _route.wtail->desc
                = gtk_text_buffer_get_text(tbuf, &ti1, &ti2, TRUE);
        }
        else
        {
            GtkWidget *confirm;

            g_free(desc);

            confirm = hildon_note_new_confirmation(GTK_WINDOW(dialog),
                    _("Creating a \"waypoint\" with no description actually "
                        "adds a break point.  Is that what you want?"));

            if(GTK_RESPONSE_OK == gtk_dialog_run(GTK_DIALOG(confirm)))
            {
                /* There's no description.  Add a break by adding a (0, 0)
                 * point (if necessary), and then the ordinary route point. */
                if(_route.tail->unity)
                {
                    MACRO_PATH_INCREMENT_TAIL(_route);
                    *_route.tail = _point_null;
                }

                MACRO_PATH_INCREMENT_TAIL(_route);
                _route.tail->unitx = unitx;
                _route.tail->unity = unity;
                _route.tail->time = 0;
                _route.tail->altitude = 0;


                gtk_widget_destroy(confirm);
            }
            else
            {
                gtk_widget_destroy(confirm);
                continue;
            }
        }

        route_find_nearest_point();
        map_render_paths();
        MACRO_QUEUE_DRAW_AREA();
        break;
    }
    gtk_widget_hide(dialog);
    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

WayPoint*
path_get_next_way()
{
    return _next_way;
}

void
path_init()
{
    gchar *settings_dir;
    printf("%s()\n", __PRETTY_FUNCTION__);

    /* Initialize settings_dir. */
    settings_dir = gnome_vfs_expand_initial_tilde(CONFIG_DIR_NAME);
    g_mkdir_with_parents(settings_dir, 0700);

    /* Open path database. */
    {   
        gchar *path_db_file;

        path_db_file = gnome_vfs_uri_make_full_from_relative(
                settings_dir, CONFIG_PATH_DB_FILE);

        if(!path_db_file || SQLITE_OK != sqlite3_open(path_db_file, &_path_db)
        /* Open worked. Now create tables, failing if they already exist. */
        || (sqlite3_exec(_path_db,
                    "create table route_path ("
                    "num integer primary key, "
                    "unitx integer, "
                    "unity integer, "
                    "time integer, "
                    "altitude integer)"
                    ";"
                    "create table route_way ("
                    "route_point primary key, "
                    "description text)"
                    ";"
                    "create table track_path ("
                    "num integer primary key, "
                    "unitx integer, "
                    "unity integer, "
                    "time integer, "
                    "altitude integer)"
                    ";"
                    "create table track_way ("
                    "track_point primary key, "
                    "description text)",
                    NULL, NULL, NULL), FALSE) /* !! Comma operator !! */
            /* Create prepared statements - failure here is bad! */
            || SQLITE_OK != sqlite3_prepare(_path_db,
                    "select unitx, unity, time, altitude, description "
                    "from route_path left join route_way on "
                    "route_path.num = route_way.route_point "
                    "order by route_path.num",
                    -1, &_route_stmt_select, NULL)
            || SQLITE_OK != sqlite3_prepare(_path_db,
                    "select unitx, unity, time, altitude, description "
                    "from track_path left join track_way on "
                    "track_path.num = track_way.track_point "
                    "order by track_path.num",
                    -1, &_track_stmt_select, NULL)
            || SQLITE_OK != sqlite3_prepare(_path_db,
                    "delete from route_path",
                    -1, &_route_stmt_delete_path, NULL)
            || SQLITE_OK != sqlite3_prepare(_path_db,
                    "delete from route_way",
                    -1, &_route_stmt_delete_way, NULL)
            || SQLITE_OK != sqlite3_prepare(_path_db,
                    "insert into route_path (num, unitx, unity, time, altitude) "
                    "values (NULL, ?, ?, ?, ?)",
                    -1, &_route_stmt_insert_path, NULL)
            || SQLITE_OK != sqlite3_prepare(_path_db,
                    "insert into route_way (route_point, description) "
                    "values (?, ?)",
                    -1, &_route_stmt_insert_way, NULL)
            || SQLITE_OK != sqlite3_prepare(_path_db,
                    "delete from track_path",
                    -1, &_track_stmt_delete_path, NULL)
            || SQLITE_OK != sqlite3_prepare(_path_db,
                    "delete from track_way",
                    -1, &_track_stmt_delete_way, NULL)
            || SQLITE_OK != sqlite3_prepare(_path_db,
                    "insert into track_path (num, unitx, unity, time, altitude) "
                    "values (NULL, ?, ?, ?, ?)",
                    -1, &_track_stmt_insert_path, NULL)
            || SQLITE_OK != sqlite3_prepare(_path_db,
                    "insert into track_way (track_point, description) "
                    "values (?, ?)",
                    -1, &_track_stmt_insert_way, NULL)
            || SQLITE_OK != sqlite3_prepare(_path_db, "begin transaction",
                    -1, &_path_stmt_trans_begin, NULL)
            || SQLITE_OK != sqlite3_prepare(_path_db, "commit transaction",
                    -1, &_path_stmt_trans_commit, NULL)
            || SQLITE_OK != sqlite3_prepare(_path_db, "rollback transaction",
                    -1, &_path_stmt_trans_rollback, NULL))
        {   
            gchar buffer[BUFFER_SIZE];
            snprintf(buffer, sizeof(buffer), "%s\n%s",
                    _("Failed to open path database. "
                        "Tracks and routes will not be saved."),
                    sqlite3_errmsg(_path_db));
            sqlite3_close(_path_db);
            _path_db = NULL;
            popup_error(_window, buffer);
        }
        else
        {   
            read_path_from_db(&_route, _route_stmt_select);
            read_path_from_db(&_track, _track_stmt_select);
            _track_index_last_saved = _track.tail - _track.head - 1;
        }
        g_free(path_db_file);
    }

    g_free(settings_dir);

    _last_spoken_phrase = g_strdup("");

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

void
path_destroy()
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    /* Save paths. */
    if(_track.tail->unity)
        track_insert_break(FALSE);
    path_update_track_in_db();
    path_save_route_to_db();

    if(_path_db)
    {   
        sqlite3_close(_path_db);
        _path_db = NULL;
    }

    MACRO_PATH_FREE(_track);
    MACRO_PATH_FREE(_route);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}
