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

#ifndef LEGACY
#    include <hildon/hildon-help.h>
#    include <hildon/hildon-program.h>
#    include <hildon/hildon-banner.h>
#else
#    include <osso-helplib.h>
#    include <hildon-widgets/hildon-program.h>
#    include <hildon-widgets/hildon-banner.h>
#    include <hildon-widgets/hildon-input-mode-hint.h>
#endif

#include "types.h"
#include "data.h"
#include "defines.h"

#include "display.h"
#include "gps.h"
#include "gdk-pixbuf-rotate.h"
#include "gpx.h"
#include "maps.h"
#include "menu.h"
#include "path.h"
#include "poi.h"
#include "settings.h"
#include "util.h"

static GtkWidget *_menu_route_open_item = NULL;
static GtkWidget *_menu_route_download_item = NULL;
static GtkWidget *_menu_route_save_item = NULL;
static GtkWidget *_menu_route_distnext_item = NULL;
static GtkWidget *_menu_route_distlast_item = NULL;
static GtkWidget *_menu_route_reset_item = NULL;
static GtkWidget *_menu_route_clear_item = NULL;
static GtkWidget *_menu_track_open_item = NULL;
static GtkWidget *_menu_track_save_item = NULL;
static GtkWidget *_menu_track_insert_break_item = NULL;
static GtkWidget *_menu_track_insert_mark_item = NULL;
static GtkWidget *_menu_track_distlast_item = NULL;
static GtkWidget *_menu_track_distfirst_item = NULL;
static GtkWidget *_menu_track_clear_item = NULL;
static GtkWidget *_menu_poi_import_item = NULL;
static GtkWidget *_menu_poi_download_item = NULL;
static GtkWidget *_menu_poi_browse_item = NULL;
static GtkWidget *_menu_poi_categories_item = NULL;
static GtkWidget *_menu_maps_submenu = NULL;
static GtkWidget *_menu_layers_submenu = NULL;
static GtkWidget *_menu_maps_mapman_item = NULL;
static GtkWidget *_menu_maps_repoman_item = NULL;
static GtkWidget *_menu_view_zoom_in_item = NULL;
static GtkWidget *_menu_view_zoom_out_item = NULL;
static GtkWidget *_menu_view_rotate_clock_item = NULL;
static GtkWidget *_menu_view_rotate_counter_item = NULL;
static GtkWidget *_menu_view_rotate_reset_item = NULL;
static GtkWidget *_menu_view_pan_up_item = NULL;
static GtkWidget *_menu_view_pan_down_item = NULL;
static GtkWidget *_menu_view_pan_left_item = NULL;
static GtkWidget *_menu_view_pan_right_item = NULL;
static GtkWidget *_menu_view_pan_north_item = NULL;
static GtkWidget *_menu_view_pan_south_item = NULL;
static GtkWidget *_menu_view_pan_west_item = NULL;
static GtkWidget *_menu_view_pan_east_item = NULL;
static GtkWidget *_menu_view_show_zoomlevel_item = NULL;
static GtkWidget *_menu_view_show_comprose_item = NULL;
static GtkWidget *_menu_view_show_velvec_item = NULL;
static GtkWidget *_menu_view_goto_latlon_item = NULL;
static GtkWidget *_menu_view_goto_address_item = NULL;
static GtkWidget *_menu_view_goto_gps_item = NULL;
static GtkWidget *_menu_view_goto_nextway_item = NULL;
static GtkWidget *_menu_view_goto_nearpoi_item = NULL;
static GtkWidget *_menu_settings_item = NULL;
static GtkWidget *_menu_help_item = NULL;
static GtkWidget *_menu_about_item = NULL;
static GtkWidget *_menu_close_item = NULL;

/****************************************************************************
 * BELOW: ROUTE MENU ********************************************************
 ****************************************************************************/

static gboolean
menu_cb_route_open(GtkMenuItem *item)
{
    gchar *buffer;
    gint size;
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(display_open_file(GTK_WINDOW(_window), &buffer, NULL, &size,
                &_route_dir_uri, NULL, GTK_FILE_CHOOSER_ACTION_OPEN))
    {
        /* If auto is enabled, append the route, otherwise replace it. */
        if(gpx_path_parse(&_route, buffer, size,
                    _autoroute_data.enabled ? 0 : 1))
        {
            path_save_route_to_db();

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
menu_cb_route_download(GtkMenuItem *item)
{
    printf("%s()\n", __PRETTY_FUNCTION__);
    route_download(NULL);
    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_route_save(GtkMenuItem *item)
{
    GnomeVFSHandle *handle;
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(display_open_file(GTK_WINDOW(_window), NULL, &handle, NULL,
                &_route_dir_uri, NULL, GTK_FILE_CHOOSER_ACTION_SAVE))
    {
        if(gpx_path_write(&_route, handle))
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
menu_cb_route_distnext(GtkMenuItem *item)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    route_show_distance_to_next();

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_route_distlast(GtkMenuItem *item)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    route_show_distance_to_last();

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

gboolean
menu_cb_route_reset(GtkMenuItem *item)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    path_reset_route();

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_route_clear(GtkMenuItem *item)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    cancel_autoroute();
    MACRO_PATH_FREE(_route);
    MACRO_PATH_INIT(_route);
    path_save_route_to_db();
    route_find_nearest_point();
    map_force_redraw();

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

/****************************************************************************
 * ABOVE: ROUTE MENU ********************************************************
 ****************************************************************************/

/****************************************************************************
 * BELOW: TRACK MENU ********************************************************
 ****************************************************************************/

static gboolean
menu_cb_track_open(GtkMenuItem *item)
{
    gchar *buffer;
    gint size;
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(display_open_file(GTK_WINDOW(_window), &buffer, NULL, &size,
                NULL, &_track_file_uri, GTK_FILE_CHOOSER_ACTION_OPEN))
    {
        if(gpx_path_parse(&_track, buffer, size, -1))
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
menu_cb_track_save(GtkMenuItem *item)
{
    GnomeVFSHandle *handle;
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(display_open_file(GTK_WINDOW(_window), NULL, &handle, NULL,
                NULL, &_track_file_uri, GTK_FILE_CHOOSER_ACTION_SAVE))
    {
        if(gpx_path_write(&_track, handle))
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
menu_cb_track_insert_break(GtkMenuItem *item)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    track_insert_break(TRUE);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_track_insert_mark(GtkMenuItem *item)
{
    gdouble lat, lon;
    gchar tmp1[LL_FMT_LEN], tmp2[LL_FMT_LEN], *p_latlon;
    static GtkWidget *dialog = NULL;
    static GtkWidget *table = NULL;
    static GtkWidget *label = NULL;
    static GtkWidget *lbl_latlon = NULL;
    static GtkWidget *txt_scroll = NULL;
    static GtkWidget *txt_desc = NULL;
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(dialog == NULL)
    {
        dialog = gtk_dialog_new_with_buttons(_("Insert Mark"),
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

        gtk_table_attach(GTK_TABLE(table),
                lbl_latlon = gtk_label_new(""),
                1, 2, 0, 1, GTK_FILL, 0, 2, 4);
        gtk_misc_set_alignment(GTK_MISC(lbl_latlon), 0.0f, 0.5f);

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

    unit2latlon(_pos.unitx, _pos.unity, lat, lon);
    lat_format(lat, tmp1);
    lon_format(lon, tmp2);
    p_latlon = g_strdup_printf("%s, %s", tmp1, tmp2);
    gtk_label_set_text(GTK_LABEL(lbl_latlon), p_latlon);
    g_free(p_latlon);

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
            MACRO_PATH_INCREMENT_WTAIL(_track);
            _track.wtail->point = _track.tail;
            _track.wtail->desc
                = gtk_text_buffer_get_text(tbuf, &ti1, &ti2, TRUE);
        }
        else
        {
            popup_error(dialog,
                    _("Please provide a description for the mark."));
            g_free(desc);
            continue;
        }

        map_render_paths();
        MACRO_QUEUE_DRAW_AREA();
        break;
    }
    gtk_widget_hide(dialog);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_track_distlast(GtkMenuItem *item)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    track_show_distance_from_last();

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_track_distfirst(GtkMenuItem *item)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    track_show_distance_from_first();

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_track_clear(GtkMenuItem *item)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    track_clear();

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_track_enable_tracking(GtkMenuItem *item)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(!(_enable_tracking = gtk_check_menu_item_get_active(
                GTK_CHECK_MENU_ITEM(_menu_track_enable_tracking_item))))
    {
        track_insert_break(FALSE); /* FALSE = not temporary */
        MACRO_BANNER_SHOW_INFO(_window, _("Tracking Disabled"));
    }
    else
    {
        MACRO_BANNER_SHOW_INFO(_window, _("Tracking Enabled"));
    }


    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

/****************************************************************************
 * ABOVE: TRACK MENU ********************************************************
 ****************************************************************************/

/****************************************************************************
 * BELOW: POI MENU **********************************************************
 ****************************************************************************/

static gboolean
menu_cb_poi_import(GtkMenuItem *item)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(poi_import_dialog(_center.unitx, _center.unity))
        map_force_redraw();

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_poi_download(GtkMenuItem *item)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(poi_download_dialog(0, 0)) /* 0, 0 means no default origin */
        map_force_redraw();

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_poi_browse(GtkMenuItem *item)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(poi_browse_dialog(0, 0)) /* 0, 0 means no default origin */
        map_force_redraw();

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_poi_categories(GtkMenuItem *item)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(category_list_dialog(_window))
        map_force_redraw();

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

/****************************************************************************
 * ABOVE: POI MENU **********************************************************
 ****************************************************************************/

/****************************************************************************
 * BELOW: MAPS MENU *********************************************************
 ****************************************************************************/

static gboolean
menu_cb_maps_repoman(GtkMenuItem *item)
{
    printf("%s()\n", __PRETTY_FUNCTION__);
    repoman_dialog();
    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_maps_select(GtkMenuItem *item, gpointer new_repo)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item)))
    {
        repo_set_curr(new_repo);
        map_refresh_mark(TRUE);
    }
    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_maps_mapman(GtkMenuItem *item)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    mapman_dialog();

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_maps_auto_download(GtkMenuItem *item)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if((_auto_download = gtk_check_menu_item_get_active(
            GTK_CHECK_MENU_ITEM(_menu_maps_auto_download_item))))
    {
        if(_curr_repo->url == REPOTYPE_NONE)
            popup_error(_window,
                _("NOTE: You must set a Map URI in the current repository in "
                    "order to download maps."));
        map_refresh_mark(TRUE);
    }

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

/****************************************************************************
 * ABOVE: MAPS MENU *********************************************************
 ****************************************************************************/

/****************************************************************************
 * BELOW: LAYERS MENU *******************************************************
 ****************************************************************************/

static gboolean
menu_cb_layers_toggle(GtkCheckMenuItem *item, gpointer layer)
{
    RepoData* rd = (RepoData*)layer;

    printf("%s()\n", __PRETTY_FUNCTION__);

    rd->layer_enabled = !rd->layer_enabled;

    /* refresh if layer is on top of active map */
    if (repo_is_layer (_curr_repo, rd)) {
        /* reset layer's countdown */
        rd->layer_refresh_countdown = rd->layer_refresh_interval;
        map_cache_clean ();
        map_refresh_mark (TRUE);
    }

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

/****************************************************************************
 * ABOVE: LAYERS MENU *******************************************************
 ****************************************************************************/

/****************************************************************************
 * BELOW: VIEW/ZOOM MENU ****************************************************
 ****************************************************************************/

static gboolean
menu_cb_view_zoom_in(GtkMenuItem *item)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(_zoom > MIN_ZOOM)
    {
        gchar buffer[80];
        snprintf(buffer, sizeof(buffer),"%s %d",
                _("Zoom to Level"), _next_zoom - 1);
        MACRO_BANNER_SHOW_INFO(_window, buffer);
        map_set_zoom(_next_zoom - 1);
    }

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_view_zoom_out(GtkMenuItem *item)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(_zoom < MAX_ZOOM)
    {
        gchar buffer[80];
        snprintf(buffer, sizeof(buffer),"%s %d",
                _("Zoom to Level"), _next_zoom + 1);
        MACRO_BANNER_SHOW_INFO(_window, buffer);
        map_set_zoom(_next_zoom + 1);
    }

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

/****************************************************************************
 * ABOVE: VIEW/ZOOM MENU ****************************************************
 ****************************************************************************/

/****************************************************************************
 * BELOW: VIEW/ROTATE MENU **************************************************
 ****************************************************************************/

static gboolean
menu_cb_view_rotate_clock(GtkMenuItem *item)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    map_rotate(-ROTATE_DEGREES);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_view_rotate_counter(GtkMenuItem *item)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    map_rotate(ROTATE_DEGREES);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_view_rotate_reset(GtkMenuItem *item)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    map_rotate(-_next_map_rotate_angle);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_view_rotate_auto(GtkMenuItem *item)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(gtk_check_menu_item_get_active(
                GTK_CHECK_MENU_ITEM(_menu_view_rotate_auto_item)))
    {
        _center_rotate = TRUE;
        if(_center_mode > 0)
            map_refresh_mark(TRUE);
        MACRO_BANNER_SHOW_INFO(_window, _("Auto-Rotate Enabled"));
    }
    else
    {
        _center_rotate = FALSE;
        MACRO_BANNER_SHOW_INFO(_window, _("Auto-Rotate Disabled"));
    }

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

/****************************************************************************
 * ABOVE: VIEW/ROTATE MENU **************************************************
 ****************************************************************************/

/****************************************************************************
 * BELOW: VIEW/PAN MENU *****************************************************
 ****************************************************************************/

static gboolean
menu_cb_view_pan_up(GtkMenuItem *item)
{
    gfloat panx_adj, pany_adj;
    printf("%s()\n", __PRETTY_FUNCTION__);

    /* Adjust for rotate angle. */
    gdk_pixbuf_rotate_vector(&panx_adj, &pany_adj, _map_reverse_matrix,
            0, -PAN_PIXELS);

    map_pan(pixel2unit((gint)(panx_adj + 0.5f)),
                pixel2unit((gint)(pany_adj + 0.5f)));

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_view_pan_down(GtkMenuItem *item)
{
    gfloat panx_adj, pany_adj;
    printf("%s()\n", __PRETTY_FUNCTION__);

    /* Adjust for rotate angle. */
    gdk_pixbuf_rotate_vector(&panx_adj, &pany_adj, _map_reverse_matrix,
            0, PAN_PIXELS);

    map_pan(pixel2unit((gint)(panx_adj + 0.5f)),
                pixel2unit((gint)(pany_adj + 0.5f)));

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_view_pan_left(GtkMenuItem *item)
{
    gfloat panx_adj, pany_adj;
    printf("%s()\n", __PRETTY_FUNCTION__);

    /* Adjust for rotate angle. */
    gdk_pixbuf_rotate_vector(&panx_adj, &pany_adj, _map_reverse_matrix,
            -PAN_PIXELS, 0);

    map_pan(pixel2unit((gint)(panx_adj + 0.5f)),
                pixel2unit((gint)(pany_adj + 0.5f)));

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_view_pan_right(GtkMenuItem *item)
{
    gfloat panx_adj, pany_adj;
    printf("%s()\n", __PRETTY_FUNCTION__);

    /* Adjust for rotate angle. */
    gdk_pixbuf_rotate_vector(&panx_adj, &pany_adj, _map_reverse_matrix,
            PAN_PIXELS, 0);

    map_pan(pixel2unit((gint)(panx_adj + 0.5f)),
                pixel2unit((gint)(pany_adj + 0.5f)));

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_view_pan_north(GtkMenuItem *item)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    map_pan(0, -pixel2unit(PAN_PIXELS));

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_view_pan_south(GtkMenuItem *item)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    map_pan(0, pixel2unit(PAN_PIXELS));

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_view_pan_west(GtkMenuItem *item)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    map_pan(-pixel2unit(PAN_PIXELS), 0);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_view_pan_east(GtkMenuItem *item)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    map_pan(pixel2unit(PAN_PIXELS), 0);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

/****************************************************************************
 * ABOVE: VIEW/PAN MENU *****************************************************
 ****************************************************************************/

/****************************************************************************
 * BELOW: VIEW/GOTO MENU ****************************************************
 ****************************************************************************/

static gboolean
menu_cb_view_goto_latlon(GtkMenuItem *item)
{
    static GtkWidget *dialog = NULL;
    static GtkWidget *table = NULL;
    static GtkWidget *label = NULL;
    static GtkWidget *txt_lat = NULL;
    static GtkWidget *txt_lon = NULL;
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(dialog == NULL)
    {
        dialog = gtk_dialog_new_with_buttons(_("Go to Lat/Lon"),
                GTK_WINDOW(_window), GTK_DIALOG_MODAL,
                GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                NULL);

        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
                table = gtk_table_new(2, 3, FALSE), TRUE, TRUE, 0);

        gtk_table_attach(GTK_TABLE(table),
                label = gtk_label_new(_("Latitude")),
                0, 1, 0, 1, GTK_FILL, 0, 2, 4);
        gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);

        gtk_table_attach(GTK_TABLE(table),
                txt_lat = gtk_entry_new(),
                1, 2, 0, 1, GTK_FILL, 0, 2, 4);
        gtk_entry_set_width_chars(GTK_ENTRY(txt_lat), 16);
        gtk_misc_set_alignment(GTK_MISC(label), 0.0f, 0.5f);
#ifndef LEGACY
        g_object_set(G_OBJECT(txt_lat), "hildon-input-mode",
                HILDON_GTK_INPUT_MODE_FULL, NULL);
#else
        g_object_set(G_OBJECT(txt_lat), HILDON_AUTOCAP, FALSE, NULL);
        g_object_set(G_OBJECT(txt_lat), HILDON_INPUT_MODE_HINT,
                HILDON_INPUT_MODE_HINT_ALPHANUMERICSPECIAL, NULL);
#endif

        gtk_table_attach(GTK_TABLE(table),
                label = gtk_label_new(_("Longitude")),
                0, 1, 1, 2, GTK_FILL, 0, 2, 4);
        gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);

        gtk_table_attach(GTK_TABLE(table),
                txt_lon = gtk_entry_new(),
                1, 2, 1, 2, GTK_FILL, 0, 2, 4);
        gtk_entry_set_width_chars(GTK_ENTRY(txt_lon), 16);
        gtk_misc_set_alignment(GTK_MISC(label), 0.0f, 0.5f);
#ifndef LEGACY
        g_object_set(G_OBJECT(txt_lon), "hildon-input-mode",
                HILDON_GTK_INPUT_MODE_FULL, NULL);
#else
        g_object_set(G_OBJECT(txt_lon), HILDON_AUTOCAP, FALSE, NULL);
        g_object_set(G_OBJECT(txt_lon), HILDON_INPUT_MODE_HINT,
                HILDON_INPUT_MODE_HINT_ALPHANUMERICSPECIAL, NULL);
#endif
    }

    /* Initialize with the current center position. */
    {
        gchar buffer[32];
        gdouble lat, lon;
        unit2latlon(_center.unitx, _center.unity, lat, lon);
        lat_format(lat, buffer);
        gtk_entry_set_text(GTK_ENTRY(txt_lat), buffer);
        lon_format(lon, buffer);
        gtk_entry_set_text(GTK_ENTRY(txt_lon), buffer);
    }

    gtk_widget_show_all(dialog);

    while(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(dialog)))
    {
        const gchar *text;
        gchar *error_check;
        gdouble lat, lon;
        Point sel_unit;

        text = gtk_entry_get_text(GTK_ENTRY(txt_lat));
        lat = strdmstod(text, &error_check);
        if(text == error_check || lat < -90. || lat > 90.) {
            popup_error(dialog, _("Invalid Latitude"));
            continue;
        }

        text = gtk_entry_get_text(GTK_ENTRY(txt_lon));
        lon = strdmstod(text, &error_check);
        if(text == error_check || lon < -180. || lon > 180.) {
            popup_error(dialog, _("Invalid Longitude"));
            continue;
        }

        latlon2unit(lat, lon, sel_unit.unitx, sel_unit.unity);
        if(_center_mode > 0)
            gtk_check_menu_item_set_active(
                    GTK_CHECK_MENU_ITEM(_menu_view_ac_none_item), TRUE);
        map_center_unit(sel_unit);
        break;
    }
    gtk_widget_hide(dialog);
    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_view_goto_address(GtkMenuItem *item)
{
    static GtkWidget *dialog = NULL;
    static GtkWidget *table = NULL;
    static GtkWidget *label = NULL;
    static GtkWidget *txt_addr = NULL;
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(dialog == NULL)
    {
        GtkEntryCompletion *comp;
        dialog = gtk_dialog_new_with_buttons(_("Go to Address"),
                GTK_WINDOW(_window), GTK_DIALOG_MODAL,
                GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                NULL);

        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
                table = gtk_table_new(2, 3, FALSE), TRUE, TRUE, 0);

        gtk_table_attach(GTK_TABLE(table),
                label = gtk_label_new(_("Address")),
                0, 1, 0, 1, GTK_FILL, 0, 2, 4);
        gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);

        gtk_table_attach(GTK_TABLE(table),
                txt_addr = gtk_entry_new(),
                1, 2, 0, 1, GTK_FILL, 0, 2, 4);
        gtk_entry_set_width_chars(GTK_ENTRY(txt_addr), 25);
        gtk_misc_set_alignment(GTK_MISC(label), 0.0f, 0.5f);

        /* Set up auto-completion. */
        comp = gtk_entry_completion_new();
        gtk_entry_completion_set_model(comp, GTK_TREE_MODEL(_loc_model));
        gtk_entry_completion_set_text_column(comp, 0);
        gtk_entry_set_completion(GTK_ENTRY(txt_addr), comp);
    }

    gtk_widget_show_all(dialog);

    while(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(dialog)))
    {
        Point origin = locate_address(dialog,
                gtk_entry_get_text(GTK_ENTRY(txt_addr)));
        if(origin.unity)
        {
            MACRO_BANNER_SHOW_INFO(_window, _("Address Located"));

            if(_center_mode > 0)
                gtk_check_menu_item_set_active(
                        GTK_CHECK_MENU_ITEM(_menu_view_ac_none_item), TRUE);

            map_center_unit(origin);

            /* Success! Get out of the while loop. */
            break;
        }
    }
    gtk_widget_hide(dialog);
    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_view_goto_gps(GtkMenuItem *item)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(_center_mode > 0)
        gtk_check_menu_item_set_active(
                GTK_CHECK_MENU_ITEM(_menu_view_ac_none_item), TRUE);

    map_center_unit(_pos);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

gboolean
menu_cb_view_goto_nextway(GtkMenuItem *item)
{
    WayPoint *next_way;
    printf("%s()\n", __PRETTY_FUNCTION__);

    next_way = path_get_next_way();

    if(next_way && next_way->point->unity)
    {
        if(_center_mode > 0)
            gtk_check_menu_item_set_active(
                    GTK_CHECK_MENU_ITEM(_menu_view_ac_none_item), TRUE);

        map_center_unit(*next_way->point);
    }
    else
    {
        MACRO_BANNER_SHOW_INFO(_window, _("There is no next waypoint."));
    }

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

gboolean
menu_cb_view_goto_nearpoi(GtkMenuItem *item)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(_poi_enabled)
    {
        PoiInfo poi;
        gchar *banner;

        if((_center_mode > 0 ? get_nearest_poi(_pos.unitx, _pos.unity, &poi)
                    : get_nearest_poi(_center.unitx, _center.unity, &poi) ))
        {
            /* Auto-Center is enabled - use the GPS position. */
            Point unit;
            latlon2unit(poi.lat, poi.lon, unit.unitx, unit.unity);
            banner = g_strdup_printf("%s (%s)", poi.label, poi.clabel);
            MACRO_BANNER_SHOW_INFO(_window, banner);
            g_free(banner);
            g_free(poi.label);
            g_free(poi.desc);
            g_free(poi.clabel);

            if(_center_mode > 0)
                gtk_check_menu_item_set_active(
                        GTK_CHECK_MENU_ITEM(_menu_view_ac_none_item), TRUE);

            map_center_unit(unit);
        }
        else
        {
            MACRO_BANNER_SHOW_INFO(_window, _("No POIs found."));
            /* Auto-Center is disabled - use the view center. */
        }
    }

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

/****************************************************************************
 * ABOVE: VIEW/GOTO MENU ****************************************************
 ****************************************************************************/

/****************************************************************************
 * BELOW: VIEW/SHOW MENU ****************************************************
 ****************************************************************************/

static gboolean
menu_cb_view_show_tracks(GtkMenuItem *item)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    _show_paths ^= TRACKS_MASK;
    if(gtk_check_menu_item_get_active(
                GTK_CHECK_MENU_ITEM(_menu_view_show_tracks_item)))
    {
        _show_paths |= TRACKS_MASK;
        map_render_paths();
        MACRO_QUEUE_DRAW_AREA();
        MACRO_BANNER_SHOW_INFO(_window, _("Tracks are now shown"));
    }
    else
    {
        _show_paths &= ~TRACKS_MASK;
        map_force_redraw();
        MACRO_BANNER_SHOW_INFO(_window, _("Tracks are now hidden"));
    }

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_view_show_zoomlevel(GtkMenuItem *item)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    _show_zoomlevel = gtk_check_menu_item_get_active(
                GTK_CHECK_MENU_ITEM(_menu_view_show_zoomlevel_item));
    map_force_redraw();

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_view_show_scale(GtkMenuItem *item)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    _show_scale = gtk_check_menu_item_get_active(
                GTK_CHECK_MENU_ITEM(_menu_view_show_scale_item));
    map_force_redraw();

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_view_show_comprose(GtkMenuItem *item)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    _show_comprose = gtk_check_menu_item_get_active(
                GTK_CHECK_MENU_ITEM(_menu_view_show_comprose_item));
    map_force_redraw();

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_view_show_routes(GtkMenuItem *item)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(gtk_check_menu_item_get_active(
                GTK_CHECK_MENU_ITEM(_menu_view_show_routes_item)))
    {
        _show_paths |= ROUTES_MASK;
        map_render_paths();
        MACRO_QUEUE_DRAW_AREA();
        MACRO_BANNER_SHOW_INFO(_window, _("Routes are now shown"));
    }
    else
    {
        _show_paths &= ~ROUTES_MASK;
        map_force_redraw();
        MACRO_BANNER_SHOW_INFO(_window, _("Routes are now hidden"));
    }

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_view_show_velvec(GtkMenuItem *item)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    _show_velvec = gtk_check_menu_item_get_active(
            GTK_CHECK_MENU_ITEM(_menu_view_show_velvec_item));
    map_move_mark();

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_view_show_poi(GtkMenuItem *item)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    _show_poi = gtk_check_menu_item_get_active(
            GTK_CHECK_MENU_ITEM(_menu_view_show_poi_item));
    map_force_redraw();

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

/****************************************************************************
 * ABOVE: VIEW/SHOW MENU ****************************************************
 ****************************************************************************/

/****************************************************************************
 * BELOW: VIEW/AUTO-CENTER MENU *********************************************
 ****************************************************************************/

static gboolean
menu_cb_view_ac_lead(GtkMenuItem *item)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(gtk_check_menu_item_get_active(
                GTK_CHECK_MENU_ITEM(_menu_view_ac_lead_item)))
    {
        _center_mode = CENTER_LEAD;
        MACRO_BANNER_SHOW_INFO(_window, _("Auto-Center Mode: Lead"));
        map_refresh_mark(TRUE);
    }

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_view_ac_latlon(GtkMenuItem *item)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(gtk_check_menu_item_get_active(
                GTK_CHECK_MENU_ITEM(_menu_view_ac_latlon_item)))
    {
        _center_mode = CENTER_LATLON;
        MACRO_BANNER_SHOW_INFO(_window, _("Auto-Center Mode: Lat/Lon"));
        map_refresh_mark(TRUE);
    }

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_view_ac_none(GtkMenuItem *item)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(gtk_check_menu_item_get_active(
                GTK_CHECK_MENU_ITEM(_menu_view_ac_none_item)))
    {
        _center_mode = -_center_mode;
        MACRO_BANNER_SHOW_INFO(_window, _("Auto-Center Off"));
    }

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

/****************************************************************************
 * ABOVE: VIEW/AUTO-CENTER MENU *********************************************
 ****************************************************************************/

static gboolean
menu_cb_view_fullscreen(GtkMenuItem *item)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if((_fullscreen = gtk_check_menu_item_get_active(
                GTK_CHECK_MENU_ITEM(_menu_view_fullscreen_item))))
        gtk_window_fullscreen(GTK_WINDOW(_window));
    else
        gtk_window_unfullscreen(GTK_WINDOW(_window));

    g_idle_add((GSourceFunc)window_present, NULL);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

/****************************************************************************
 * BELOW: GPS MENU **********************************************************
 ****************************************************************************/

static gboolean
menu_cb_gps_enable(GtkMenuItem *item)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if((_enable_gps = gtk_check_menu_item_get_active(
                GTK_CHECK_MENU_ITEM(_menu_gps_enable_item))))
        rcvr_connect();
    else
        rcvr_disconnect();

    map_move_mark();
    gps_show_info();
    gtk_widget_set_sensitive(GTK_WIDGET(_menu_gps_details_item), _enable_gps);
    gtk_widget_set_sensitive(GTK_WIDGET(_menu_gps_reset_item), _enable_gps);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_gps_show_info(GtkMenuItem *item)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    _gps_info = gtk_check_menu_item_get_active(
                GTK_CHECK_MENU_ITEM(_menu_gps_show_info_item));

    gps_show_info();

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_gps_details(GtkMenuItem *item)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    gps_details();

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_gps_reset(GtkMenuItem *item)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    reset_bluetooth();

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

/****************************************************************************
 * ABOVE: GPS MENU **********************************************************
 ****************************************************************************/

static gboolean
menu_cb_settings(GtkMenuItem *item)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(settings_dialog())
    {
        /* Settings have changed - reconnect to receiver. */
        if(_enable_gps)
        {
            rcvr_disconnect();
            rcvr_connect();
        }
    }

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_help(GtkMenuItem *item)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

#ifndef LEGACY
    hildon_help_show(_osso, HELP_ID_INTRO, 0);
#else
    ossohelp_show(_osso, HELP_ID_INTRO, 0);
#endif

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
menu_cb_about(GtkMenuItem *item)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

#ifndef LEGACY
    hildon_help_show(_osso, HELP_ID_ABOUT, HILDON_HELP_SHOW_DIALOG);
#else
    ossohelp_show(_osso, HELP_ID_ABOUT, OSSO_HELP_SHOW_DIALOG);
#endif

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}



void
menu_maps_remove_repos()
{
    GList *curr;
    printf("%s()\n", __PRETTY_FUNCTION__);

    /* Delete one menu item for each repo. */
    for(curr = _repo_list; curr; curr = curr->next)
    {
        gtk_widget_destroy(gtk_container_get_children(
                    GTK_CONTAINER(_menu_maps_submenu))->data);
        if (gtk_container_get_children(GTK_CONTAINER(_menu_layers_submenu)))
            gtk_widget_destroy(gtk_container_get_children(GTK_CONTAINER(_menu_layers_submenu))->data);
    }
    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

void
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
                GTK_CHECK_MENU_ITEM(menu_item), rd == _curr_repo);
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


void
menu_layers_add_repos()
{
    GList *curr;
    printf("%s()\n", __PRETTY_FUNCTION__);

    for(curr = _repo_list; curr; curr = curr->next)
    {
        RepoData* rd = (RepoData*)curr->data;
        GtkWidget *item, *submenu = NULL, *layer_item;

        /* if repository doesn't have layers, skip it */
        if (!rd->layers)
            continue;

        /* append main repository menu item  */
        gtk_menu_append (_menu_layers_submenu, item = gtk_menu_item_new_with_label(rd->name));

        rd = rd->layers;
        while (rd) {
            if (!submenu)
                submenu = gtk_menu_new ();
            gtk_menu_append (submenu, layer_item = gtk_check_menu_item_new_with_label (rd->name));
            gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (layer_item), rd->layer_enabled);
            g_signal_connect (G_OBJECT (layer_item), "toggled", G_CALLBACK (menu_cb_layers_toggle), rd);
            rd->menu_item = layer_item;
            rd = rd->layers;
        }

        if (submenu)
            gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), submenu);
    }

    gtk_widget_show_all(_menu_layers_submenu);
    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}


/**
 * Create the menu items needed for the drop down menu.
 */
void
menu_init()
{
    /* Create needed handles. */
    GtkMenu *menu;
    GtkWidget *submenu;
    GtkWidget *submenu2;
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
    gtk_menu_append(submenu, _menu_route_distnext_item
        = gtk_menu_item_new_with_label(_("Show Distance to Next Waypoint")));
    gtk_menu_append(submenu, _menu_route_distlast_item
        = gtk_menu_item_new_with_label(_("Show Distance to End of Route")));
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
    gtk_menu_append(submenu, _menu_track_insert_break_item
            = gtk_menu_item_new_with_label(_("Insert Break")));
    gtk_menu_append(submenu, _menu_track_insert_mark_item
            = gtk_menu_item_new_with_label(_("Insert Mark...")));
    gtk_menu_append(submenu, _menu_track_distlast_item
            = gtk_menu_item_new_with_label(_("Show Distance from Last Mark")));
    gtk_menu_append(submenu, _menu_track_distfirst_item
            = gtk_menu_item_new_with_label(_("Show Distance from Beginning")));
    gtk_menu_append(submenu, _menu_track_clear_item
            = gtk_menu_item_new_with_label(_("Clear")));
    gtk_menu_append(submenu, _menu_track_enable_tracking_item
            = gtk_check_menu_item_new_with_label(_("Enable Tracking")));
    gtk_check_menu_item_set_active(
            GTK_CHECK_MENU_ITEM(_menu_track_enable_tracking_item), _enable_tracking);

    /* The "POI" submenu. */
    gtk_menu_append(menu, menu_item = _menu_poi_item
            = gtk_menu_item_new_with_label(_("POI")));
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item),
            submenu = gtk_menu_new());
    gtk_menu_append(submenu, _menu_poi_import_item
            = gtk_menu_item_new_with_label(_("Import...")));
    gtk_menu_append(submenu, _menu_poi_download_item
            = gtk_menu_item_new_with_label(_("Download...")));
    gtk_menu_append(submenu, _menu_poi_browse_item
            = gtk_menu_item_new_with_label(_("Browse...")));
    gtk_menu_append(submenu, _menu_poi_categories_item
            = gtk_menu_item_new_with_label(_("Categories...")));

    /* The "Maps" submenu. */
    gtk_menu_append(menu, menu_item
            = gtk_menu_item_new_with_label(_("Maps")));
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item),
            _menu_maps_submenu = gtk_menu_new());
    gtk_menu_append(_menu_maps_submenu, gtk_separator_menu_item_new());
    gtk_menu_append(_menu_maps_submenu, _menu_maps_mapman_item
            = gtk_menu_item_new_with_label(_("Manage Maps...")));
    gtk_menu_append(_menu_maps_submenu, _menu_maps_repoman_item
            = gtk_menu_item_new_with_label(_("Manage Repositories...")));
    gtk_menu_append(_menu_maps_submenu, _menu_maps_auto_download_item
            = gtk_check_menu_item_new_with_label(_("Auto-Download")));
    gtk_check_menu_item_set_active(
            GTK_CHECK_MENU_ITEM(_menu_maps_auto_download_item),_auto_download);
    menu_maps_add_repos();

    gtk_menu_append(menu, menu_item
            = gtk_menu_item_new_with_label(_("Layers")));
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item),
            _menu_layers_submenu = gtk_menu_new());
    menu_layers_add_repos();

    gtk_menu_append(menu, gtk_separator_menu_item_new());

    /* The "View" submenu. */
    gtk_menu_append(menu, menu_item
            = gtk_menu_item_new_with_label(_("View")));
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item),
            submenu = gtk_menu_new());

    /* The "View"/"Zoom" submenu. */
    gtk_menu_append(submenu, menu_item
            = gtk_menu_item_new_with_label(_("Zoom")));
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item),
            submenu2 = gtk_menu_new());
    gtk_menu_append(submenu2, _menu_view_zoom_in_item
            = gtk_menu_item_new_with_label(_("Zoom In")));
    gtk_menu_append(submenu2, _menu_view_zoom_out_item
            = gtk_menu_item_new_with_label(_("Zoom Out")));

    /* The "View"/"Rotate" submenu. */
    gtk_menu_append(submenu, menu_item
            = gtk_menu_item_new_with_label(_("Rotate")));
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item),
            submenu2 = gtk_menu_new());
    gtk_menu_append(submenu2, _menu_view_rotate_clock_item
            = gtk_menu_item_new_with_label(_("Clockwise")));
    gtk_menu_append(submenu2, _menu_view_rotate_counter_item
            = gtk_menu_item_new_with_label(_("Counter")));
    gtk_menu_append(submenu2, gtk_separator_menu_item_new());
    gtk_menu_append(submenu2, _menu_view_rotate_reset_item
            = gtk_menu_item_new_with_label(_("Reset")));
    gtk_menu_append(submenu2, _menu_view_rotate_auto_item
            = gtk_check_menu_item_new_with_label(_("Auto-Rotate")));
    gtk_check_menu_item_set_active(
            GTK_CHECK_MENU_ITEM(_menu_view_rotate_auto_item), _center_rotate);

    /* The "View"/"Pan" submenu. */
    gtk_menu_append(submenu, menu_item
            = gtk_menu_item_new_with_label(_("Pan")));
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item),
            submenu2 = gtk_menu_new());
    gtk_menu_append(submenu2, _menu_view_pan_up_item
            = gtk_menu_item_new_with_label(_("Up")));
    gtk_menu_append(submenu2, _menu_view_pan_down_item
            = gtk_menu_item_new_with_label(_("Down")));
    gtk_menu_append(submenu2, _menu_view_pan_left_item
            = gtk_menu_item_new_with_label(_("Left")));
    gtk_menu_append(submenu2, _menu_view_pan_right_item
            = gtk_menu_item_new_with_label(_("Right")));
    gtk_menu_append(submenu2, gtk_separator_menu_item_new());
    gtk_menu_append(submenu2, _menu_view_pan_north_item
            = gtk_menu_item_new_with_label(_("North")));
    gtk_menu_append(submenu2, _menu_view_pan_south_item
            = gtk_menu_item_new_with_label(_("South")));
    gtk_menu_append(submenu2, _menu_view_pan_west_item
            = gtk_menu_item_new_with_label(_("West")));
    gtk_menu_append(submenu2, _menu_view_pan_east_item
            = gtk_menu_item_new_with_label(_("East")));

    /* The "Go to" submenu. */
    gtk_menu_append(submenu, menu_item
            = gtk_menu_item_new_with_label(_("Go to")));
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item),
            submenu2 = gtk_menu_new());
    gtk_menu_append(submenu2, _menu_view_goto_latlon_item
            = gtk_menu_item_new_with_label(_("Lat/Lon...")));
    gtk_menu_append(submenu2, _menu_view_goto_address_item
            = gtk_menu_item_new_with_label(_("Address...")));
    gtk_menu_append(submenu2, _menu_view_goto_gps_item
            = gtk_menu_item_new_with_label(_("GPS Location")));
    gtk_menu_append(submenu2, _menu_view_goto_nextway_item
            = gtk_menu_item_new_with_label(_("Next Waypoint")));
    gtk_menu_append(submenu2, _menu_view_goto_nearpoi_item
            = gtk_menu_item_new_with_label(_("Nearest POI")));

    gtk_menu_append(submenu, gtk_separator_menu_item_new());

    /* The "View"/"Show" submenu. */
    gtk_menu_append(submenu, menu_item
            = gtk_menu_item_new_with_label(_("Show")));
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item),
            submenu2 = gtk_menu_new());
    gtk_menu_append(submenu2, _menu_view_show_zoomlevel_item
            = gtk_check_menu_item_new_with_label(_("Zoom Level")));
    gtk_check_menu_item_set_active(
            GTK_CHECK_MENU_ITEM(_menu_view_show_zoomlevel_item),
            _show_zoomlevel);
    gtk_menu_append(submenu2, _menu_view_show_scale_item
            = gtk_check_menu_item_new_with_label(_("Scale")));
    gtk_check_menu_item_set_active(
            GTK_CHECK_MENU_ITEM(_menu_view_show_scale_item),
            _show_scale);
    gtk_menu_append(submenu2, _menu_view_show_comprose_item
            = gtk_check_menu_item_new_with_label(_("Compass Rose")));
    gtk_check_menu_item_set_active(
            GTK_CHECK_MENU_ITEM(_menu_view_show_comprose_item),
            _show_comprose);
    gtk_menu_append(submenu2, _menu_view_show_routes_item
            = gtk_check_menu_item_new_with_label(_("Route")));
    gtk_check_menu_item_set_active(
            GTK_CHECK_MENU_ITEM(_menu_view_show_routes_item),
            _show_paths & ROUTES_MASK);
    gtk_menu_append(submenu2, _menu_view_show_tracks_item
            = gtk_check_menu_item_new_with_label(_("Track")));
    gtk_check_menu_item_set_active(
            GTK_CHECK_MENU_ITEM(_menu_view_show_tracks_item),
            _show_paths & TRACKS_MASK);
    gtk_menu_append(submenu2, _menu_view_show_velvec_item
            = gtk_check_menu_item_new_with_label(_("Velocity Vector")));
    gtk_check_menu_item_set_active(
            GTK_CHECK_MENU_ITEM(_menu_view_show_velvec_item), _show_velvec);
    gtk_menu_append(submenu2, _menu_view_show_poi_item
            = gtk_check_menu_item_new_with_label(_("POI")));
    gtk_check_menu_item_set_active(
            GTK_CHECK_MENU_ITEM(_menu_view_show_poi_item), _show_poi);

    /* The "View"/"Auto-Center" submenu. */
    gtk_menu_append(submenu, menu_item
            = gtk_menu_item_new_with_label(_("Auto-Center")));
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item),
            submenu2 = gtk_menu_new());
    gtk_menu_append(submenu2, _menu_view_ac_latlon_item
            = gtk_radio_menu_item_new_with_label(NULL, _("Lat/Lon")));
    gtk_check_menu_item_set_active(
            GTK_CHECK_MENU_ITEM(_menu_view_ac_latlon_item),
            _center_mode == CENTER_LATLON);
    gtk_menu_append(submenu2, _menu_view_ac_lead_item
            = gtk_radio_menu_item_new_with_label_from_widget(
                GTK_RADIO_MENU_ITEM(_menu_view_ac_latlon_item), _("Lead")));
    gtk_check_menu_item_set_active(
            GTK_CHECK_MENU_ITEM(_menu_view_ac_lead_item),
            _center_mode == CENTER_LEAD);
    gtk_menu_append(submenu2, _menu_view_ac_none_item
            = gtk_radio_menu_item_new_with_label_from_widget(
                GTK_RADIO_MENU_ITEM(_menu_view_ac_latlon_item), _("None")));
    gtk_check_menu_item_set_active(
            GTK_CHECK_MENU_ITEM(_menu_view_ac_none_item),
            _center_mode < 0);

    gtk_menu_append(submenu, gtk_separator_menu_item_new());

    gtk_menu_append(submenu, _menu_view_fullscreen_item
            = gtk_check_menu_item_new_with_label(_("Full Screen")));
    gtk_check_menu_item_set_active(
            GTK_CHECK_MENU_ITEM(_menu_view_fullscreen_item), _fullscreen);

    /* The "GPS" submenu. */
    gtk_menu_append(menu, menu_item
            = gtk_menu_item_new_with_label(_("GPS")));
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item),
            submenu = gtk_menu_new());
    gtk_menu_append(submenu, _menu_gps_enable_item
            = gtk_check_menu_item_new_with_label(_("Enable GPS")));
    gtk_check_menu_item_set_active(
            GTK_CHECK_MENU_ITEM(_menu_gps_enable_item), _enable_gps);
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

    /* The other menu items. */
    gtk_menu_append(menu, _menu_settings_item
        = gtk_menu_item_new_with_label(_("Settings...")));
    gtk_menu_append(menu, gtk_separator_menu_item_new());
    gtk_menu_append(menu, _menu_help_item
        = gtk_menu_item_new_with_label(_("Help...")));
    gtk_menu_append(menu, _menu_about_item
        = gtk_menu_item_new_with_label(_("About...")));
    gtk_menu_append(menu, _menu_close_item
        = gtk_menu_item_new_with_label(_("Close")));

    /* We need to show menu items. */
    gtk_widget_show_all(GTK_WIDGET(menu));

    hildon_window_set_menu(HILDON_WINDOW(_window), menu);

    /* Connect the "Route" signals. */
    g_signal_connect(G_OBJECT(_menu_route_open_item), "activate",
                      G_CALLBACK(menu_cb_route_open), NULL);
    g_signal_connect(G_OBJECT(_menu_route_download_item), "activate",
                      G_CALLBACK(menu_cb_route_download), NULL);
    g_signal_connect(G_OBJECT(_menu_route_save_item), "activate",
                      G_CALLBACK(menu_cb_route_save), NULL);
    g_signal_connect(G_OBJECT(_menu_route_distnext_item), "activate",
                      G_CALLBACK(menu_cb_route_distnext), NULL);
    g_signal_connect(G_OBJECT(_menu_route_distlast_item), "activate",
                      G_CALLBACK(menu_cb_route_distlast), NULL);
    g_signal_connect(G_OBJECT(_menu_route_reset_item), "activate",
                      G_CALLBACK(menu_cb_route_reset), NULL);
    g_signal_connect(G_OBJECT(_menu_route_clear_item), "activate",
                      G_CALLBACK(menu_cb_route_clear), NULL);

    /* Connect the "Track" signals. */
    g_signal_connect(G_OBJECT(_menu_track_open_item), "activate",
                      G_CALLBACK(menu_cb_track_open), NULL);
    g_signal_connect(G_OBJECT(_menu_track_save_item), "activate",
                      G_CALLBACK(menu_cb_track_save), NULL);
    g_signal_connect(G_OBJECT(_menu_track_insert_break_item), "activate",
                      G_CALLBACK(menu_cb_track_insert_break), NULL);
    g_signal_connect(G_OBJECT(_menu_track_insert_mark_item), "activate",
                      G_CALLBACK(menu_cb_track_insert_mark), NULL);
    g_signal_connect(G_OBJECT(_menu_track_distlast_item), "activate",
                      G_CALLBACK(menu_cb_track_distlast), NULL);
    g_signal_connect(G_OBJECT(_menu_track_distfirst_item), "activate",
                      G_CALLBACK(menu_cb_track_distfirst), NULL);
    g_signal_connect(G_OBJECT(_menu_track_clear_item), "activate",
                      G_CALLBACK(menu_cb_track_clear), NULL);
    g_signal_connect(G_OBJECT(_menu_track_enable_tracking_item), "toggled",
                      G_CALLBACK(menu_cb_track_enable_tracking), NULL);

    /* Connect the "POI" signals. */
    g_signal_connect(G_OBJECT(_menu_poi_import_item), "activate",
                      G_CALLBACK(menu_cb_poi_import), NULL);
    g_signal_connect(G_OBJECT(_menu_poi_download_item), "activate",
                      G_CALLBACK(menu_cb_poi_download), NULL);
    g_signal_connect(G_OBJECT(_menu_poi_browse_item), "activate",
                      G_CALLBACK(menu_cb_poi_browse), NULL);
    g_signal_connect(G_OBJECT(_menu_poi_categories_item), "activate",
                      G_CALLBACK(menu_cb_poi_categories), NULL);

    /* Connect the "Maps" signals. */
    g_signal_connect(G_OBJECT(_menu_maps_repoman_item), "activate",
                      G_CALLBACK(menu_cb_maps_repoman), NULL);
    g_signal_connect(G_OBJECT(_menu_maps_mapman_item), "activate",
                      G_CALLBACK(menu_cb_maps_mapman), NULL);
    g_signal_connect(G_OBJECT(_menu_maps_auto_download_item), "toggled",
                      G_CALLBACK(menu_cb_maps_auto_download), NULL);

    /* Connect the "View" signals. */
    g_signal_connect(G_OBJECT(_menu_view_zoom_in_item), "activate",
                      G_CALLBACK(menu_cb_view_zoom_in), NULL);
    g_signal_connect(G_OBJECT(_menu_view_zoom_out_item), "activate",
                      G_CALLBACK(menu_cb_view_zoom_out), NULL);

    g_signal_connect(G_OBJECT(_menu_view_rotate_clock_item), "activate",
                      G_CALLBACK(menu_cb_view_rotate_clock), NULL);
    g_signal_connect(G_OBJECT(_menu_view_rotate_counter_item), "activate",
                      G_CALLBACK(menu_cb_view_rotate_counter), NULL);
    g_signal_connect(G_OBJECT(_menu_view_rotate_reset_item), "activate",
                      G_CALLBACK(menu_cb_view_rotate_reset), NULL);
    g_signal_connect(G_OBJECT(_menu_view_rotate_auto_item), "toggled",
                      G_CALLBACK(menu_cb_view_rotate_auto), NULL);

    g_signal_connect(G_OBJECT(_menu_view_pan_up_item), "activate",
                      G_CALLBACK(menu_cb_view_pan_up), NULL);
    g_signal_connect(G_OBJECT(_menu_view_pan_down_item), "activate",
                      G_CALLBACK(menu_cb_view_pan_down), NULL);
    g_signal_connect(G_OBJECT(_menu_view_pan_left_item), "activate",
                      G_CALLBACK(menu_cb_view_pan_left), NULL);
    g_signal_connect(G_OBJECT(_menu_view_pan_right_item), "activate",
                      G_CALLBACK(menu_cb_view_pan_right), NULL);
    g_signal_connect(G_OBJECT(_menu_view_pan_north_item), "activate",
                      G_CALLBACK(menu_cb_view_pan_north), NULL);
    g_signal_connect(G_OBJECT(_menu_view_pan_south_item), "activate",
                      G_CALLBACK(menu_cb_view_pan_south), NULL);
    g_signal_connect(G_OBJECT(_menu_view_pan_west_item), "activate",
                      G_CALLBACK(menu_cb_view_pan_west), NULL);
    g_signal_connect(G_OBJECT(_menu_view_pan_east_item), "activate",
                      G_CALLBACK(menu_cb_view_pan_east), NULL);

    g_signal_connect(G_OBJECT(_menu_view_goto_latlon_item), "activate",
                      G_CALLBACK(menu_cb_view_goto_latlon), NULL);
    g_signal_connect(G_OBJECT(_menu_view_goto_address_item), "activate",
                      G_CALLBACK(menu_cb_view_goto_address), NULL);
    g_signal_connect(G_OBJECT(_menu_view_goto_gps_item), "activate",
                      G_CALLBACK(menu_cb_view_goto_gps), NULL);
    g_signal_connect(G_OBJECT(_menu_view_goto_nextway_item), "activate",
                      G_CALLBACK(menu_cb_view_goto_nextway), NULL);
    g_signal_connect(G_OBJECT(_menu_view_goto_nearpoi_item), "activate",
                      G_CALLBACK(menu_cb_view_goto_nearpoi), NULL);

    g_signal_connect(G_OBJECT(_menu_view_show_tracks_item), "toggled",
                      G_CALLBACK(menu_cb_view_show_tracks), NULL);
    g_signal_connect(G_OBJECT(_menu_view_show_zoomlevel_item), "toggled",
                      G_CALLBACK(menu_cb_view_show_zoomlevel), NULL);
    g_signal_connect(G_OBJECT(_menu_view_show_scale_item), "toggled",
                      G_CALLBACK(menu_cb_view_show_scale), NULL);
    g_signal_connect(G_OBJECT(_menu_view_show_comprose_item), "toggled",
                      G_CALLBACK(menu_cb_view_show_comprose), NULL);
    g_signal_connect(G_OBJECT(_menu_view_show_routes_item), "toggled",
                      G_CALLBACK(menu_cb_view_show_routes), NULL);
    g_signal_connect(G_OBJECT(_menu_view_show_velvec_item), "toggled",
                      G_CALLBACK(menu_cb_view_show_velvec), NULL);
    g_signal_connect(G_OBJECT(_menu_view_show_poi_item), "toggled",
                      G_CALLBACK(menu_cb_view_show_poi), NULL);

    g_signal_connect(G_OBJECT(_menu_view_ac_latlon_item), "toggled",
                      G_CALLBACK(menu_cb_view_ac_latlon), NULL);
    g_signal_connect(G_OBJECT(_menu_view_ac_lead_item), "toggled",
                      G_CALLBACK(menu_cb_view_ac_lead), NULL);
    g_signal_connect(G_OBJECT(_menu_view_ac_none_item), "toggled",
                      G_CALLBACK(menu_cb_view_ac_none), NULL);

    g_signal_connect(G_OBJECT(_menu_view_fullscreen_item), "toggled",
                      G_CALLBACK(menu_cb_view_fullscreen), NULL);

    /* Connect the "GPS" signals. */
    g_signal_connect(G_OBJECT(_menu_gps_enable_item), "toggled",
                      G_CALLBACK(menu_cb_gps_enable), NULL);
    g_signal_connect(G_OBJECT(_menu_gps_show_info_item), "toggled",
                      G_CALLBACK(menu_cb_gps_show_info), NULL);
    g_signal_connect(G_OBJECT(_menu_gps_details_item), "activate",
                      G_CALLBACK(menu_cb_gps_details), NULL);
    g_signal_connect(G_OBJECT(_menu_gps_reset_item), "activate",
                      G_CALLBACK(menu_cb_gps_reset), NULL);

    /* Connect the other menu item signals. */
    g_signal_connect(G_OBJECT(_menu_settings_item), "activate",
                      G_CALLBACK(menu_cb_settings), NULL);
    g_signal_connect(G_OBJECT(_menu_help_item), "activate",
                      G_CALLBACK(menu_cb_help), NULL);
    g_signal_connect(G_OBJECT(_menu_about_item), "activate",
                      G_CALLBACK(menu_cb_about), NULL);
    g_signal_connect(G_OBJECT(_menu_close_item), "activate",
                      G_CALLBACK(gtk_main_quit), NULL);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

