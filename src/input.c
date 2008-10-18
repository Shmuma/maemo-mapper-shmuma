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

#include <math.h>
#include <gdk/gdkkeysyms.h>

#ifndef LEGACY
#    include <hildon/hildon-defines.h>
#    include <hildon/hildon-banner.h>
#else
#    include <hildon-widgets/hildon-defines.h>
#    include <hildon-widgets/hildon-banner.h>
#endif

#include "types.h"
#include "data.h"
#include "defines.h"

#include "display.h"
#include "gdk-pixbuf-rotate.h"
#include "gps.h"
#include "input.h"
#include "maps.h"
#include "path.h"
#include "poi.h"
#include "util.h"

static gint _key_zoom_is_down = FALSE;
static gint _key_zoom_new = -1;
static gint _key_zoom_timeout_sid = 0;
static gint _key_pan_is_down = FALSE;
static gfloat _key_pan_incr_devx = 0;
static gfloat _key_pan_incr_devy = 0;
static gint _key_pan_timeout_sid = 0;

static gboolean
key_pan_timeout(CustomAction action)
{
    printf("%s()\n", __PRETTY_FUNCTION__);
    if(_key_pan_is_down)
    {
        /* The pan key is still down.  Continue panning. */
        _map_offset_devx += -_key_pan_incr_devx;
        _map_offset_devy += -_key_pan_incr_devy;
        MACRO_QUEUE_DRAW_AREA();
        vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
        return TRUE;
    }
    else
    {
        gfloat panx_adj, pany_adj;
        /* Time is up for further action - execute the pan. */
        /* Adjust for rotate angle. */
        gdk_pixbuf_rotate_vector(&panx_adj, &pany_adj, _map_reverse_matrix,
                _map_offset_devx, _map_offset_devy);
        map_pan(-pixel2unit((gint)(panx_adj + 0.5f)),
                    -pixel2unit((gint)(pany_adj + 0.5f)));
        _key_pan_timeout_sid = 0;
        vprintf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
        return FALSE;
    }
}

static gboolean
key_zoom_timeout(CustomAction action)
{
    printf("%s()\n", __PRETTY_FUNCTION__);
    if(_key_zoom_is_down)
    {
        /* The zoom key is still down.  Continue zooming. */
        if(action == CUSTOM_ACTION_ZOOM_IN)
        {
            /* We're currently zooming in (_zoom is decreasing). */
            gint test = _key_zoom_new - _curr_repo->view_zoom_steps;
            if(test >= 0)
                /* We can zoom some more.  Hurray! */
                _key_zoom_new = test;
        }
        else
        {
            /* We're currently zooming out (_zoom is increasing). */
            gint test = _key_zoom_new + _curr_repo->view_zoom_steps;
            if(test <= MAX_ZOOM)
                /* We can zoom some more.  Hurray! */
                _key_zoom_new = test;
        }
        /* Tell them where they're about to zoom. */
        {
            gchar buffer[32];
            snprintf(buffer, sizeof(buffer),
                    "%s %d", _("Zoom to Level"), _key_zoom_new);
            MACRO_BANNER_SHOW_INFO(_window, buffer);
        }
        vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
        return TRUE;
    }
    else
    {
        /* Time is up for further action - execute. */
        if(_key_zoom_new != _zoom)
            map_set_zoom(_key_zoom_new);
        _key_pan_timeout_sid = 0;
        _key_zoom_new = -1;
        vprintf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
        return FALSE;
    }

}

static CustomKey
get_custom_key_from_keyval(gint keyval)
{
    CustomKey custom_key;
    printf("%s(%d)\n", __PRETTY_FUNCTION__, keyval);

    switch(keyval)
    {
        case HILDON_HARDKEY_UP:
            custom_key = CUSTOM_KEY_UP;
            break;
        case HILDON_HARDKEY_DOWN:
            custom_key = CUSTOM_KEY_DOWN;
            break;
        case HILDON_HARDKEY_LEFT:
            custom_key = CUSTOM_KEY_LEFT;
            break;
        case HILDON_HARDKEY_RIGHT:
            custom_key = CUSTOM_KEY_RIGHT;
            break;
        case HILDON_HARDKEY_SELECT:
            custom_key = CUSTOM_KEY_SELECT;
            break;
        case HILDON_HARDKEY_INCREASE:
            custom_key = CUSTOM_KEY_INCREASE;
            break;
        case HILDON_HARDKEY_DECREASE:
            custom_key = CUSTOM_KEY_DECREASE;
            break;
        case HILDON_HARDKEY_FULLSCREEN:
            custom_key = CUSTOM_KEY_FULLSCREEN;
            break;
        case HILDON_HARDKEY_ESC:
            custom_key = CUSTOM_KEY_ESC;
            break;
        default:
            custom_key = -1;
    }

    vprintf("%s(): return %d\n", __PRETTY_FUNCTION__, custom_key);
    return custom_key;
}

gboolean
window_cb_key_press(GtkWidget* widget, GdkEventKey *event)
{
    CustomKey custom_key;
    printf("%s()\n", __PRETTY_FUNCTION__);

    custom_key = get_custom_key_from_keyval(event->keyval);
    if(custom_key == -1)
        return FALSE; /* Not our event. */

    switch(_action[custom_key])
    {
        case CUSTOM_ACTION_PAN_NORTH:
        case CUSTOM_ACTION_PAN_SOUTH:
        case CUSTOM_ACTION_PAN_EAST:
        case CUSTOM_ACTION_PAN_WEST:
        case CUSTOM_ACTION_PAN_UP:
        case CUSTOM_ACTION_PAN_DOWN:
        case CUSTOM_ACTION_PAN_LEFT:
        case CUSTOM_ACTION_PAN_RIGHT:
            if(!_key_pan_is_down)
            {
                _key_pan_is_down = TRUE;
                if(_center_mode > 0)
                    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(
                                _menu_view_ac_none_item), FALSE);
                if(_key_pan_timeout_sid)
                {
                    g_source_remove(_key_pan_timeout_sid);
                    _key_pan_timeout_sid = 0;
                }
                /* Figure out new pan. */
                switch(_action[custom_key])
                {
                    case CUSTOM_ACTION_PAN_UP:
                    case CUSTOM_ACTION_PAN_NORTH:
                        _key_pan_incr_devy = -PAN_PIXELS;
                        break;
                    case CUSTOM_ACTION_PAN_SOUTH:
                    case CUSTOM_ACTION_PAN_DOWN:
                        _key_pan_incr_devy = PAN_PIXELS;
                        break;
                    case CUSTOM_ACTION_PAN_EAST:
                    case CUSTOM_ACTION_PAN_RIGHT:
                        _key_pan_incr_devx = PAN_PIXELS;
                        break;
                    case CUSTOM_ACTION_PAN_WEST:
                    case CUSTOM_ACTION_PAN_LEFT:
                        _key_pan_incr_devx = -PAN_PIXELS;
                        break;
                    default:
                        g_printerr("Invalid action in key_pan_timeout(): %d\n",
                                _action[custom_key]);
                }
                switch(_action[custom_key])
                {
                    case CUSTOM_ACTION_PAN_NORTH:
                    case CUSTOM_ACTION_PAN_SOUTH:
                    case CUSTOM_ACTION_PAN_EAST:
                    case CUSTOM_ACTION_PAN_WEST:
                        /* Adjust for rotate angle. */
                        gdk_pixbuf_rotate_vector(&_key_pan_incr_devx,
                                &_key_pan_incr_devy, _map_rotate_matrix,
                                _key_pan_incr_devx, _key_pan_incr_devy);
                    default:
                        ;
                }
                key_pan_timeout(_action[custom_key]);
                _key_pan_timeout_sid = g_timeout_add_full(G_PRIORITY_HIGH_IDLE,
                        250, (GSourceFunc)key_pan_timeout,
                        (gpointer)(_action[custom_key]), NULL);
            }
            break;

        case CUSTOM_ACTION_RESET_VIEW_ANGLE:
            map_rotate(-_next_map_rotate_angle);
            break;

        case CUSTOM_ACTION_ROTATE_CLOCKWISE:
        {
            map_rotate(-ROTATE_DEGREES);
            break;
        }

        case CUSTOM_ACTION_ROTATE_COUNTERCLOCKWISE:
        {
            map_rotate(ROTATE_DEGREES);
            break;
        }

        case CUSTOM_ACTION_TOGGLE_AUTOROTATE:
            gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(
                        _menu_view_rotate_auto_item), !_center_rotate);
            break;

        case CUSTOM_ACTION_TOGGLE_AUTOCENTER:
            switch(_center_mode)
            {
                case CENTER_LATLON:
                case CENTER_WAS_LEAD:
                    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(
                                _menu_view_ac_lead_item), TRUE);
                    break;
                case CENTER_LEAD:
                case CENTER_WAS_LATLON:
                    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(
                                _menu_view_ac_latlon_item), TRUE);
                    break;
            }
            break;

        case CUSTOM_ACTION_ZOOM_IN:
        case CUSTOM_ACTION_ZOOM_OUT:
            if(!_key_zoom_is_down)
            {
                _key_zoom_is_down = TRUE;
                if(_key_zoom_timeout_sid)
                {
                    g_source_remove(_key_zoom_timeout_sid);
                    _key_zoom_timeout_sid = 0;
                }
                if(_key_zoom_new == -1)
                    _key_zoom_new = _next_zoom;
                _key_zoom_new = _key_zoom_new
                    + (_action[custom_key] == CUSTOM_ACTION_ZOOM_IN
                            ? -_curr_repo->view_zoom_steps
                            : _curr_repo->view_zoom_steps);
                BOUND(_key_zoom_new, 0, MAX_ZOOM);
                {
                    gchar buffer[80];
                    snprintf(buffer, sizeof(buffer),"%s %d",
                            _("Zoom to Level"), _key_zoom_new);
                    MACRO_BANNER_SHOW_INFO(_window, buffer);
                }
                _key_zoom_timeout_sid =g_timeout_add_full(G_PRIORITY_HIGH_IDLE,
                        500, (GSourceFunc)key_zoom_timeout,
                        (gpointer)(_action[custom_key]), NULL);
            }
            break;

        case CUSTOM_ACTION_TOGGLE_FULLSCREEN:
            gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(
                        _menu_view_fullscreen_item), !_fullscreen);
            break;

        case CUSTOM_ACTION_TOGGLE_TRACKING:
            gtk_check_menu_item_set_active(
                    GTK_CHECK_MENU_ITEM(_menu_track_enable_tracking_item),
                    !_enable_tracking);
            break;

        case CUSTOM_ACTION_TOGGLE_TRACKS:
            switch(_show_paths)
            {
                case 0:
                    /* Nothing shown, nothing saved; just set both. */
                    _show_paths = TRACKS_MASK | ROUTES_MASK;
                    break;
                case TRACKS_MASK << 16:
                case ROUTES_MASK << 16:
                case (ROUTES_MASK | TRACKS_MASK) << 16:
                    /* Something was saved and nothing changed since.
                     * Restore saved. */
                    _show_paths = _show_paths >> 16;
                    break;
                default:
                    /* There is no history, or they changed something
                     * since the last historical change. Save and
                     * clear. */
                    _show_paths = _show_paths << 16;
            }
            gtk_check_menu_item_set_active(
                    GTK_CHECK_MENU_ITEM(_menu_view_show_routes_item),
                    _show_paths & ROUTES_MASK);

            gtk_check_menu_item_set_active(
                    GTK_CHECK_MENU_ITEM(_menu_view_show_tracks_item),
                    _show_paths & TRACKS_MASK);

        case CUSTOM_ACTION_TOGGLE_SCALE:
            gtk_check_menu_item_set_active(
                    GTK_CHECK_MENU_ITEM(_menu_view_show_scale_item),
                    !_show_scale);
            break;

        case CUSTOM_ACTION_TOGGLE_POI:
            gtk_check_menu_item_set_active(
                    GTK_CHECK_MENU_ITEM(_menu_view_show_poi_item),
                    !_show_poi);
            break;
        case CUSTOM_ACTION_CHANGE_REPO: {
            GList *curr = g_list_find(_repo_list, _curr_repo);
            if(!curr)
                break;

            /* Loop until we reach a next-able repo, or until we get
             * back to the current repo. */
            while((curr = (curr->next ? curr->next : _repo_list))
                    && !((RepoData*)curr->data)->nextable
                    && curr->data != _curr_repo) { }

            if(curr->data != _curr_repo)
            {
                repo_set_curr(curr->data);
                gtk_check_menu_item_set_active(
                        GTK_CHECK_MENU_ITEM(_curr_repo->menu_item),
                        TRUE);
            }
            else
            {
                popup_error(_window,
                    _("There are no other next-able repositories."));
            }
            break;
        }

        case CUSTOM_ACTION_RESET_BLUETOOTH:
            reset_bluetooth();
            break;

        case CUSTOM_ACTION_ROUTE_DISTNEXT:
            route_show_distance_to_next();
            break;

        case CUSTOM_ACTION_ROUTE_DISTLAST:
            route_show_distance_to_last();
            break;

        case CUSTOM_ACTION_TRACK_BREAK:
            track_insert_break(TRUE);
            break;

        case CUSTOM_ACTION_TRACK_CLEAR:
            track_clear();
            break;

        case CUSTOM_ACTION_TRACK_DISTLAST:
            track_show_distance_from_last();
            break;

        case CUSTOM_ACTION_TRACK_DISTFIRST:
            track_show_distance_from_first();
            break;

        case CUSTOM_ACTION_TOGGLE_GPS:
            gtk_check_menu_item_set_active(
                    GTK_CHECK_MENU_ITEM(_menu_gps_enable_item),
                    !_enable_gps);
            break;

        case CUSTOM_ACTION_TOGGLE_GPSINFO:
            gtk_check_menu_item_set_active(
                    GTK_CHECK_MENU_ITEM(_menu_gps_show_info_item),
                    !_gps_info);
            break;

        case CUSTOM_ACTION_TOGGLE_SPEEDLIMIT:
            _speed_limit_on ^= 1;
            break;

        case CUSTOM_ACTION_TOGGLE_LAYERS:
            maps_toggle_visible_layers ();
            break;

        default:
            vprintf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
            return FALSE;
    }

    return TRUE;
    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
}

gboolean
window_cb_key_release(GtkWidget* widget, GdkEventKey *event)
{
    CustomKey custom_key;
    printf("%s()\n", __PRETTY_FUNCTION__);

    custom_key = get_custom_key_from_keyval(event->keyval);
    if(custom_key < 0)
        return FALSE; /* Not our event. */

    switch(_action[custom_key])
    {
        case CUSTOM_ACTION_ZOOM_IN:
        case CUSTOM_ACTION_ZOOM_OUT:
            if(_key_zoom_timeout_sid)
            {
                g_source_remove(_key_zoom_timeout_sid);
                _key_zoom_timeout_sid = 0;
                _key_zoom_timeout_sid =g_timeout_add_full(G_PRIORITY_HIGH_IDLE,
                        500, (GSourceFunc)key_zoom_timeout, NULL, NULL);
            }
            _key_zoom_is_down = FALSE;
            return TRUE;

        case CUSTOM_ACTION_PAN_NORTH:
        case CUSTOM_ACTION_PAN_SOUTH:
        case CUSTOM_ACTION_PAN_EAST:
        case CUSTOM_ACTION_PAN_WEST:
        case CUSTOM_ACTION_PAN_UP:
        case CUSTOM_ACTION_PAN_DOWN:
        case CUSTOM_ACTION_PAN_LEFT:
        case CUSTOM_ACTION_PAN_RIGHT:
            if(_key_pan_timeout_sid)
            {
                g_source_remove(_key_pan_timeout_sid);
                _key_pan_timeout_sid = g_timeout_add_full(G_PRIORITY_HIGH_IDLE,
                        500, (GSourceFunc)key_pan_timeout, NULL, NULL);
            }
            _key_pan_is_down = FALSE;
            _key_pan_incr_devx = 0;
            _key_pan_incr_devy = 0;
            return TRUE;

        default:
            return FALSE;
    }
    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

gboolean
map_cb_motion_notify(GtkWidget *widget, GdkEventMotion *event)
{
    gint x, y;
    GdkModifierType state;
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(!_mouse_is_down)
        return FALSE;

    if(event->is_hint)
        gdk_window_get_pointer(event->window, &x, &y, &state);
    else
    {
        x = event->x;
        y = event->y;
        state = event->state;
    }

    if(_mouse_is_dragging)
    {
        /* Already in the process of dragging.  Set the offset. */
        _map_offset_devx = x - _cmenu_position_x;
        _map_offset_devy = y - _cmenu_position_y;
        MACRO_QUEUE_DRAW_AREA();
    }
    else
    {
        gint diffx = x - _cmenu_position_x - _map_offset_devx;
        gint diffy = y - _cmenu_position_y - _map_offset_devy;
        if(diffx * diffx + diffy + diffy > 100)
        {
            _mouse_is_dragging = TRUE;
        }
    }

    /* Return FALSE to allow context menu to work. */
    vprintf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

gboolean
map_cb_button_press(GtkWidget *widget, GdkEventButton *event)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(!_mouse_is_down)
    {
        _cmenu_position_x = event->x + 0.5 - _map_offset_devx;
        _cmenu_position_y = event->y + 0.5 - _map_offset_devy;
        g_mutex_lock(_mouse_mutex);
        _mouse_is_down = TRUE;
    }

    /* Return FALSE to allow context menu to work. */
    vprintf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
    return FALSE;
}

gboolean
map_cb_button_release(GtkWidget *widget, GdkEventButton *event)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(!_mouse_is_down)
    {
        /* We didn't know about it anyway.  Return FALSE. */
        vprintf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
        return FALSE;
    }

    _mouse_is_down = FALSE;

    if(event->button == 3) /* right-click */
    {   
        gtk_menu_popup(_map_cmenu, NULL, NULL, NULL, NULL,
                       event->button, event->time);
    }
    else
#ifdef DEBUG
    if(event->button == 2) /* middle-click */
    {   
        screen2unit((gint)(event->x + 0.5), (gint)(event->y + 0.5),
                _pos.unitx, _pos.unity);
        unit2latlon(_pos.unitx, _pos.unity, _gps.lat, _gps.lon);
        /* Test unit-to-lat/lon conversion. */
        latlon2unit(_gps.lat, _gps.lon, _pos.unitx, _pos.unity);
        _gps.speed = ((gint)(_gps.speed + 5) % 60);
        _gps.heading = ((gint)(_gps.heading + 5) % 360);
        if(track_add(time(NULL), FALSE))
        {
            /* Move mark to new location. */
            map_refresh_mark(FALSE);
        }
        else
        {
            map_move_mark();
        }
    }
    else
#endif
    if(_mouse_is_dragging)
    {
        Point clkpt;

        _mouse_is_dragging = FALSE;

        screen2unit(_view_halfwidth_pixels, _view_halfheight_pixels,
                clkpt.unitx, clkpt.unity);

        if(_center_mode > 0)
        {
            gfloat matrix[4];
            gint mvpt_unitx, mvpt_unity;
            gfloat new_offset_unitx, new_offset_unity;
            /* If auto-center is enabled (meaning that panning will cause the
             * view the rotate according to the heading), rotate the new
             * unitx/unity around the mvpt, by any changes in _gps.heading
             * since the last time _map_rotate_angle was updated.  The end
             * result is that the point the user is moving stays in the same
             * relative point, even if the heading has since changed. */
            gdk_pixbuf_rotate_matrix_fill_for_rotation(matrix,
                    deg2rad(_gps.heading - _next_map_rotate_angle));
            buf2unit(_cmenu_position_x, _cmenu_position_y,
                    mvpt_unitx, mvpt_unity);
            gdk_pixbuf_rotate_vector(&new_offset_unitx, &new_offset_unity,
                    matrix,
                    (gint)(clkpt.unitx - mvpt_unitx),
                    (gint)(clkpt.unity -mvpt_unity));
            clkpt.unitx = mvpt_unitx + new_offset_unitx;
            clkpt.unity = mvpt_unity + new_offset_unity;
        }

        if(_center_mode > 0)
            gtk_check_menu_item_set_active(
                    GTK_CHECK_MENU_ITEM(_menu_view_ac_none_item), TRUE);

        map_center_unit(clkpt);
    }
    else
    {
        gboolean selected_point = FALSE;
        Point clkpt;
        screen2unit(event->x, event->y, clkpt.unitx, clkpt.unity);

        if(_show_poi && (_poi_zoom > _zoom))
        {
            PoiInfo poi;
            selected_point = select_poi(
                    clkpt.unitx, clkpt.unity, &poi, TRUE);
            if(selected_point)
            {
                gchar *banner;
                banner = g_strdup_printf("%s (%s)", poi.label, poi.clabel);
                MACRO_BANNER_SHOW_INFO(_window, banner);
                g_free(banner);
                g_free(poi.label);
                g_free(poi.desc);
                g_free(poi.clabel);
            }
        }
        if(!selected_point && (_show_paths & ROUTES_MASK)
                && _route.whead <= _route.wtail)
        {
            WayPoint *way = find_nearest_waypoint(
                    clkpt.unitx, clkpt.unity);
            if(way)
            {
                selected_point = TRUE;
                MACRO_BANNER_SHOW_INFO(_window, way->desc);
            }
        }
        if(!selected_point)
        {
            if(_center_mode > 0)
                gtk_check_menu_item_set_active(
                        GTK_CHECK_MENU_ITEM(_menu_view_ac_none_item), TRUE);
            map_center_unit_full(clkpt, _next_zoom,
                    _center_mode > 0 && _center_rotate
                    ? _gps.heading : _next_map_rotate_angle);
        }
    }

    g_mutex_unlock(_mouse_mutex);

    /* Return FALSE to avoid context menu (if it hasn't popped up already). */
    vprintf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
    return FALSE;
}

void
input_init()
{
    printf("%s():\n", __PRETTY_FUNCTION__);

    g_signal_connect(G_OBJECT(_window), "key_press_event",
            G_CALLBACK(window_cb_key_press), NULL);

    g_signal_connect(G_OBJECT(_window), "key_release_event",
            G_CALLBACK(window_cb_key_release), NULL);

    g_signal_connect(G_OBJECT(_map_widget), "motion_notify_event",
            G_CALLBACK(map_cb_motion_notify), NULL);

    g_signal_connect(G_OBJECT(_map_widget), "button_press_event",
            G_CALLBACK(map_cb_button_press), NULL);

    g_signal_connect(G_OBJECT(_map_widget), "button_release_event",
            G_CALLBACK(map_cb_button_release), NULL);

    gtk_widget_add_events(_map_widget,
            GDK_EXPOSURE_MASK
            | GDK_POINTER_MOTION_MASK
            | GDK_POINTER_MOTION_HINT_MASK
            | GDK_BUTTON_PRESS_MASK
            | GDK_BUTTON_RELEASE_MASK);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}
