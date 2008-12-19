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

#ifndef MAEMO_MAPPER_DISPLAY_H
#define MAEMO_MAPPER_DISPLAY_H

typedef struct {
    gdouble glat, glon;
    GtkWidget *fmt_combo;
    GtkWidget *lat;
    GtkWidget *lon;
    GtkWidget *lat_title;
    GtkWidget *lon_title;
} LatlonDialog;


gboolean gps_display_details(void);
void gps_display_data(void);
void gps_hide_text(void);
void gps_show_info(void);
void gps_details(void);

void map_render_segment(GdkGC *gc_norm, GdkGC *gc_alt,
        gint unitx1, gint unity1, gint unitx2, gint unity2);
void map_render_paths(void);

void update_gcs(void);

gboolean window_present(void);

void map_pan(gint delta_unitx, gint delta_unity);
void map_move_mark(void);
void map_refresh_mark(gboolean force_redraw);
void map_force_redraw(void);


void map_center_unit_full(Point new_center, gint zoom, gint rotate_angle);
void map_center_unit(Point new_center);
void map_rotate(gint rotate_angle);
void map_center_zoom(gint zoom);
Point map_calc_new_center(gint zoom);

gboolean map_download_refresh_idle(MapUpdateTask *mut);
void map_set_zoom(gint new_zoom);

gboolean thread_render_map(MapRenderTask *mrt);

gboolean map_cb_configure(GtkWidget *widget, GdkEventConfigure *event);
gboolean map_cb_expose(GtkWidget *widget, GdkEventExpose *event);

gboolean latlon_dialog(gdouble lat, gdouble lon);

gboolean display_open_file(GtkWindow *parent, gchar **bytes_out,
        GnomeVFSHandle **handle_out, gint *size_out, gchar **dir, gchar **file,
        GtkFileChooserAction chooser_action);

void display_init(void);

#ifdef INCLUDE_APRS
void plot_aprs_station(AprsDataRow *p_station, gboolean single );
#endif // INCLUDE_APRS

#endif /* ifndef MAEMO_MAPPER_DISPLAY_H */
