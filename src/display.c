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

#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef LEGACY
#    include <hildon/hildon-help.h>
#    include <hildon/hildon-note.h>
#    include <hildon/hildon-file-chooser-dialog.h>
#    include <hildon/hildon-banner.h>
#    include <hildon/hildon-sound.h>
#else
#    include <osso-helplib.h>
#    include <hildon-widgets/hildon-note.h>
#    include <hildon-widgets/hildon-file-chooser-dialog.h>
#    include <hildon-widgets/hildon-banner.h>
#    include <hildon-widgets/hildon-system-sound.h>
#endif

#include "types.h"
#include "data.h"
#include "defines.h"

#include "dbus-ifc.h"
#include "display.h"
#include "gdk-pixbuf-rotate.h"
#include "gps.h"
#include "maps.h"
#include "path.h"
#include "poi.h"
#include "settings.h"
#include "util.h"

#define VELVEC_SIZE_FACTOR (4)

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
static gint _redraw_count = 0;

static gint _mark_bufx1 = -1;
static gint _mark_bufx2 = -1;
static gint _mark_bufy1 = -1;
static gint _mark_bufy2 = -1;
static gint _mark_minx = -1;
static gint _mark_miny = -1;
static gint _mark_width = -1;
static gint _mark_height = -1;
static GdkRectangle _scale_rect = { 0, 0, 0, 0};
static GdkRectangle _zoom_rect = { 0, 0, 0, 0};
static gint _dl_errors = 0;

static volatile gint _pending_replaces = 0;

/** Pango stuff. */
GdkRectangle _comprose_rect = { 0, 0, 0, 0};
PangoLayout *_scale_layout = NULL;
PangoLayout *_zoom_layout = NULL;
PangoLayout *_comprose_layout = NULL;
GdkGC *_speed_limit_gc1 = NULL;
GdkGC *_speed_limit_gc2 = NULL;
PangoLayout *_speed_limit_layout = NULL;
PangoLayout *_sat_panel_layout = NULL;
PangoLayout *_heading_panel_layout = NULL;
PangoFontDescription *_heading_panel_fontdesc = NULL;
GdkGC *_sat_info_gc1 = NULL;
GdkGC *_sat_info_gc2 = NULL;
PangoLayout *_sat_info_layout = NULL;
PangoLayout *_sat_details_layout = NULL;
PangoLayout *_sat_details_expose_layout = NULL;

#define SCALE_WIDTH (100)

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
    GdkGC *gc;
    gfloat cur_speed;
    gchar *buffer;
    static gint x = 0, y = 0, width = 0, height = 0;
    printf("%s()\n", __PRETTY_FUNCTION__);

    cur_speed = _gps.speed * UNITS_CONVERT[_units];

    if(cur_speed > _speed_limit)
    {
        gc = _speed_limit_gc1;
        if(!_speed_excess)
        {
            _speed_excess = TRUE;
            g_timeout_add_full(G_PRIORITY_HIGH_IDLE,
                    5000, (GSourceFunc)speed_excess, NULL, NULL);
        }
    }
    else
    {
        gc = _speed_limit_gc2;
        _speed_excess = FALSE;
    }

    /* remove previous number */
    if (width != 0 && height != 0) {
      gtk_widget_queue_draw_area (_map_widget,
                                 x - 5,
                                 y - 5,
                                 width + 10,
                                 height + 10);
      gdk_window_process_all_updates();
    }

    buffer = g_strdup_printf("%0.0f", cur_speed);
    pango_layout_set_text(_speed_limit_layout, buffer, -1);


    pango_layout_get_pixel_size(_speed_limit_layout, &width, &height);

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
        _speed_limit_layout);
    g_free(buffer);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

gboolean
gps_display_details(void)
{
    gchar *buffer, litbuf[16];
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
        lat_format(_gps.lat, litbuf);
        gtk_label_set_label(GTK_LABEL(_sdi_lat), litbuf);

        /* longitude */
        lon_format(_gps.lon, litbuf);
        gtk_label_set_label(GTK_LABEL(_sdi_lon), litbuf);

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
                buffer = g_strdup_printf("%d ft",
                        (gint)(_pos.altitude * 3.2808399f));
                break;
            default:
                buffer = g_strdup_printf("%d m", _pos.altitude);
                break;
        }
        gtk_label_set_label(GTK_LABEL(_sdi_alt), buffer);
        g_free(buffer);

        /* heading */
        buffer = g_strdup_printf("%0.0f°", _gps.heading);
        gtk_label_set_label(GTK_LABEL(_sdi_hea), buffer);
        g_free(buffer);

        /* local time */
        strftime(litbuf, 15, "%X", localtime(&_pos.time));
        gtk_label_set_label(GTK_LABEL(_sdi_tim), litbuf);
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
            case 7 : buffer = g_strdup_printf(_("Manual")); break;
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

void
gps_display_data(void)
{
    gchar *buffer, litbuf[LL_FMT_LEN];
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
        lat_format(_gps.lat, litbuf);
        gtk_label_set_label(GTK_LABEL(_text_lat), litbuf);

        /* longitude */
        lon_format(_gps.lon, litbuf);
        gtk_label_set_label(GTK_LABEL(_text_lon), litbuf);

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
                buffer = g_strdup_printf("Alt: %d ft",
                        (gint)(_pos.altitude * 3.2808399f));
                break;
            default:
                buffer = g_strdup_printf("Alt: %d m", _pos.altitude);
        }
        gtk_label_set_label(GTK_LABEL(_text_alt), buffer);
        g_free(buffer);

        /* local time */
        strftime(litbuf, 15, "%X", localtime(&_pos.time));
        gtk_label_set_label(GTK_LABEL(_text_time), litbuf);
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

void
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

void
gps_show_info(void)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(_gps_info && _enable_gps)
        gtk_widget_show_all(GTK_WIDGET(_gps_widget));
    else
    {
        gps_hide_text();
        gtk_widget_hide(GTK_WIDGET(_gps_widget));
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

static void
draw_sat_info(GtkWidget *widget, gint x0, gint y0,
        gint width, gint height, gboolean showsnr)
{
    GdkGC *gc;
    gint step, i, j, snr_height, bymargin, xoffset, yoffset;
    gint x, y, x1, y1;
    gchar *tmp = NULL;
    printf("%s()\n", __PRETTY_FUNCTION__);

    xoffset = x0;
    yoffset = y0;
    /* Bootom margin - 12% */
    bymargin = height * 0.88f;

    /* Bottom line */
    gdk_draw_line(widget->window,
        widget->style->fg_gc[GTK_WIDGET_STATE(widget)],
        xoffset + 5, yoffset + bymargin,
        xoffset + width - 10 - 2, yoffset + bymargin);
    gdk_draw_line(widget->window,
        widget->style->fg_gc[GTK_WIDGET_STATE(widget)],
        xoffset + 5, yoffset + bymargin - 1,
        xoffset + width - 10 - 2, yoffset + bymargin - 1);

    if(_gps.satinview > 0 )
    {
        /* Left margin - 5pix, Right margin - 5pix */
        step = (width - 10) / _gps.satinview;

        for(i = 0; i < _gps.satinview; i++)
        {
            /* Sat used or not */
            gc = _sat_info_gc1;
            for(j = 0; j < _gps.satinuse ; j++)
            {
                if(_gps.satforfix[j] == _gps_sat[i].prn)
                {
                    gc = _sat_info_gc2;
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
                pango_layout_set_text(_sat_info_layout, tmp, 2);
                pango_layout_get_pixel_size(_sat_info_layout, &x1, &y1);
                gdk_draw_layout(widget->window,
                    widget->style->fg_gc[GTK_WIDGET_STATE(widget)],
                    xoffset + x + ((step - 2) - x1)/2,
                    yoffset + y - 15,
                    _sat_info_layout);
                g_free(tmp);
            }

            /* ...and sat number */
            tmp = g_strdup_printf("%02d", _gps_sat[i].prn);
            pango_layout_set_text(_sat_info_layout, tmp, 2);
            pango_layout_get_pixel_size(_sat_info_layout, &x1, &y1);
            gdk_draw_layout(widget->window,
                widget->style->fg_gc[GTK_WIDGET_STATE(widget)],
                xoffset + x + ((step - 2) - x1)/2 ,
                yoffset + bymargin + 1,
                _sat_info_layout);
            g_free(tmp);
        }
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return;
}

static void
draw_sat_details(GtkWidget *widget, gint x0, gint y0,
        gint width, gint height)
{
    gint i, j, x, y, size, halfsize, xoffset, yoffset;
    gint x1, y1;
    gfloat tmp;
    GdkColor color;
    GdkGC *gc1, *gc2, *gc3, *gc;
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

    gint line[12] = {0,30,60,90,120,150,180,210,240,270,300,330};

    for(i = 0; i < 6; i++)
    {
        /* line */
        tmp = deg2rad(line[i]);
        gdk_draw_line(widget->window,
            widget->style->fg_gc[GTK_WIDGET_STATE(widget)],
            xoffset + halfsize + (halfsize -2) * sinf(tmp),
            yoffset + halfsize - (halfsize -2) * cosf(tmp),
            xoffset + halfsize - (halfsize -2) * sinf(tmp),
            yoffset + halfsize + (halfsize -2) * cosf(tmp));
    }

    for(i = 0; i < 12; i++)
    {
        tmp = deg2rad(line[i]);
        /* azimuth */
        if(line[i] == 0)
            buffer = g_strdup_printf("N");
        else
            buffer = g_strdup_printf("%d°", line[i]);
        pango_layout_set_text(_sat_details_layout, buffer, -1);
        pango_layout_get_pixel_size(_sat_details_layout, &x, &y);
        gdk_draw_layout(widget->window,
            widget->style->fg_gc[GTK_WIDGET_STATE(widget)],
            (xoffset + halfsize + (halfsize - size/12) * sinf(tmp)) - x/2,
            (yoffset + halfsize - (halfsize - size/12) * cosf(tmp)) - y/2,
            _sat_details_layout);
        g_free(buffer);
    }

    /* elevation 30 */
    tmp = deg2rad(30);
    buffer = g_strdup_printf("30°");
    pango_layout_set_text(_sat_details_layout, buffer, -1);
    pango_layout_get_pixel_size(_sat_details_layout, &x, &y);
    gdk_draw_layout(widget->window,
        widget->style->fg_gc[GTK_WIDGET_STATE(widget)],
        (xoffset + halfsize + size/6*2 * sinf(tmp)) - x/2,
        (yoffset + halfsize - size/6*2 * cosf(tmp)) - y/2,
        _sat_details_layout);
    g_free(buffer);

    /* elevation 60 */
    tmp = deg2rad(30);
    buffer = g_strdup_printf("60°");
    pango_layout_set_text(_sat_details_layout, buffer, -1);
    pango_layout_get_pixel_size(_sat_details_layout, &x, &y);
    gdk_draw_layout(widget->window,
        widget->style->fg_gc[GTK_WIDGET_STATE(widget)],
        (xoffset + halfsize + size/6 * sinf(tmp)) - x/2,
        (yoffset + halfsize - size/6 * cosf(tmp)) - y/2,
        _sat_details_layout);
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

        tmp = deg2rad(_gps_sat[i].azimuth);
        x = xoffset + halfsize
            + (90 - _gps_sat[i].elevation)*halfsize/90 * sinf(tmp);
        y = yoffset + halfsize
            - (90 - _gps_sat[i].elevation)*halfsize/90 * cosf(tmp);

        gdk_draw_arc (widget->window,
            gc, TRUE,
            x - 10, y - 10, 20, 20,
            0, 64 * 360);

        buffer = g_strdup_printf("%02d", _gps_sat[i].prn);
        pango_layout_set_text(_sat_details_layout, buffer, -1);
        pango_layout_get_pixel_size(_sat_details_layout, &x1, &y1);
        gdk_draw_layout(widget->window,
            gc3,
            x - x1/2,
            y - y1/2,
            _sat_details_layout);
        g_free(buffer);
    }
    g_object_unref (gc1);
    g_object_unref (gc2);
    g_object_unref (gc3);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return;
}


static gboolean
sat_details_panel_expose(GtkWidget *widget, GdkEventExpose *event)
{
    gint width, height, x, y;
    gchar *buffer = NULL;
    printf("%s()\n", __PRETTY_FUNCTION__);

    width = widget->allocation.width;
    height = widget->allocation.height * 0.9;

    draw_sat_info(widget, 0, 0, width/2, height, TRUE);
    draw_sat_details(widget, width/2, 0, width/2, height);

    buffer = g_strdup_printf(
        "%s: %d; %s: %d",
        _("Satellites in view"), _gps.satinview,
        _("in use"), _gps.satinuse);
    pango_layout_set_text(_sat_details_expose_layout, buffer, -1);
    pango_layout_get_pixel_size(_sat_details_expose_layout, &x, &y);
    gdk_draw_layout(widget->window,
        widget->style->fg_gc[GTK_WIDGET_STATE(widget)],
        10,
        height*0.9 + 10,
        _sat_details_expose_layout);
    g_free(buffer);

    buffer = g_strdup_printf("HDOP: %.01f", _gps.hdop);
    pango_layout_set_text(_sat_details_expose_layout, buffer, -1);
    pango_layout_get_pixel_size(_sat_details_expose_layout, &x, &y);
    gdk_draw_layout(widget->window,
        widget->style->fg_gc[GTK_WIDGET_STATE(widget)],
        (width/8) - x/2,
        (height/6) - y/2,
        _sat_details_expose_layout);
    g_free(buffer);
    buffer = g_strdup_printf("PDOP: %.01f", _gps.pdop);
    pango_layout_set_text(_sat_details_expose_layout, buffer, -1);
    pango_layout_get_pixel_size(_sat_details_expose_layout, &x, &y);
    gdk_draw_layout(widget->window,
        widget->style->fg_gc[GTK_WIDGET_STATE(widget)],
        (width/8) - x/2,
        (height/6) - y/2 + 20,
        _sat_details_expose_layout);
    g_free(buffer);
    buffer = g_strdup_printf("VDOP: %.01f", _gps.vdop);
    pango_layout_set_text(_sat_details_expose_layout, buffer, -1);
    pango_layout_get_pixel_size(_sat_details_expose_layout, &x, &y);
    gdk_draw_layout(widget->window,
        widget->style->fg_gc[GTK_WIDGET_STATE(widget)],
        (width/8) - x/2,
        (height/6) - y/2 + 40,
        _sat_details_expose_layout);
    g_free(buffer);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return TRUE;
}

void
gps_details(void)
{
    static GtkWidget *dialog = NULL;
    static GtkWidget *table = NULL;
    static GtkWidget *label = NULL;
    static GtkWidget *notebook = NULL;
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(dialog == NULL)
    {
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
        gtk_misc_set_alignment(GTK_MISC(_sdi_fqu), 0.f, 0.5f);

        gtk_table_attach(GTK_TABLE(table),
                label = gtk_label_new(_("Max speed")),
                2, 3, 5, 6, GTK_EXPAND | GTK_FILL, 0, 20, 4);
        gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
        gtk_table_attach(GTK_TABLE(table),
                _sdi_msp = gtk_label_new(" --- "),
                3, 4, 5, 6, GTK_EXPAND | GTK_FILL, 0, 2, 4);
        gtk_misc_set_alignment(GTK_MISC(_sdi_msp), 0.f, 0.5f);
    }

    gtk_widget_show_all(dialog);
    _satdetails_on = TRUE;
    gps_display_details();
    while(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(dialog)))
    {
        _satdetails_on = FALSE;
        break;
    }
    gtk_widget_hide(dialog);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}


/**
 * Render a single track line to _map_pixmap.  If either point on the line
 * is a break (defined as unity == 0), a circle is drawn at the other point.
 * IT IS AN ERROR FOR BOTH POINTS TO INDICATE A BREAK.
 */
void
map_render_segment(GdkGC *gc_norm, GdkGC *gc_alt,
        gint unitx1, gint unity1, gint unitx2, gint unity2)
{
    /* vprintf("%s(%d, %d, %d, %d)\n", __PRETTY_FUNCTION__,
            unitx1, unity1, unitx2, unity2); */

    if(!unity1)
    {
        gint x2, y2;
        unit2buf(unitx2, unity2, x2, y2);
        if(((unsigned)(x2+_draw_width) <= _view_width_pixels+2*_draw_width)
         &&((unsigned)(y2+_draw_width) <= _view_height_pixels+2*_draw_width))
        {
            gdk_draw_arc(_map_pixmap, gc_alt,
                    FALSE, /* FALSE: not filled. */
                    x2 - _draw_width,
                    y2 - _draw_width,
                    2 * _draw_width,
                    2 * _draw_width,
                    0, /* start at 0 degrees. */
                    360 * 64);
        }
    }
    else if(!unity2)
    {
        gint x1, y1;
        unit2buf(unitx1, unity1, x1, y1);
        if(((unsigned)(x1+_draw_width) <= _view_width_pixels+2*_draw_width)
         &&((unsigned)(y1+_draw_width) <= _view_height_pixels+2*_draw_width))
        {
            gdk_draw_arc(_map_pixmap, gc_alt,
                    FALSE, /* FALSE: not filled. */
                    x1 - _draw_width,
                    y1 - _draw_width,
                    2 * _draw_width,
                    2 * _draw_width,
                    0, /* start at 0 degrees. */
                    360 * 64);
        }
    }
    else
    {
        gint x1, y1, x2, y2;
        unit2buf(unitx1, unity1, x1, y1);
        unit2buf(unitx2, unity2, x2, y2);
        /* Make sure this line could possibly be visible. */
        if(!((x1 > _view_width_pixels && x2 > _view_width_pixels)
                || (x1 < 0 && x2 < 0)
                || (y1 > _view_height_pixels && y2 > _view_height_pixels)
                || (y1 < 0 && y2 < 0)))
            gdk_draw_line(_map_pixmap, gc_norm, x1, y1, x2, y2);
    }

    /* vprintf("%s(): return\n", __PRETTY_FUNCTION__); */
}

/**
 * Render all track data onto the _map_pixmap.  Note that this does not
 * clear the pixmap of previous track data (use map_force_redraw() for
 * that), and also note that this method does not queue any redraws, so it
 * is up to the caller to decide which part of the track really needs to be
 * redrawn.
 */
static void
map_render_path(Path *path, GdkGC **gc)
{
    Point *curr;
    WayPoint *wcurr;
    printf("%s()\n", __PRETTY_FUNCTION__);

    /* gc is a pointer to the first GC to use (for plain points).  (gc + 1)
     * is a pointer to the GC to use for waypoints, and (gc + 2) is a pointer
     * to the GC to use for breaks. */

    /* else there is a route to draw. */
    for(curr = path->head, wcurr = path->whead; curr++ != path->tail; )
    {
        /* Draw the line from (curr - 1) to (curr). */
        map_render_segment(gc[0], gc[2],
                curr[-1].unitx, curr[-1].unity, curr->unitx, curr->unity);

        /* Now, check if curr is a waypoint. */
        if(wcurr <= path->wtail && wcurr->point == curr)
        {
            gint x1, y1;
            unit2buf(wcurr->point->unitx, wcurr->point->unity, x1, y1);
            gdk_draw_arc(_map_pixmap,
                    gc[1],
                    FALSE, /* FALSE: not filled. */
                    x1 - _draw_width,
                    y1 - _draw_width,
                    2 * _draw_width,
                    2 * _draw_width,
                    0, /* start at 0 degrees. */
                    360 * 64);
            wcurr++;
        }
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

void
map_render_paths()
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if((_show_paths & ROUTES_MASK) && _route.head != _route.tail)
    {
        WayPoint *next_way;
        map_render_path(&_route, _gc + COLORABLE_ROUTE);

        next_way = path_get_next_way();

        /* Now, draw the next waypoint on top of all other waypoints. */
        if(next_way)
        {
            gint x1, y1;
            unit2buf(next_way->point->unitx, next_way->point->unity, x1, y1);
            /* Draw the next waypoint as a break. */
            gdk_draw_arc(_map_pixmap,
                    _gc[COLORABLE_ROUTE_BREAK],
                    FALSE, /* FALSE: not filled. */
                    x1 - _draw_width,
                    y1 - _draw_width,
                    2 * _draw_width,
                    2 * _draw_width,
                    0, /* start at 0 degrees. */
                    360 * 64);
        }
    }
    if(_show_paths & TRACKS_MASK)
        map_render_path(&_track, _gc + COLORABLE_TRACK);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

/**
 * Update all GdkGC objects to reflect the current _draw_width.
 */
#define UPDATE_GC(gc) \
    gdk_gc_set_line_attributes(gc, \
            _draw_width, GDK_LINE_SOLID, GDK_CAP_ROUND, GDK_JOIN_ROUND);
void
update_gcs()
{
    gint i;
    printf("%s()\n", __PRETTY_FUNCTION__);

    for(i = 0; i < COLORABLE_ENUM_COUNT; i++)
    {
        gdk_color_alloc(gtk_widget_get_colormap(_map_widget), &_color[i]);
        if(_gc[i])
            g_object_unref(_gc[i]);
        _gc[i] = gdk_gc_new(_map_pixmap);
        gdk_gc_set_foreground(_gc[i], &_color[i]);
        gdk_gc_set_line_attributes(_gc[i],
                _draw_width, GDK_LINE_SOLID, GDK_CAP_ROUND, GDK_JOIN_ROUND);
    }
    
    /* Update the _map_widget's gc's. */
    gdk_gc_set_line_attributes(
            _map_widget->style->fg_gc[GTK_STATE_ACTIVE],
            2, GDK_LINE_SOLID, GDK_CAP_BUTT, GDK_JOIN_MITER);
    gdk_gc_set_line_attributes(
            _map_widget->style->bg_gc[GTK_STATE_ACTIVE],
            2, GDK_LINE_SOLID, GDK_CAP_BUTT, GDK_JOIN_MITER);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

/**
 * Call gtk_window_present() on Maemo Mapper.  This also checks the
 * configuration and brings up the Settings dialog if the GPS Receiver is
 * not set up, the first time it is called.
 */
gboolean
window_present()
{
    static gint been_here = 0;
    static gint done_here = 0;
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(!been_here++)
    {
        /* Set connection state first, to avoid going into this if twice. */
        if(_is_first_time)
        {
            GtkWidget *confirm;

            gtk_window_present(GTK_WINDOW(_window));

            confirm = hildon_note_new_confirmation(GTK_WINDOW(_window),
                    _("It looks like this is your first time running"
                        " Maemo Mapper.  Press OK to view the the help pages."
                        " Otherwise, press Cancel to continue."));

            if(GTK_RESPONSE_OK == gtk_dialog_run(GTK_DIALOG(confirm)))
            {
#ifndef LEGACY
                hildon_help_show(_osso, HELP_ID_INTRO, 0);
#else
                ossohelp_show(_osso, HELP_ID_INTRO, 0);
#endif
            }
            gtk_widget_destroy(confirm);

            /* Present the settings dialog. */
            settings_dialog();
            popup_error(_window,
                _("OpenStreetMap.org provides public, free-to-use maps.  "
                "You can also download a sample set of repositories from "
                " the internet by using the \"Download...\" button."));

            /* Present the repository dialog. */
            repoman_dialog();
            confirm = hildon_note_new_confirmation(GTK_WINDOW(_window),
                _("You will now see a blank screen.  You can download"
                    " maps using the \"Manage Maps\" menu item in the"
                    " \"Maps\" menu.  Or, press OK now to enable"
                    " Auto-Download."));
            if(GTK_RESPONSE_OK == gtk_dialog_run(GTK_DIALOG(confirm)))
            {
                gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(
                            _menu_maps_auto_download_item), TRUE);
            }
            gtk_widget_destroy(confirm);
        }

        /* Connect to receiver. */
        if(_enable_gps)
            rcvr_connect();

        ++done_here; /* Don't ask... */
    }
    if(done_here)
    {
        gtk_window_present(GTK_WINDOW(_window));
        g_timeout_add_full(G_PRIORITY_HIGH_IDLE,
                250, (GSourceFunc)banner_reset, NULL, NULL);
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return FALSE;
}

/**
 * "Set" the mark, which translates the current GPS position into on-screen
 * units in preparation for drawing the mark with map_draw_mark().
 */
static void
map_set_mark()
{
    gfloat sqrt_speed, tmp, vel_offset_devx, vel_offset_devy;
    printf("%s()\n", __PRETTY_FUNCTION__);

    tmp = deg2rad(_gps.heading);
    sqrt_speed = VELVEC_SIZE_FACTOR * sqrtf(10 + _gps.speed);
    gdk_pixbuf_rotate_vector(&vel_offset_devx, &vel_offset_devy,
            _map_rotate_matrix,
            sqrt_speed * sinf(tmp), -sqrt_speed * cosf(tmp));

    unit2buf(_pos.unitx, _pos.unity, _mark_bufx1, _mark_bufy1);

    _mark_bufx2 = _mark_bufx1 + (_show_velvec ? (vel_offset_devx + 0.5f) : 0);
    _mark_bufy2 = _mark_bufy1 + (_show_velvec ? (vel_offset_devy + 0.5f) : 0);

    _mark_minx = MIN(_mark_bufx1, _mark_bufx2) - (2 * _draw_width);
    _mark_miny = MIN(_mark_bufy1, _mark_bufy2) - (2 * _draw_width);
    _mark_width = abs(_mark_bufx1 - _mark_bufx2) + (4 * _draw_width);
    _mark_height = abs(_mark_bufy1 - _mark_bufy2) + (4 * _draw_width);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}


/**
 * Force a redraw of the entire _map_pixmap, including fetching the
 * background maps from disk and redrawing the tracks on top of them.
 */
void
map_force_redraw()
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    gdk_draw_pixbuf(
            _map_pixmap,
            _map_widget->style->fg_gc[GTK_STATE_ACTIVE],
            _map_pixbuf,
            0, 0,
            0, 0,
            _view_width_pixels, _view_height_pixels,
            GDK_RGB_DITHER_NONE, 0, 0);

    MACRO_MAP_RENDER_DATA();
    MACRO_QUEUE_DRAW_AREA();

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

Point
map_calc_new_center(gint zoom)
{
    Point new_center;
    printf("%s()\n", __PRETTY_FUNCTION__);

    switch(_center_mode)
    {
        case CENTER_LEAD:
        {
            gfloat tmp = deg2rad(_gps.heading);
            gfloat screen_pixels = _view_width_pixels
                + (((gint)_view_height_pixels
                            - (gint)_view_width_pixels)
                        * fabsf(cosf(deg2rad(
                                ROTATE_DIR_ENUM_DEGREES[_rotate_dir] -
                                (_center_rotate ? 0
                             : (_next_map_rotate_angle
                                 - (gint)(_gps.heading)))))));
            gfloat lead_pixels = 0.0025f
                * pixel2zunit((gint)screen_pixels, zoom)
                * _lead_ratio
                * VELVEC_SIZE_FACTOR
                * (_lead_is_fixed ? 7 : sqrtf(_gps.speed));

            new_center.unitx = _pos.unitx + (gint)(lead_pixels * sinf(tmp));
            new_center.unity = _pos.unity - (gint)(lead_pixels * cosf(tmp));
            break;
        }
        case CENTER_LATLON:
            new_center.unitx = _pos.unitx;
            new_center.unity = _pos.unity;
            break;
        default:
            new_center.unitx = _next_center.unitx;
            new_center.unity = _next_center.unity;
    }

    vprintf("%s(): return (%d, %d)\n", __PRETTY_FUNCTION__,
            new_center.unitx, new_center.unity);
    return new_center;
}

/**
 * Center the view on the given unitx/unity.
 */
void
map_center_unit_full(Point new_center,
        gint zoom, gint rotate_angle)
{
    MapRenderTask *mrt;
    printf("%s(%d, %d)\n", __PRETTY_FUNCTION__,
            new_center.unitx, new_center.unity);

    if(!_mouse_is_down)
    {
        /* Assure that _center.unitx/y are bounded. */
        BOUND(new_center.unitx, 0, WORLD_SIZE_UNITS);
        BOUND(new_center.unity, 0, WORLD_SIZE_UNITS);

        mrt = g_slice_new(MapRenderTask);
        mrt->repo = _curr_repo;
        mrt->old_offsetx = _map_offset_devx;
        mrt->old_offsety = _map_offset_devy;
        mrt->new_center = _next_center = new_center;
        mrt->screen_width_pixels = _view_width_pixels;
        mrt->screen_height_pixels = _view_height_pixels;
        mrt->zoom = _next_zoom = zoom;
        mrt->rotate_angle = _next_map_rotate_angle = rotate_angle;


        gtk_widget_queue_draw_area(
                _map_widget,
                _redraw_wait_bounds.x,
                _redraw_wait_bounds.y,
                _redraw_wait_bounds.width
                    + (_redraw_count * HOURGLASS_SEPARATION),
                _redraw_wait_bounds.height);

        ++_redraw_count;

        g_thread_pool_push(_mrt_thread_pool, mrt, NULL);
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

void
map_center_unit(Point new_center)
{
    map_center_unit_full(new_center, _next_zoom,
        _center_mode > 0 && _center_rotate
            ? _gps.heading : _next_map_rotate_angle);
}

void
map_rotate(gint rotate_angle)
{
    if(_center_mode > 0 && gtk_check_menu_item_get_active(
                GTK_CHECK_MENU_ITEM(_menu_view_rotate_auto_item)))
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(
                    _menu_view_rotate_auto_item), FALSE);

    map_center_unit_full(map_calc_new_center(_next_zoom), _next_zoom,
        (_next_map_rotate_angle + rotate_angle) % 360);
}

void
map_center_zoom(gint zoom)
{
    map_center_unit_full(map_calc_new_center(zoom), zoom,
        _center_mode > 0 && _center_rotate
            ? _gps.heading : _next_map_rotate_angle);
}

/**
 * Pan the view by the given number of units in the X and Y directions.
 */
void
map_pan(gint delta_unitx, gint delta_unity)
{
    Point new_center;
    printf("%s(%d, %d)\n", __PRETTY_FUNCTION__, delta_unitx, delta_unity);

    if(_center_mode > 0)
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(
                    _menu_view_ac_none_item), TRUE);
    new_center.unitx = _center.unitx + delta_unitx;
    new_center.unity = _center.unity + delta_unity;
    map_center_unit_full(new_center, _next_zoom,
            _center_mode > 0 && _center_rotate
            ? _gps.heading : _next_map_rotate_angle);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

/**
 * Initiate a move of the mark from the old location to the current
 * location.  This function queues the draw area of the old mark (to force
 * drawing of the background map), then updates the mark, then queus the
 * draw area of the new mark.
 */
void
map_move_mark()
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    /* Just queue the old and new draw areas. */
    gtk_widget_queue_draw_area(_map_widget,
            _mark_minx + _map_offset_devx,
            _mark_miny + _map_offset_devy,
            _mark_width,
            _mark_height);
    map_set_mark();
    gtk_widget_queue_draw_area(_map_widget,
            _mark_minx + _map_offset_devx,
            _mark_miny + _map_offset_devy,
            _mark_width,
            _mark_height);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

/**
 * Make sure the mark is up-to-date.  This function triggers a panning of
 * the view if the mark is appropriately close to the edge of the view.
 */
void
map_refresh_mark(gboolean force_redraw)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    gint new_center_devx, new_center_devy;

    Point new_center = map_calc_new_center(_next_zoom);

    unit2buf(new_center.unitx, new_center.unity,
            new_center_devx, new_center_devy);
    if(force_redraw || (_center_mode > 0
                && (UNITS_CONVERT[_units] * _gps.speed) >= _ac_min_speed &&
    (((unsigned)(new_center_devx - (_view_width_pixels * _center_ratio / 20))
                > ((10 - _center_ratio) * _view_width_pixels / 10))
 || ((unsigned)(new_center_devy - (_view_height_pixels * _center_ratio / 20))
                > ((10 - _center_ratio) * _view_height_pixels / 10))
            || (_center_rotate &&
              abs(_next_map_rotate_angle - _gps.heading)
                  > (4*(10-_rotate_sens))))))
    {
        map_move_mark();
        map_center_unit(new_center);
    }
    else
    {
        /* We're not changing the view - just move the mark. */
        map_move_mark();
    }

    /* Draw speed info */
    if(_speed_limit_on)
        speed_limit();

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

gboolean
map_download_refresh_idle(MapUpdateTask *mut)
{
    vprintf("%s(%p, %d, %d, %d)\n", __PRETTY_FUNCTION__, mut,
            mut->zoom, mut->tilex, mut->tiley);

    /* Test if download succeeded (only if retries != 0). */
    if(mut->pixbuf && repo_is_layer (_curr_repo, mut->repo))
    {
        gint zoff = mut->zoom - _zoom;
        /* Update the UI to reflect the updated map database. */
        /* Only refresh at same or "lower" (more detailed) zoom level. */
        if(mut->update_type == MAP_UPDATE_AUTO && (unsigned)zoff <= 4)
        {
            gfloat destx, desty;
            gint boundx, boundy, width, height;

            pixel2buf(
                    tile2pixel(mut->tilex << zoff)
                        + ((TILE_SIZE_PIXELS << zoff) >> 1),
                    tile2pixel(mut->tiley << zoff)
                        + ((TILE_SIZE_PIXELS << zoff) >> 1),
                    destx, desty);

            /* Multiply the matrix to cause blitting. */
            if(zoff)
                gdk_pixbuf_rotate_matrix_mult_number(
                        _map_rotate_matrix, 1 << zoff);
            gdk_pixbuf_rotate(_map_pixbuf,
                        /* Apply Map Correction. */
                        destx - unit2pixel(_map_correction_unitx),
                        desty - unit2pixel(_map_correction_unity),
                        _map_rotate_matrix,
                        mut->pixbuf,
                        TILE_SIZE_PIXELS / 2,
                        TILE_SIZE_PIXELS / 2,
                        TILE_SIZE_PIXELS,
                        TILE_SIZE_PIXELS,
                        &boundx, &boundy, &width, &height);
            /* Un-multiply the matrix that we used for blitting.  Good thing
             * we're multiplying by powers of two, or this wouldn't work
             * consistently... */
            if(zoff)
                gdk_pixbuf_rotate_matrix_mult_number(
                        _map_rotate_matrix, 1.f / (1 << zoff));

            if(width * height)
            {
                gdk_draw_pixbuf(
                        _map_pixmap,
                        _map_widget->style->fg_gc[GTK_STATE_ACTIVE],
                        _map_pixbuf,
                        boundx, boundy,
                        boundx, boundy,
                        width, height,
                        GDK_RGB_DITHER_NONE, 0, 0);
                MACRO_MAP_RENDER_DATA();
                gtk_widget_queue_draw_area(
                    _map_widget, boundx, boundy, width, height);
            }
        }
        g_object_unref(mut->pixbuf);
    }
    else if(mut->vfs_result != GNOME_VFS_OK)
    {
        _dl_errors++;
    }

    if(++_curr_download == _num_downloads)
    {
        if(_download_banner)
        {
            gtk_widget_destroy(_download_banner);
            _download_banner = NULL;
        }
        _num_downloads = _curr_download = 0;
        g_thread_pool_stop_unused_threads();
#ifndef MAPDB_SQLITE
        if(_curr_repo->db)
            gdbm_sync(_curr_repo->db);
#endif
        if(_dl_errors)
        {
            gchar buffer[BUFFER_SIZE];
            snprintf(buffer, sizeof(buffer), "%d %s", _dl_errors,
                    _("maps failed to download."));
            MACRO_BANNER_SHOW_INFO(_window, buffer);
            _dl_errors = 0;
        }
        else if(mut->update_type != MAP_UPDATE_AUTO)
        {
            /* Update the map. */
            map_refresh_mark(TRUE);
        }
    }
    else if(_download_banner)
    {
        hildon_banner_set_fraction(HILDON_BANNER(_download_banner),
                _curr_download / (double)_num_downloads);
    }

    g_mutex_lock(_mut_priority_mutex);
    g_hash_table_remove(_mut_exists_table, mut);
    g_mutex_unlock(_mut_priority_mutex);

    g_slice_free(MapUpdateTask, mut);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return FALSE;
}

/**
 * Set the current zoom level.  If the given zoom level is the same as the
 * current zoom level, or if the new zoom is invalid
 * (not MIN_ZOOM <= new_zoom < MAX_ZOOM), then this method does nothing.
 */
void
map_set_zoom(gint new_zoom)
{
    printf("%s(%d)\n", __PRETTY_FUNCTION__, _zoom);

    /* This if condition also checks for new_zoom >= 0. */
    if((unsigned)new_zoom > MAX_ZOOM)
        return;

    map_center_zoom(new_zoom / _curr_repo->view_zoom_steps
                     * _curr_repo->view_zoom_steps);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

static gboolean
map_replace_pixbuf_idle(MapRenderTask *mrt)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(!--_pending_replaces && !_mouse_is_down
            && mrt->screen_width_pixels == _view_width_pixels
            && mrt->screen_height_pixels == _view_height_pixels)
    {
        g_object_unref(_map_pixbuf);
        _map_pixbuf = mrt->pixbuf;

        if(_center.unitx != mrt->new_center.unitx
                || _center.unity != mrt->new_center.unity
                || _zoom != mrt->zoom
                || _map_rotate_angle != mrt->rotate_angle)
        {
            dbus_ifc_fire_view_position_changed(
                    mrt->new_center, mrt->zoom, mrt->rotate_angle);
        }

        _center = mrt->new_center;
        _zoom = mrt->zoom;
        _map_rotate_angle = mrt->rotate_angle;

        gdk_pixbuf_rotate_matrix_fill_for_rotation(
                _map_rotate_matrix,
                deg2rad(ROTATE_DIR_ENUM_DEGREES[_rotate_dir]
                    - _map_rotate_angle));
        gdk_pixbuf_rotate_matrix_fill_for_rotation(
                _map_reverse_matrix,
                deg2rad(_map_rotate_angle
                    - ROTATE_DIR_ENUM_DEGREES[_rotate_dir]));

        --_redraw_count;

        _map_offset_devx = 0;
        _map_offset_devy = 0;

        map_set_mark();
        map_force_redraw();
    }
    else
    {
        /* Ignore this new pixbuf. We have newer ones coming. */
        g_object_unref(mrt->pixbuf);
        --_redraw_count;
    }

    g_slice_free(MapRenderTask, mrt);

    vprintf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
    return FALSE;
}

gboolean
thread_render_map(MapRenderTask *mrt)
{
    gfloat matrix[4];
    gint start_tilex, start_tiley, stop_tilex, stop_tiley;
    gint x = 0, y, num_tilex, num_tiley;
    gint diag_halflength_units;
    gfloat angle_rad;
    gint tile_rothalf_pixels;
    gint curr_tile_to_draw, num_tiles_to_draw;
    gfloat *tile_dev;
    ThreadLatch *refresh_latch = NULL;
    gint cache_amount;
    static gint8 auto_download_batch_id = INT8_MIN;
    printf("%s(%d, %d, %d, %d, %d, %d)\n", __PRETTY_FUNCTION__,
            mrt->screen_width_pixels, mrt->screen_height_pixels,
            mrt->new_center.unitx, mrt->new_center.unity, mrt->zoom,
            mrt->rotate_angle);

    /* If there are more render tasks in the queue, skip this one. */
    if(g_thread_pool_unprocessed(_mrt_thread_pool))
    {
        g_slice_free(MapRenderTask, mrt);
        --_redraw_count;
        vprintf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
        return FALSE;
    }

    angle_rad = deg2rad(ROTATE_DIR_ENUM_DEGREES[_rotate_dir]
            - mrt->rotate_angle);

    gdk_pixbuf_rotate_matrix_fill_for_rotation(matrix, angle_rad);

    /* Determine (roughly) the tiles we might have to process.
     * Basically, we take the center unit and subtract the maximum dimension
     * of the screen plus the maximum additional pixels of a rotated tile.
     */
    tile_rothalf_pixels = MAX(
            fabsf(TILE_HALFDIAG_PIXELS * sinf((PI / 4) - angle_rad)),
            fabsf(TILE_HALFDIAG_PIXELS * cosf((PI / 4) - angle_rad)));

    mrt->zoom = _next_zoom;

    if(mrt->repo->type != REPOTYPE_NONE && mrt->repo->db)
        cache_amount = _auto_download_precache;
    else
        cache_amount = 1; /* No cache. */

    diag_halflength_units = pixel2zunit(TILE_HALFDIAG_PIXELS
        + MAX(mrt->screen_width_pixels, mrt->screen_height_pixels) / 2,
        mrt->zoom);
    start_tilex = unit2ztile(
            mrt->new_center.unitx - diag_halflength_units
            + _map_correction_unitx, mrt->zoom);
    start_tilex = MAX(start_tilex - (cache_amount - 1), 0);
    start_tiley = unit2ztile(
            mrt->new_center.unity - diag_halflength_units
            + _map_correction_unity, mrt->zoom);
    start_tiley = MAX(start_tiley - (cache_amount - 1), 0);
    stop_tilex = unit2ztile(mrt->new_center.unitx + diag_halflength_units
            + _map_correction_unitx, mrt->zoom);
    stop_tilex = MIN(stop_tilex + (cache_amount - 1),
            unit2ztile(WORLD_SIZE_UNITS, mrt->zoom));
    stop_tiley = unit2ztile(mrt->new_center.unity + diag_halflength_units
            + _map_correction_unity, mrt->zoom);
    stop_tiley = MIN(stop_tiley + (cache_amount - 1),
            unit2ztile(WORLD_SIZE_UNITS, mrt->zoom));

    num_tilex = (stop_tilex - start_tilex + 1);
    num_tiley = (stop_tiley - start_tiley + 1);
    tile_dev = g_new0(gfloat, num_tilex * num_tiley * 2);

    ++auto_download_batch_id;

    /* Iterate through the tiles and mark which ones need retrieval. */
    num_tiles_to_draw = 0;
    for(y = 0; y < num_tiley; ++y)
    {
        for(x = 0; x < num_tilex; ++x)
        {
            gfloat devx, devy;

            /* Find the device location of this tile's center. */
            pixel2buf_full(
                    tile2pixel(x + start_tilex) + (TILE_SIZE_PIXELS >> 1),
                    tile2pixel(y + start_tiley) + (TILE_SIZE_PIXELS >> 1),
                    devx, devy,
                    mrt->new_center, mrt->zoom, matrix);

            /* Apply Map Correction. */
            devx -= unit2zpixel(_map_correction_unitx, mrt->zoom);
            devy -= unit2zpixel(_map_correction_unity, mrt->zoom);

            /* Skip this tile under the following conditions:
             * devx < -tile_rothalf_pixels
             * devx > _view_width_pixels + tile_rothalf_pixels
             * devy < -tile_rothalf_pixels
             * devy > _view_height_pixels + tile_rothalf_pixels
             */
            if(((unsigned)(devx + tile_rothalf_pixels))
                    < (_view_width_pixels + (2 * tile_rothalf_pixels))
                && ((unsigned)(devy + tile_rothalf_pixels))
                    < (_view_height_pixels + (2 * tile_rothalf_pixels)))
            {
                tile_dev[2 * (y * num_tilex + x)] = devx;
                tile_dev[2 * (y * num_tilex + x) + 1] = devy;
                ++num_tiles_to_draw;
            }
            else
            {
                tile_dev[2 * (y * num_tilex + x)] = FLT_MAX;
            }
        }
    }

    mrt->pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8,
            mrt->screen_width_pixels, mrt->screen_height_pixels);

    /* Iterate through the tiles, get them (or queue a download if they're
     * not in the cache), and rotate them into the pixbuf. */
    for(y = curr_tile_to_draw = 0; y < num_tiley; ++y)
    {
        gint tiley = y + start_tiley;
        for(x = 0; x < num_tilex; ++x)
        {
            GdkPixbuf *tile_pixbuf = NULL;
            gboolean started_download = FALSE;
            gint zoff;
            gint tilex;

            tilex = x + start_tilex;

            zoff = mrt->repo->double_size ? 1 : 0;

            /* Iteratively try to retrieve a map to draw the tile. */
            while((mrt->zoom + zoff) <= MAX_ZOOM && zoff < TILE_SIZE_P2)
            {
                /* Check if we're actually going to draw this map. */
                if(tile_dev[2 * (y*num_tilex + x)] != FLT_MAX)
                {
                    if(NULL != (tile_pixbuf = mapdb_get(
                                    mrt->repo, mrt->zoom + zoff,
                                    tilex >> zoff,
                                    tiley >> zoff)))
                    {
                        /* Found a map. */
                        break;
                    }
                }
                /* Else we're not going to be drawing this map, so just check
                 * if it's in the database. */
                else if(mapdb_exists(
                                    mrt->repo, mrt->zoom + zoff,
                                    tilex >> zoff,
                                    tiley >> zoff))
                {
                    break;
                }

                /* No map; download, if we should. */
                if(!started_download && _auto_download
                        && mrt->repo->type != REPOTYPE_NONE
                        /* Make sure this map is within dl zoom limits. */
                        && ((unsigned)(mrt->zoom + zoff - mrt->repo->min_zoom)
                            <= (mrt->repo->max_zoom - mrt->repo->min_zoom))
                        /* Make sure this map matches the dl_zoom_steps,
                         * or that there currently is no cache. */
                        && (!mrt->repo->db || !((mrt->zoom + zoff
                                    - (mrt->repo->double_size ? 1 : 0))
                                % mrt->repo->dl_zoom_steps))
                    /* Make sure this tile is even possible. */
                    && ((unsigned)(tilex >> zoff)
                            < unit2ztile(WORLD_SIZE_UNITS, mrt->zoom + zoff)
                      && (unsigned)(tiley >> zoff)
                            < unit2ztile(WORLD_SIZE_UNITS, mrt->zoom + zoff)))
                {
                    started_download = TRUE;

                    if(!refresh_latch)
                    {
                        refresh_latch = g_slice_new(ThreadLatch);
                        refresh_latch->is_open = FALSE;
                        refresh_latch->is_done_adding_tasks = FALSE;
                        refresh_latch->num_tasks = 1;
                        refresh_latch->num_done = 0;
                        refresh_latch->mutex = g_mutex_new();
                        refresh_latch->cond = g_cond_new();
                    }
                    else
                        ++refresh_latch->num_tasks;

                    mapdb_initiate_update(
                            mrt->repo,
                            mrt->zoom + zoff,
                            tilex >> zoff,
                            tiley >> zoff,
                            MAP_UPDATE_AUTO,
                            auto_download_batch_id,
                            (abs((tilex >> zoff) - unit2ztile(
                                     mrt->new_center.unitx, mrt->zoom + zoff))
                             + abs((tiley >> zoff) - unit2ztile(
                                    mrt->new_center.unity, mrt->zoom + zoff))),
                            refresh_latch);
                }

                /* Try again at a coarser resolution. */
                ++zoff;
            }

            if(tile_pixbuf)
            {
                gint boundx, boundy, width, height;
                RepoData* repo_p = mrt->repo->layers;
                GdkPixbuf* layer_pixbuf;

                /* before we rotate the resulting tile, we look for other layer's pixbufs */
                while (repo_p) {
                    if(NULL != (layer_pixbuf = mapdb_get(
                                    repo_p, mrt->zoom + zoff,
                                    tilex >> zoff,
                                    tiley >> zoff)))
                    {
                        /* join pixbufs together */
                        gdk_pixbuf_composite (layer_pixbuf, tile_pixbuf, 0, 0, 
                                              gdk_pixbuf_get_width (tile_pixbuf), 
                                              gdk_pixbuf_get_height (tile_pixbuf), 
                                              0, 0, 1, 1, GDK_INTERP_NEAREST, 255);
                        g_object_unref (layer_pixbuf);
                    }

                    repo_p = repo_p->layers;
                }

                if(zoff)
                    gdk_pixbuf_rotate_matrix_mult_number(matrix, 1 << zoff);
                gdk_pixbuf_rotate(mrt->pixbuf,
                        tile_dev[2 * (y * num_tilex + x)],
                        tile_dev[2 * (y * num_tilex + x) + 1],
                        matrix,
                        tile_pixbuf,
                        ((tilex - ((tilex >> zoff) << zoff))
                            << (TILE_SIZE_P2 - zoff))
                            + (TILE_SIZE_PIXELS >> (1 + zoff)),
                        ((tiley - ((tiley>>zoff) << zoff))
                            << (TILE_SIZE_P2 - zoff))
                            + (TILE_SIZE_PIXELS >> (1 + zoff)),
                        TILE_SIZE_PIXELS >> zoff,
                        TILE_SIZE_PIXELS >> zoff,
                        &boundx, &boundy, &width, &height);
                g_object_unref(tile_pixbuf);
                /* Un-multiply the matrix that we used for blitting.  Good
                 * thing we're multiplying by powers of two, or this wouldn't
                 * work consistently... */
                if(zoff)
                    gdk_pixbuf_rotate_matrix_mult_number(
                            matrix, 1.f / (1 << zoff));
            }
            /* usleep(10000); DEBUG */
        }
    }

    /* Don't replace the pixbuf unless/until the mouse is released. */
    g_mutex_lock(_mouse_mutex);
    ++_pending_replaces;
    g_idle_add_full(G_PRIORITY_HIGH_IDLE + 20,
            (GSourceFunc)map_replace_pixbuf_idle, mrt, NULL);
    g_mutex_unlock(_mouse_mutex);

    /* Release the view-change lock. */
    if(refresh_latch)
    {
        g_mutex_lock(refresh_latch->mutex);
        if(refresh_latch->num_tasks == refresh_latch->num_done)
        {
            /* Fast little workers, aren't they? */
            g_mutex_unlock(refresh_latch->mutex);
            g_cond_free(refresh_latch->cond);
            g_mutex_free(refresh_latch->mutex);
            g_slice_free(ThreadLatch, refresh_latch);
        }
        else
        {
            refresh_latch->is_done_adding_tasks = TRUE;
            refresh_latch->is_open = TRUE;
            g_cond_signal(refresh_latch->cond);
            g_mutex_unlock(refresh_latch->mutex);
        }
    }

    g_free(tile_dev);

    vprintf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
    return FALSE;
}

gboolean
map_cb_configure(GtkWidget *widget, GdkEventConfigure *event)
{
    gint old_view_width_pixels, old_view_height_pixels;
    GdkPixbuf *old_map_pixbuf;
    printf("%s(%d, %d)\n", __PRETTY_FUNCTION__,
            _map_widget->allocation.width, _map_widget->allocation.height);

    if(_map_widget->allocation.width == 1
            && _map_widget->allocation.height == 1)
        /* Special case - first allocation - not persistent. */
        return TRUE;

    old_view_width_pixels = _view_width_pixels;
    old_view_height_pixels = _view_height_pixels;
    _view_width_pixels = _map_widget->allocation.width;
    _view_height_pixels = _map_widget->allocation.height;
    _view_halfwidth_pixels = _view_width_pixels / 2;
    _view_halfheight_pixels = _view_height_pixels / 2;

    g_object_unref(_map_pixmap);
    _map_pixmap = gdk_pixmap_new(
                _map_widget->window,
                _view_width_pixels, _view_height_pixels,
                -1); /* -1: use bit depth of widget->window. */

    old_map_pixbuf = _map_pixbuf;
    _map_pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8,
            _view_width_pixels, _view_height_pixels);

    {
        gint oldnew_diffx = (gint)(_view_width_pixels
                - old_view_width_pixels) / 2;
        gint oldnew_diffy = (gint)(_view_height_pixels
                - old_view_height_pixels) / 2;
        gdk_pixbuf_copy_area(old_map_pixbuf,
                MAX(0, -oldnew_diffx), MAX(0, -oldnew_diffy),
                MIN(_view_width_pixels, old_view_width_pixels),
                MIN(_view_height_pixels, old_view_height_pixels),
                _map_pixbuf,
                MAX(0, oldnew_diffx), MAX(0, oldnew_diffy));
    }

    g_object_unref(old_map_pixbuf);

    /* Set _scale_rect. */
    _scale_rect.x = (_view_width_pixels - SCALE_WIDTH) / 2;
    _scale_rect.width = SCALE_WIDTH;
    pango_layout_set_text(_scale_layout, "0", -1);
    pango_layout_get_pixel_size(_scale_layout, NULL, &_scale_rect.height);
    _scale_rect.y = _view_height_pixels - _scale_rect.height - 1;

    /* Set _zoom rect. */
    pango_layout_set_text(_zoom_layout, "00", -1);
    pango_layout_get_pixel_size(_zoom_layout, &_zoom_rect.width,
            &_zoom_rect.height);
    _zoom_rect.width *= 1.25;
    pango_layout_set_width(_zoom_layout, _zoom_rect.width);
    pango_layout_context_changed(_zoom_layout);
    _zoom_rect.x = _scale_rect.x - _zoom_rect.width;
    _zoom_rect.y = _view_height_pixels - _zoom_rect.height - 1;

    /* Set _comprose_rect. */
    _comprose_rect.x = _view_width_pixels - 25 - _comprose_rect.width;
    _comprose_rect.y = _view_height_pixels - 25 - _comprose_rect.height;

    map_set_mark();
    map_force_redraw();

    /* Fire the screen_dimensions_changed DBUS signal. */
    dbus_ifc_fire_view_dimensions_changed(
            _view_width_pixels, _view_height_pixels);

    /* If Auto-Center is set to Lead, then recalc center. */
    if(_center_mode == CENTER_LEAD)
        map_center_unit(map_calc_new_center(_next_zoom));
    else
        map_center_unit(_next_center);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

gboolean
sat_panel_expose(GtkWidget *widget, GdkEventExpose *event)
{
    gchar *tmp = NULL;
    gint x, y;
    printf("%s()\n", __PRETTY_FUNCTION__);

    draw_sat_info(widget,
        0, 0,
        widget->allocation.width,
        widget->allocation.height,
        FALSE);

    /* Sat View/In Use */
    tmp = g_strdup_printf("%d/%d", _gps.satinuse, _gps.satinview);
    pango_layout_set_text(_sat_panel_layout, tmp, -1);
    pango_layout_set_alignment(_sat_panel_layout, PANGO_ALIGN_LEFT);
    gdk_draw_layout(widget->window,
        widget->style->fg_gc[GTK_WIDGET_STATE(widget)],
        20, 2,
        _sat_panel_layout);
    g_free(tmp);

    switch(_gps.fix)
    {
        case 2:
        case 3: tmp = g_strdup_printf("%dD fix", _gps.fix); break;
        default: tmp = g_strdup_printf("nofix"); break;
    }
    pango_layout_set_text(_sat_panel_layout, tmp, -1);
    pango_layout_set_alignment(_sat_panel_layout, PANGO_ALIGN_RIGHT);
    pango_layout_get_pixel_size(_sat_panel_layout, &x, &y);
    gdk_draw_layout(widget->window,
        widget->style->fg_gc[GTK_WIDGET_STATE(widget)],
        widget->allocation.width - 20 - x, 2,
        _sat_panel_layout);
    g_free(tmp);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return TRUE;
}

gboolean
heading_panel_expose(GtkWidget *widget, GdkEventExpose *event)
{
    gint size, xoffset, yoffset, i, x, y;
    gint dir;
    gfloat tmp;
    gchar *text;
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
    pango_font_description_set_size(_heading_panel_fontdesc,12*PANGO_SCALE);
    pango_layout_set_font_description(_heading_panel_layout,
            _heading_panel_fontdesc);
    pango_layout_set_alignment(_heading_panel_layout, PANGO_ALIGN_CENTER);

    text = g_strdup_printf("%3.0f°", _gps.heading);
    pango_layout_set_text(_heading_panel_layout, text, -1);
    pango_layout_get_pixel_size(_heading_panel_layout, &x, &y);

    gdk_draw_layout(widget->window,
        widget->style->fg_gc[GTK_WIDGET_STATE(widget)],
        xoffset + size/2 - x/2,
        yoffset + size - y - 2, _heading_panel_layout);
    g_free(text);

    gdk_draw_arc (widget->window,
        widget->style->fg_gc[GTK_WIDGET_STATE(widget)],
        FALSE,
        xoffset, yoffset + size/2,
        size, size,
        0, 64 * 180);

    /* Simple arrow for heading*/
    gdk_draw_line(widget->window,
        widget->style->fg_gc[GTK_WIDGET_STATE(widget)],
        xoffset + size/2 + 3,
        yoffset + size - y - 5,
        xoffset + size/2,
        yoffset + size/2 + 5);

    gdk_draw_line(widget->window,
        widget->style->fg_gc[GTK_WIDGET_STATE(widget)],
        xoffset + size/2 - 3,
        yoffset + size - y - 5,
        xoffset + size/2,
        yoffset + size/2 + 5);

    gdk_draw_line(widget->window,
        widget->style->fg_gc[GTK_WIDGET_STATE(widget)],
        xoffset + size/2 - 3,
        yoffset + size - y - 5,
        xoffset + size/2,
        yoffset + size - y - 8);

    gdk_draw_line(widget->window,
        widget->style->fg_gc[GTK_WIDGET_STATE(widget)],
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

        tmp = deg2rad(dir - _gps.heading);
        gdk_draw_line(widget->window,
            widget->style->fg_gc[GTK_WIDGET_STATE(widget)],
            xoffset + size/2 + ((size/2 - 5) * sinf(tmp)),
            yoffset + size - ((size/2 - 5) * cosf(tmp)),
            xoffset + size/2 + ((size/2 + 5) * sinf(tmp)),
            yoffset + size - ((size/2 + 5) * cosf(tmp)));

        x = fsize[i];
        if(abs((gint)(_gps.heading/45)*45 - _gps.heading)
                > abs((gint)(_gps.heading/45)*45 + 45 - _gps.heading)
                && (i > 0))
            x = fsize[i - 1];

        pango_font_description_set_size(_heading_panel_fontdesc,
                (10 + x)*PANGO_SCALE);
        pango_layout_set_font_description(_heading_panel_layout,
                _heading_panel_fontdesc);
        pango_layout_set_text(_heading_panel_layout, text, -1);
        pango_layout_get_pixel_size(_heading_panel_layout, &x, &y);
        x = xoffset + size/2 + ((size/2 + 15) * sinf(tmp)) - x/2,
        y = yoffset + size - ((size/2 + 15) * cosf(tmp)) - y/2,

        gdk_draw_layout(widget->window,
            widget->style->fg_gc[GTK_WIDGET_STATE(widget)],
            x, y, _heading_panel_layout);
        g_free(text);
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return TRUE;
}

gboolean
map_cb_expose(GtkWidget *widget, GdkEventExpose *event)
{
    gint i;
    printf("%s(%d, %d, %d, %d)\n", __PRETTY_FUNCTION__,
            event->area.x, event->area.y,
            event->area.width, event->area.height);

    gdk_draw_drawable(
            _map_widget->window,
            _gc[COLORABLE_MARK],
            _map_pixmap,
            event->area.x - _map_offset_devx, event->area.y - _map_offset_devy,
            event->area.x, event->area.y,
            event->area.width, event->area.height);

    /* Draw the mark. */
    if((((unsigned)(_mark_bufx1 + _draw_width)
                <= _view_width_pixels+2*_draw_width)
             &&((unsigned)(_mark_bufy1 + _draw_width)
                 <= _view_height_pixels+2*_draw_width))
        || (((unsigned)(_mark_bufx2 + _draw_width)
                 <= _view_width_pixels+2*_draw_width)
             &&((unsigned)(_mark_bufy2 + _draw_width)
                 <= _view_height_pixels+2*_draw_width)))
    {
        gdk_draw_arc(
                _map_widget->window,
                _gps_state == RCVR_FIXED
                    ? _gc[COLORABLE_MARK] : _gc[COLORABLE_MARK_OLD],
                FALSE, /* not filled. */
                _mark_bufx1 - _draw_width + _map_offset_devx,
                _mark_bufy1 - _draw_width + _map_offset_devy,
                2 * _draw_width, 2 * _draw_width,
                0, 360 * 64);
        gdk_draw_line(
                _map_widget->window,
                _gps_state == RCVR_FIXED
                    ? (_show_velvec
                        ? _gc[COLORABLE_MARK_VELOCITY] : _gc[COLORABLE_MARK])
                    : _gc[COLORABLE_MARK_OLD],
                _mark_bufx1 + _map_offset_devx,
                _mark_bufy1 + _map_offset_devy,
                _mark_bufx2 + _map_offset_devx,
                _mark_bufy2 + _map_offset_devy);
    }

    /* draw zoom box if so wanted */
    if(_show_zoomlevel) {
        gchar *buffer = g_strdup_printf("%d", _zoom);
        gdk_draw_rectangle(_map_widget->window,
                _map_widget->style->bg_gc[GTK_STATE_ACTIVE],
                TRUE,
                _zoom_rect.x, _zoom_rect.y,
                _zoom_rect.width, _zoom_rect.height);
        gdk_draw_rectangle(_map_widget->window,
                _map_widget->style->fg_gc[GTK_STATE_ACTIVE],
                FALSE,
                _zoom_rect.x, _zoom_rect.y,
                _zoom_rect.width, _zoom_rect.height);
        pango_layout_set_text(_zoom_layout, buffer, -1);
        gdk_draw_layout(_map_widget->window,
                _map_widget->style->fg_gc[GTK_STATE_ACTIVE],
                _zoom_rect.x + _zoom_rect.width / 2,
                _zoom_rect.y, _zoom_layout);
    }

    /* Draw scale, if necessary. */
    if(_show_scale)
    {
        gdk_rectangle_intersect(&event->area, &_scale_rect, &event->area);
        if(event->area.width && event->area.height)
        {
            gdk_draw_rectangle(_map_widget->window,
                    _map_widget->style->bg_gc[GTK_STATE_ACTIVE],
                    TRUE,
                    _scale_rect.x, _scale_rect.y,
                    _scale_rect.width, _scale_rect.height);
            gdk_draw_rectangle(_map_widget->window,
                    _map_widget->style->fg_gc[GTK_STATE_ACTIVE],
                    FALSE,
                    _scale_rect.x, _scale_rect.y,
                    _scale_rect.width, _scale_rect.height);

            /* Now calculate and draw the distance. */
            {
                gchar buffer[16];
                gfloat distance;
                gdouble lat1, lon1, lat2, lon2;
                gint width;

                unit2latlon(_center.unitx - pixel2unit(SCALE_WIDTH / 2 - 4),
                        _center.unity, lat1, lon1);
                unit2latlon(_center.unitx + pixel2unit(SCALE_WIDTH / 2 - 4),
                        _center.unity, lat2, lon2);
                distance = calculate_distance(lat1, lon1, lat2, lon2)
                    * UNITS_CONVERT[_units];

                if(distance < 1.f)
                    snprintf(buffer, sizeof(buffer), "%0.2f %s", distance,
                            UNITS_ENUM_TEXT[_units]);
                else if(distance < 10.f)
                    snprintf(buffer, sizeof(buffer), "%0.1f %s", distance,
                            UNITS_ENUM_TEXT[_units]);
                else
                    snprintf(buffer, sizeof(buffer), "%0.f %s", distance,
                            UNITS_ENUM_TEXT[_units]);
                pango_layout_set_text(_scale_layout, buffer, -1);

                pango_layout_get_pixel_size(_scale_layout, &width, NULL);

                /* Draw the layout itself. */
                gdk_draw_layout(_map_widget->window,
                    _map_widget->style->fg_gc[GTK_STATE_ACTIVE],
                    _scale_rect.x + (_scale_rect.width - width) / 2,
                    _scale_rect.y, _scale_layout);

                /* Draw little hashes on the ends. */
                gdk_draw_line(_map_widget->window,
                    _map_widget->style->fg_gc[GTK_STATE_ACTIVE],
                    _scale_rect.x + 4,
                    _scale_rect.y + _scale_rect.height / 2 - 4,
                    _scale_rect.x + 4,
                    _scale_rect.y + _scale_rect.height / 2 + 4);
                gdk_draw_line(_map_widget->window,
                    _map_widget->style->fg_gc[GTK_STATE_ACTIVE],
                    _scale_rect.x + 4,
                    _scale_rect.y + _scale_rect.height / 2,
                    _scale_rect.x + (_scale_rect.width - width) / 2 - 4,
                    _scale_rect.y + _scale_rect.height / 2);
                gdk_draw_line(_map_widget->window,
                    _map_widget->style->fg_gc[GTK_STATE_ACTIVE],
                    _scale_rect.x + _scale_rect.width - 4,
                    _scale_rect.y + _scale_rect.height / 2 - 4,
                    _scale_rect.x + _scale_rect.width - 4,
                    _scale_rect.y + _scale_rect.height / 2 + 4);
                gdk_draw_line(_map_widget->window,
                    _map_widget->style->fg_gc[GTK_STATE_ACTIVE],
                    _scale_rect.x + _scale_rect.width - 4,
                    _scale_rect.y + _scale_rect.height / 2,
                    _scale_rect.x + (_scale_rect.width + width) / 2 + 4,
                    _scale_rect.y + _scale_rect.height / 2);
            }
        }
    }

    /* Draw the compass rose, if necessary. */
    if(_show_comprose)
    {
        GdkPoint points[3];
        gint offsetx, offsety;
        gfloat x, y;

        offsetx = _comprose_rect.x;
        offsety = _comprose_rect.y;

        gdk_pixbuf_rotate_vector(&x, &y, _map_rotate_matrix, 0, 12);
        points[0].x = offsetx + x + 0.5f; points[0].y = offsety + y + 0.5f;
        gdk_pixbuf_rotate_vector(&x, &y, _map_rotate_matrix, 20, 30);
        points[1].x = offsetx + x + 0.5f; points[1].y = offsety + y + 0.5f;
        gdk_pixbuf_rotate_vector(&x, &y, _map_rotate_matrix, 0, -45);
        points[2].x = offsetx + x + 0.5f; points[2].y = offsety + y + 0.5f;
        gdk_pixbuf_rotate_vector(&x, &y, _map_rotate_matrix, -20, 30);
        points[3].x = offsetx + x + 0.5f; points[3].y = offsety + y + 0.5f;

        gdk_draw_polygon(_map_widget->window,
                _map_widget->style->bg_gc[GTK_STATE_ACTIVE],
                TRUE, /* FILLED */
                points, 4);
        gdk_draw_polygon(_map_widget->window,
                _map_widget->style->fg_gc[GTK_STATE_ACTIVE],
                FALSE, /* NOT FILLED */
                points, 4);

        gdk_draw_layout(_map_widget->window,
                _map_widget->style->fg_gc[GTK_STATE_ACTIVE],
                _comprose_rect.x - _comprose_rect.width / 2 + 1,
                _comprose_rect.y - _comprose_rect.height / 2 - 4,
                _comprose_layout);
    }

    /* Draw a stopwatch if we're redrawing the map. */
    for(i = _redraw_count - 1; i >= 0; i--)
    {
        gdk_draw_pixbuf(
                _map_widget->window,
                _map_widget->style->fg_gc[GTK_STATE_ACTIVE],
                _redraw_wait_icon,
                0, 0,
                _redraw_wait_bounds.x + i
                        * HOURGLASS_SEPARATION,
                _redraw_wait_bounds.y,
                _redraw_wait_bounds.width,
                _redraw_wait_bounds.height,
                GDK_RGB_DITHER_NONE, 0, 0);
    }

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static void
latlon_cb_copy_clicked(GtkWidget *widget, LatlonDialog *lld) {
    gchar buffer[42];

    snprintf(buffer, sizeof(buffer),
            "%s %s",
            gtk_label_get_text(GTK_LABEL(lld->lat)),
            gtk_label_get_text(GTK_LABEL(lld->lon)));

    gtk_clipboard_set_text(
            gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), buffer, -1);
}

static void
latlon_cb_fmt_changed(GtkWidget *widget, LatlonDialog *lld) {
  DegFormat fmt;

  fmt = gtk_combo_box_get_active(GTK_COMBO_BOX(lld->fmt_combo));

  {
    gint old = _degformat; /* augh... */
    gchar buffer[LL_FMT_LEN];

    _degformat = fmt;
    lat_format(lld->glat, buffer);
    gtk_label_set_label(GTK_LABEL(lld->lat), buffer);
    lon_format(lld->glon, buffer);
    gtk_label_set_label(GTK_LABEL(lld->lon), buffer);
    _degformat = old;
  }
}

gboolean
latlon_dialog(gdouble lat, gdouble lon)
{
    LatlonDialog lld;
    GtkWidget *dialog;
    GtkWidget *table;
    GtkWidget *label;
    GtkWidget *txt_lat;
    GtkWidget *txt_lon;
    GtkWidget *cmb_format;
    GtkWidget *btn_copy = NULL;
    printf("%s()\n", __PRETTY_FUNCTION__);

    dialog = gtk_dialog_new_with_buttons(_("Show Position"),
            GTK_WINDOW(_window), GTK_DIALOG_MODAL,
            GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
            NULL);

    /* Set the lat/lon strings. */
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
            table = gtk_table_new(5, 2, FALSE), TRUE, TRUE, 0);

    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new(_("Lat")),
            0, 1, 0, 1, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(table),
            txt_lat = gtk_label_new(""),
            1, 2, 0, 1, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(txt_lat), 1.f, 0.5f);

    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new(_("Lon")),
            0, 1, 1, 2, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(table),
            txt_lon = gtk_label_new(""),
            1, 2, 1, 2, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(txt_lon), 1.f, 0.5f);

    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new(_("Format")),
            0, 1, 2, 3, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(table),
            label = gtk_alignment_new(0.f, 0.5f, 0.f, 0.f),
            1, 2, 2, 3, GTK_FILL, 0, 2, 4);
    gtk_container_add(GTK_CONTAINER(label),
            cmb_format = gtk_combo_box_new_text());
    gtk_table_attach(GTK_TABLE(table),
            btn_copy = gtk_button_new_with_label(_("Copy")),
            0, 2, 3, 4, GTK_FILL, 0, 2, 4);

    /* Lat/Lon */
    {
      gchar buffer[LL_FMT_LEN];

      lat_format(lat, buffer);
      gtk_label_set_label(GTK_LABEL(txt_lat), buffer);
      lat_format(lon, buffer);
      gtk_label_set_label(GTK_LABEL(txt_lon), buffer);
    }

    /* Fill in formats */
    {
      int i;

      for(i = 0; i < DEG_FORMAT_ENUM_COUNT; i++) {
          gtk_combo_box_append_text(GTK_COMBO_BOX(cmb_format),
                  DEG_FORMAT_ENUM_TEXT[i]);
      }
      gtk_combo_box_set_active(GTK_COMBO_BOX(cmb_format), _degformat);
    }


    /* setup cb context */
    lld.fmt_combo = cmb_format;
    lld.glat = lat;
    lld.glon = lon;
    lld.lat = txt_lat;
    lld.lon = txt_lon;

    /* Connect Signals */
    g_signal_connect(G_OBJECT(cmb_format), "changed",
                    G_CALLBACK(latlon_cb_fmt_changed), &lld);
    g_signal_connect(G_OBJECT(btn_copy), "clicked",
                    G_CALLBACK(latlon_cb_copy_clicked), &lld);

    gtk_widget_show_all(dialog);

    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_hide(dialog);

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
gboolean
display_open_file(GtkWindow *parent, gchar **bytes_out,
        GnomeVFSHandle **handle_out, gint *size_out,
        gchar **dir, gchar **file, GtkFileChooserAction chooser_action)
{
    GtkWidget *dialog;
    gboolean success = FALSE;
    printf("%s()\n", __PRETTY_FUNCTION__);

    dialog = hildon_file_chooser_dialog_new(parent, chooser_action);

    if(dir && *dir)
        gtk_file_chooser_set_current_folder_uri(
                GTK_FILE_CHOOSER(dialog), *dir);
    if(file && *file)
    {
        gtk_file_chooser_set_uri(
                GTK_FILE_CHOOSER(dialog), *file);
        if(chooser_action == GTK_FILE_CHOOSER_ACTION_SAVE)
        {
            /* Work around a bug in HildonFileChooserDialog. */
            gchar *basename = g_path_get_basename(*file);
            g_object_set(G_OBJECT(dialog), "autonaming", FALSE, NULL);
            gtk_file_chooser_set_current_name(
                    GTK_FILE_CHOOSER(dialog), basename);
            g_free(basename);
        }
    }

    gtk_widget_show_all(dialog);

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
                    "%s:\n%s", chooser_action == GTK_FILE_CHOOSER_ACTION_OPEN
                                ? _("Failed to open file for reading")
                                : _("Failed to open file for writing"),
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

void
display_init()
{
    PangoContext *pango_context;
    PangoFontDescription *pango_font;
    GdkColor color;
    printf("%s()\n", __PRETTY_FUNCTION__);

    /* Cache some pango and GCs for drawing. */
    pango_context = gtk_widget_get_pango_context(_map_widget);
    _scale_layout = pango_layout_new(pango_context);
    pango_font = pango_font_description_new();
    pango_font_description_set_size(pango_font, 12 * PANGO_SCALE);
    pango_layout_set_font_description(_scale_layout, pango_font);

    /* zoom box */
    pango_context = gtk_widget_get_pango_context(_map_widget);
    _zoom_layout = pango_layout_new(pango_context);
    pango_font = pango_font_description_new();
    pango_font_description_set_size(pango_font, 12 * PANGO_SCALE);
    pango_layout_set_font_description(_zoom_layout, pango_font);
    pango_layout_set_alignment(_zoom_layout, PANGO_ALIGN_CENTER);

    /* compose rose */
    pango_context = gtk_widget_get_pango_context(_map_widget);
    _comprose_layout = pango_layout_new(pango_context);
    pango_font = pango_font_description_new();
    pango_font_description_set_size(pango_font, 16 * PANGO_SCALE);
    pango_font_description_set_weight(pango_font, PANGO_WEIGHT_BOLD);
    pango_layout_set_font_description(_comprose_layout, pango_font);
    pango_layout_set_alignment(_comprose_layout, PANGO_ALIGN_CENTER);
    pango_layout_set_text(_comprose_layout, "N", -1);
    {
        PangoRectangle rect;
        pango_layout_get_pixel_extents(_comprose_layout, &rect, NULL);
        _comprose_rect.width = rect.width + 3;
        _comprose_rect.height = rect.height + 3;
    }

    /* speed limit */
    _speed_limit_gc1 = gdk_gc_new (_map_widget->window);
    color.red = 0xffff;
    color.green = 0;
    color.blue = 0;
    gdk_gc_set_rgb_fg_color(_speed_limit_gc1, &color);
    color.red = 0;
    color.green = 0;
    color.blue = 0;
    _speed_limit_gc2 = gdk_gc_new(_map_widget->window);
    gdk_gc_set_rgb_fg_color(_speed_limit_gc2, &color);
    pango_context = gtk_widget_get_pango_context(_map_widget);
    _speed_limit_layout = pango_layout_new(pango_context);
    pango_font = pango_font_description_new();
    pango_font_description_set_size(pango_font, 64 * PANGO_SCALE);
    pango_layout_set_font_description(_speed_limit_layout,
            pango_font);
    pango_layout_set_alignment(_speed_limit_layout, PANGO_ALIGN_CENTER);

    /* draw_sat_info() */
    _sat_info_gc1 = gdk_gc_new(_map_widget->window);
    color.red = 0;
    color.green = 0;
    color.blue = 0;
    gdk_gc_set_rgb_fg_color(_sat_info_gc1, &color);
    color.red = 0;
    color.green = 0;
    color.blue = 0xffff;
    _sat_info_gc2 = gdk_gc_new(_map_widget->window);
    gdk_gc_set_rgb_fg_color(_sat_info_gc2, &color);
    pango_context = gtk_widget_get_pango_context(_map_widget);
    _sat_info_layout = pango_layout_new(pango_context);
    pango_font = pango_font_description_new();
    pango_font_description_set_family(pango_font,"Sans Serif");
    pango_font_description_set_size(pango_font, 8*PANGO_SCALE);
    pango_layout_set_font_description(_sat_info_layout, pango_font);
    pango_layout_set_alignment(_sat_info_layout, PANGO_ALIGN_CENTER);

    /* sat_panel_expose() */
    pango_context = gtk_widget_get_pango_context(_map_widget);
    _sat_panel_layout = pango_layout_new(pango_context);
    pango_font = pango_font_description_new();
    pango_font_description_set_family(pango_font,"Sans Serif");
    pango_font_description_set_size(pango_font, 14*PANGO_SCALE);
    pango_layout_set_font_description (_sat_panel_layout, pango_font);

    /* heading_panel_expose() */
    pango_context = gtk_widget_get_pango_context(_map_widget);
    _heading_panel_layout = pango_layout_new(pango_context);
    _heading_panel_fontdesc =  pango_font_description_new();
    pango_font_description_set_family(_heading_panel_fontdesc, "Sans Serif");

    /* draw_sat_details() */
    pango_context = gtk_widget_get_pango_context(_map_widget);
    _sat_details_layout = pango_layout_new(pango_context);
    pango_font = pango_font_description_new();
    pango_font_description_set_family(pango_font,"Sans Serif");
    pango_font_description_set_size(pango_font, 10*PANGO_SCALE);
    pango_layout_set_font_description(_sat_details_layout,
            pango_font);
    pango_layout_set_alignment(_sat_details_layout, PANGO_ALIGN_CENTER);

    /* sat_details_panel_expose() */
    pango_context = gtk_widget_get_pango_context(_map_widget);
    _sat_details_expose_layout = pango_layout_new(pango_context);
    pango_font = pango_font_description_new();
    pango_font_description_set_family(
            pango_font,"Sans Serif");
    pango_layout_set_alignment(_sat_details_expose_layout,
            PANGO_ALIGN_CENTER);
    pango_font_description_set_size(pango_font,
            14*PANGO_SCALE);
    pango_layout_set_font_description(_sat_details_expose_layout,
            pango_font);

    /* Load the _redraw_wait_icon. */
    {
        GError *error = NULL;
        gchar *icon_path = "/usr/share/icons/hicolor/scalable/hildon"
                           "/maemo-mapper-wait.png";
        _redraw_wait_bounds.x = 0;
        _redraw_wait_bounds.y = 0;
        _redraw_wait_icon = gdk_pixbuf_new_from_file(icon_path, &error);
        if(!_redraw_wait_icon || error)
        {
            g_printerr("Error parsing pixbuf: %s\n",
                    error ? error->message : icon_path);
            _redraw_wait_bounds.width = 0;
            _redraw_wait_bounds.height = 0;
            _redraw_wait_icon = NULL;
        }
        else
        {
            _redraw_wait_bounds.width
                = gdk_pixbuf_get_width(_redraw_wait_icon);
            _redraw_wait_bounds.height
                = gdk_pixbuf_get_height(_redraw_wait_icon);
        }
    }

    g_signal_connect(G_OBJECT(_map_widget), "configure_event",
            G_CALLBACK(map_cb_configure), NULL);

    g_signal_connect(G_OBJECT(_map_widget), "expose_event",
            G_CALLBACK(map_cb_expose), NULL);
    g_signal_connect(G_OBJECT(_sat_panel), "expose_event",
            G_CALLBACK(sat_panel_expose), NULL);
    g_signal_connect(G_OBJECT(_heading_panel), "expose_event",
            G_CALLBACK(heading_panel_expose), NULL);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}
