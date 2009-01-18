/*
 * Created by Rob Williams - 10 Aug 2008
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
 */

#ifdef HAVE_CONFIG_H
#    include "config.h"
#endif

#ifdef INCLUDE_APRS

#include "aprs_display.h"
#include "aprs_message.h"
#include "types.h"
#include "aprs.h"

#define _GNU_SOURCE

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <dbus/dbus-glib.h>
#include <bt-dbus.h>
#include <gconf/gconf-client.h>

#ifndef LEGACY
#    include <hildon/hildon-help.h>
#    include <hildon/hildon-note.h>
#    include <hildon/hildon-color-button.h>
#    include <hildon/hildon-file-chooser-dialog.h>
#    include <hildon/hildon-number-editor.h>
#    include <hildon/hildon-banner.h>
#else
#    include <osso-helplib.h>
#    include <hildon-widgets/hildon-note.h>
#    include <hildon-widgets/hildon-color-button.h>
#    include <hildon-widgets/hildon-file-chooser-dialog.h>
#    include <hildon-widgets/hildon-number-editor.h>
#    include <hildon-widgets/hildon-banner.h>
#    include <hildon-widgets/hildon-input-mode-hint.h>
#endif

#include "types.h"
#include "data.h"
#include "defines.h"

#include "gps.h"
#include "display.h"
#include "gdk-pixbuf-rotate.h"
#include "maps.h"
#include "marshal.h"
#include "poi.h"
#include "settings.h"
#include "util.h"

extern AprsDataRow *n_first;

typedef struct _AprsStationSelectInfo AprsStationSelectInfo;
struct _AprsStationSelectInfo
{
    GtkWidget *dialog;
    GtkWidget *tree_view;
    gint      column_index;
    gchar     *call_sign;
};

static AprsStationSelectInfo selected_station;



double convert_lat_l2d(long lat);
double convert_lon_l2d(long lon);
static gboolean panto_station(GtkWidget *widget, AprsStationSelectInfo *aprs_station_sel);

void convert_temp_f_to_c(gchar * f, gchar ** c)
{
	*c = g_strdup("        ");
	
	gdouble df = 0.0;
	gdouble dc = 0.0;
	
	// Convert fahrenheit to fahrenheit (double)
	df = g_ascii_strtod ( f, (gchar*)(f + strlen(f)));
	
	// Convert ff to fc
	dc = 5*((df - 32.0)/9);
	
	// Convert fc to celsius
	snprintf(*c, 8, "%0.1f°C", dc);
}

void setup_aprs_basic_wx_display_page(GtkWidget *notebook, AprsDataRow *p_station)
{
    GtkWidget *table;
    GtkWidget *label;
    

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
        table = gtk_table_new(4/*rows*/, 4/*columns*/, FALSE/*All cells same size*/),
        label = gtk_label_new(_("WX")));

    /* Last update */    
    gchar last_update_time[26];
    
    gtk_table_attach(GTK_TABLE(table),
        label = gtk_label_new("Updated:"),
            0, 1, 0, 1, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);

    
    
    if(p_station->weather_data && p_station->weather_data->wx_sec_time)
    {
    	 strftime(last_update_time, 25, "%x %X", localtime(&p_station->weather_data->wx_sec_time));
    }
    else
    {
    	snprintf(last_update_time, 25, " ");
    }
    
    gtk_table_attach(GTK_TABLE(table),
    		label = gtk_label_new( last_update_time ),
            1, 4, 0, 1, GTK_EXPAND |GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 0.f, 0.5f);
        
    
    /* Temperature */
    gchar * temp = NULL;
    
    gtk_table_attach(GTK_TABLE(table),
        label = gtk_label_new("Temp:"),
            0, 1, 1, 2, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);

    
    if(p_station->weather_data && p_station->weather_data->wx_temp)
    {
    	convert_temp_f_to_c(p_station->weather_data->wx_temp, &temp);
    }
    else
    {
    	temp = g_strdup("");
    }
    
    gtk_table_attach(GTK_TABLE(table),
    		label = gtk_label_new( temp ),
            1, 2, 1, 2, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 0.f, 0.5f);
    
    g_free(temp);
    
    /////////////////

    gtk_table_attach(GTK_TABLE(table),
        label = gtk_label_new( (p_station->weather_data->wx_storm ? "SEVERE STORM" : "")  ),
            2, 4, 1, 2, GTK_EXPAND |GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 0.5f, 0.5f);


    /////
    gchar course[7];
    gtk_table_attach(GTK_TABLE(table),
        label = gtk_label_new( "Wind course:"  ),
            0, 1, 2, 3, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    
    snprintf(course, 6, "%0.f°", 
    		g_ascii_strtod (p_station->weather_data->wx_course, p_station->weather_data->wx_course + strlen(p_station->weather_data->wx_course))
    		);
    gtk_table_attach(GTK_TABLE(table),
        label = gtk_label_new( course ),
            1, 2, 2, 3, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 0.f, 0.5f);
    
//    g_free(course);
    
    /////
    gchar speed[15];
    gtk_table_attach(GTK_TABLE(table),
        label = gtk_label_new( "Wind speed:"  ),
            2, 3, 2, 3, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);

    snprintf(speed, 14, "%0.f->%0.f MPH",
    		g_ascii_strtod (p_station->weather_data->wx_speed, p_station->weather_data->wx_speed + strlen(p_station->weather_data->wx_speed)),
    		g_ascii_strtod (p_station->weather_data->wx_gust, p_station->weather_data->wx_gust + strlen(p_station->weather_data->wx_gust))
    		);
    gtk_table_attach(GTK_TABLE(table),
        label = gtk_label_new( speed ),
            3, 4, 2, 3, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 0.f, 0.5f);
    
  

    
    /////
    gchar rain_ph[17];
    gchar rain_total[17];
    
    gtk_table_attach(GTK_TABLE(table),
        label = gtk_label_new( "Rain fall:"  ),
            0, 1, 3, 4, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);

    if(p_station->weather_data->wx_rain)
    {
	    snprintf(rain_ph, 16, "%0.f \"/hr",
	    		g_ascii_strtod (p_station->weather_data->wx_rain, p_station->weather_data->wx_rain + strlen(p_station->weather_data->wx_rain))
	    		);
    }
    else
    {
    	snprintf(rain_ph, 1, " ");
    }
    gtk_table_attach(GTK_TABLE(table),
        label = gtk_label_new( rain_ph ),
            1, 2, 3, 4, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 0.f, 0.5f);

    
    gtk_table_attach(GTK_TABLE(table),
        label = gtk_label_new( "Total:"  ),
            2, 3, 3, 4, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);

    if(p_station->weather_data->wx_rain_total)
    {
	    snprintf(rain_total, 16, "%0.f \"",
	    		g_ascii_strtod (p_station->weather_data->wx_rain_total, p_station->weather_data->wx_rain_total + strlen(p_station->weather_data->wx_rain_total))
	    		);
    }
    else
    {
    	snprintf(rain_total, 1, " ");
    }
    gtk_table_attach(GTK_TABLE(table),
        label = gtk_label_new( rain_total ),
            3, 4, 3, 4, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 0.f, 0.5f);
    
    
    
    /*

    char    wx_hurricane_radius[4];  //nautical miles       3
    char    wx_trop_storm_radius[4]; //nautical miles       3
    char    wx_whole_gale_radius[4]; // nautical miles      3
    char    wx_snow[6];         // in inches/24h            3
    char    wx_prec_24[10];     // in hundredths inch/day   3
    char    wx_prec_00[10];     // in hundredths inch       3
    char    wx_hum[5];          // in %                     3
    char    wx_baro[10];        // in hPa                   6
    char    wx_fuel_temp[5];    // in °F                    3
    char    wx_fuel_moisture[5];// in %                     2
    char    wx_type;
    char    wx_station[MAX_WXSTATION];
*/

}

void setup_aprs_moving_display_page(GtkWidget *notebook, AprsDataRow *p_station)
{
    GtkWidget *table;
    GtkWidget *label;

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
        table = gtk_table_new(4/*rows*/, 4/*columns*/, FALSE/*All cells same size*/),
        label = gtk_label_new(_("Moving")));


    ////////////
    gtk_table_attach(GTK_TABLE(table),
        label = gtk_label_new("Speed:"),
            0, 1, 0, 1, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);

  
    gchar speed[15];
    
//    snprintf(speed, sizeof(speed), "%.01f %s", atof(p_station->speed) * UNITS_CONVERT[_units], 
//    		UNITS_ENUM_TEXT[_units]);
    
    if(_units == UNITS_NM)
        snprintf(speed, sizeof(speed), "%.01f nmph", atof(p_station->speed));
    else if(_units == UNITS_KM)
    	snprintf(speed, sizeof(speed), "%.01f kph", atof(p_station->speed)*1.852);
    else if(_units == UNITS_MI)
        snprintf(speed, sizeof(speed), "%.01f mph", atof(p_station->speed)*1.1508);

    
    gtk_table_attach(GTK_TABLE(table),
    		label = gtk_label_new( speed ),
            1, 2, 0, 1, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 0.f, 0.5f);
    
    

    ////////////
    gtk_table_attach(GTK_TABLE(table),
        label = gtk_label_new("Course:"),
            0, 1, 1, 2, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);

    
    gtk_table_attach(GTK_TABLE(table),
    		label = gtk_label_new( p_station->course ),
            1, 2, 1, 2, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 0.f, 0.5f);
    
    ////////////
    gtk_table_attach(GTK_TABLE(table),
        label = gtk_label_new("Alt (m):"),
            0, 1, 2, 3, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);

    
    gtk_table_attach(GTK_TABLE(table),
    		label = gtk_label_new( p_station->altitude ),
            1, 2, 2, 3, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 0.f, 0.5f);

    ////////////
    gtk_table_attach(GTK_TABLE(table),
        label = gtk_label_new("Bearing:"),
            0, 1, 3, 4, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);

    
    gtk_table_attach(GTK_TABLE(table),
    		label = gtk_label_new( p_station->bearing ),
            1, 2, 3, 4, GTK_FILL, 0, 2, 4);
    
    /*   
    GdkPixmap *pixmap=gdk_pixmap_new(table->window,100,100,16);
    //gdk_drawable_set_colormap (pixmap,gdk_colormap_get_system ());


    GtkWidget *image = NULL;
 
    GdkColor color;
    GdkGC * gc;
    gc = gdk_gc_new(pixmap);
    color.red = 0;
    color.green = 0;
    color.blue = 0;

    gdk_gc_set_foreground(gc, &color);
    
    color.red = 255;
    color.green = 255;
    color.blue = 255;
    gdk_gc_set_background(gc, &color);

    
    gdk_gc_set_line_attributes(gc, _draw_width, GDK_LINE_SOLID, GDK_CAP_ROUND, GDK_JOIN_ROUND);
            
	gdk_draw_arc (
			pixmap, 
			gc,
			FALSE,
			2, 2,
			96, 96,
			0, 360*64
			);
	
	gdouble heading = deg2rad(atof(p_station->course));
	gint y = (gint)(48.0 * cosf(heading) );
	gint x = (gint)(48.0 * sinf(heading) );
	
	gdk_draw_line (
			pixmap, 
			gc,
			50, 50,
			50+x, 50-y);
	  
    gtk_table_attach(GTK_TABLE(table),
    		image = gtk_image_new_from_pixmap(pixmap, NULL),
    		2, 3, 0, 4, GTK_FILL, 0, 2, 4);
      		//2, 3, 0, 4, GTK_SHRINK, GTK_SHRINK, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(image), 1.0f, 0.5f);
     
    
//    g_object_unref(image);
//    gdk_pixmap_unref(pixmap);
 
 * */
}


void setup_aprs_station_stats_page(GtkWidget *notebook, AprsDataRow *p_station)
{
    GtkWidget *table;
    GtkWidget *label;
    gchar distance[15];
    gchar lat[15], lon[15];
    gchar course_deg[9];

    
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
        table = gtk_table_new(4/*rows*/, 5/*columns*/, FALSE/*All cells same size*/),
        label = gtk_label_new(_("Location")));

    ////////////
    
    course_deg[0] = '\0';
    distance[0] = '\0';
    lat[0] = '\0';
    lon[0] = '\0';
    

	if(p_station->coord_lat != 0 || p_station->coord_lon != 0)
	{
		gdouble d_lat = convert_lat_l2d(p_station->coord_lat);
		gdouble d_lon = convert_lon_l2d(p_station->coord_lon);

		format_lat_lon(d_lat, d_lon, lat, lon);
	    
		gfloat dist = (float)calculate_distance(_gps.lat, _gps.lon, d_lat, d_lon);
		
		
		snprintf(distance, sizeof(distance), "%.01f %s", dist * UNITS_CONVERT[_units], UNITS_ENUM_TEXT[_units]);

		snprintf(course_deg,  sizeof(course_deg),
				"%.01f°",
				calculate_bearing(_gps.lat, _gps.lon, d_lat, d_lon));
	}

    /* Last heard */    
    gchar last_update_time[26];
    
    gtk_table_attach(GTK_TABLE(table),
        label = gtk_label_new("Last Heard:"),
            0, 1, 4, 5, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);

    
    
    if(p_station->sec_heard)
    {
    	 strftime(last_update_time, 25, "%x %X", localtime(&p_station->sec_heard));
    }
    else
    {
    	snprintf(last_update_time, 25, " ");
    }
    
    gtk_table_attach(GTK_TABLE(table),
    		label = gtk_label_new( last_update_time ),
            1, 4, 4, 5, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 0.f, 0.5f);
        

    
	////////////
	gtk_table_attach(GTK_TABLE(table),
        label = gtk_label_new("Location:"),
            0, 1, 1, 2, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);

    gtk_table_attach(GTK_TABLE(table),
    		label = gtk_label_new( lat ),
            1, 3, 1, 2, GTK_EXPAND |GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 0.f, 0.5f);

    gtk_table_attach(GTK_TABLE(table),
    		label = gtk_label_new( lon ),
            3, 5, 1, 2, GTK_EXPAND |GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 0.f, 0.5f);

    ////////////
    
    gtk_table_attach(GTK_TABLE(table),
        label = gtk_label_new(""),
            0, 5, 2, 3, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);

    
    ////////////
    gtk_table_attach(GTK_TABLE(table),
        label = gtk_label_new("Distance:"),
            0, 1, 3, 4, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);

    gtk_table_attach(GTK_TABLE(table),
    		label = gtk_label_new( distance ),
            1, 2, 3, 4, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 0.f, 0.5f);

    //
    
    gtk_table_attach(GTK_TABLE(table),
        label = gtk_label_new("Bearing:"),
            2, 4, 3, 4, GTK_EXPAND |GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);

    gtk_table_attach(GTK_TABLE(table),
    		label = gtk_label_new( course_deg ),
            4, 5, 3, 4, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 0.f, 0.5f);
 

    ////////////
    
}


void setup_aprs_basic_display_page(GtkWidget *notebook, AprsDataRow *p_station)
{
    GtkWidget *table;
    GtkWidget *label;

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
        table = gtk_table_new(5/*rows*/, 5/*columns*/, FALSE/*All cells same size*/),
        label = gtk_label_new(_("Basic")));

    /* Callsign. */
    // Label
    
    gtk_table_attach(GTK_TABLE(table),
        label = gtk_label_new("Callsign:"),
            0, 1, 0, 1, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);

    
    
    gtk_table_attach(GTK_TABLE(table),
    		label = gtk_label_new( p_station->call_sign ),
            1, 2, 0, 1, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 0.f, 0.5f);

    
    GdkPixbuf *pixbuf = NULL;
    
    gint symbol_column = 0;
    gint symbol_row    = 0;
    gint symbol_size = 0;

    extract_aprs_symbol(p_station->aprs_symbol.aprs_symbol, p_station->aprs_symbol.aprs_type, &pixbuf, 
    		&symbol_size, 
    		&symbol_column, 
    		&symbol_row);
    
    

    GdkPixmap *pixmap=gdk_pixmap_new(table->window,symbol_size,symbol_size,16);
    
    	
    // We found an icon to draw. 
    gdk_draw_pixbuf(
        pixmap,
        _gc[COLORABLE_POI],
        pixbuf,
        symbol_size*symbol_column, symbol_size*symbol_row,
        0,
        0,
        symbol_size, symbol_size,
        GDK_RGB_DITHER_NONE, 0, 0);
    g_object_unref(pixbuf);

    GtkWidget *image;

    gtk_table_attach(GTK_TABLE(table),
		image = gtk_image_new_from_pixmap(pixmap, NULL),
        2, 3, 0, 1, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 0.f, 0.5f);

    gtk_table_attach(GTK_TABLE(table),
        label = gtk_label_new("Packets:"),
            3, 4, 0, 1, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);

    
    gchar packets[5];
    snprintf(packets, 4, "%u", p_station->num_packets);
    
    gtk_table_attach(GTK_TABLE(table),
    		label = gtk_label_new( packets ),
            4, 5, 0, 1, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 0.f, 0.5f);
    
    gtk_table_attach(GTK_TABLE(table),
    		label  = gtk_label_new("Comment:"),
            0, 1, 1, 3, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label ), 1.f, 0.5f);
    
    gchar * comment = NULL;
    
    
    if(p_station->comment_data && p_station->comment_data->text_ptr)
    {
    	comment = g_strdup(p_station->comment_data->text_ptr);
    }
    else
    {
    	comment = g_strdup("");
    }
    
    gtk_table_attach(GTK_TABLE(table),
    		label  = gtk_label_new(comment),
            1, 5, 1, 3, GTK_EXPAND |GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label ), 0.f, 0.5f);
    gtk_label_set_width_chars(label, 30);
    
    
    
    //// 

    gtk_table_attach(GTK_TABLE(table),
    		label  = gtk_label_new("Status:"),
            0, 1, 3, 4, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label ), 1.f, 0.5f);
    
    gchar * status = NULL;
    
    
    if(p_station->status_data && p_station->status_data->text_ptr)
    {
    	status = g_strdup(p_station->status_data->text_ptr);
    }
    else
    {
    	status = g_strdup("");
    }
    
    gtk_table_attach(GTK_TABLE(table),
    		label  = gtk_label_new(status),
            1, 5, 3, 4, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label ), 0.f, 0.5f);
    
    
    //// 

    gtk_table_attach(GTK_TABLE(table),
    		label  = gtk_label_new("Path:"),
            0, 1, 4, 5, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label ), 1.f, 0.5f);
    
    gchar * path = NULL;
    
    if(p_station->node_path_ptr)
    {
    	path = g_strdup(p_station->node_path_ptr);
    }
    else
    {
    	path = g_strdup("");
    }
    
    gtk_table_attach(GTK_TABLE(table),
    		label  = gtk_label_new(path),
            1, 5, 4, 5, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label ), 0.f, 0.5f);
        
    

    
}



void ShowAprsStationPopup(AprsDataRow *p_station)
{
	GtkWidget *dialog = NULL;
	GtkWidget *notebook = NULL;
	GtkWidget *btn_panto = NULL;

	dialog = gtk_dialog_new_with_buttons(_("Station Details"),
                GTK_WINDOW(_window), GTK_DIALOG_MODAL,
                GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT,
//                "Send Message...", GTK_RESPONSE_ACCEPT,
                NULL);
	
    
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area),
    		btn_panto = gtk_button_new_with_mnemonic(_("C_entre Map...")));
            
    
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
            notebook = gtk_notebook_new(), TRUE, TRUE, 0);
	

    selected_station.dialog = NULL;
    selected_station.tree_view = NULL;
    selected_station.column_index = 0;
    selected_station.call_sign = p_station->call_sign;
    
    g_signal_connect(G_OBJECT(btn_panto), "clicked",
    		G_CALLBACK(panto_station), &selected_station);
    
    
	setup_aprs_basic_display_page(notebook, p_station);
	
	
	if(p_station->weather_data )
		setup_aprs_basic_wx_display_page(notebook, p_station);

	
	
	if( ( p_station->flag & ST_MOVING) == ST_MOVING){
		setup_aprs_moving_display_page(notebook, p_station);
	}
	
	
	
	setup_aprs_station_stats_page(notebook, p_station);
	
    gtk_widget_show_all(dialog);
    gtk_dialog_run(GTK_DIALOG(dialog));  
    gtk_widget_hide(dialog);
    

}





void list_stations()
{
    static GtkWidget *dialog = NULL;
    static GtkWidget *list = NULL;
    static GtkWidget *sw = NULL;
    static GtkWidget *btn_panto = NULL;
    static GtkTreeViewColumn *column = NULL;
    static GtkCellRenderer *renderer = NULL;
    GtkListStore *store = NULL;
    GtkTreeIter iter;
    gint station_count = 0;

    gint num_cats = 0;

    printf("%s()\n", __PRETTY_FUNCTION__);
    
    typedef enum
    {
        STATION_CALLSIGN,
        STATION_DISTANCE,
        STATION_BEARING,
        STATION_COMMENT,
        STATION_DISTANCE_NUM,
        STATION_NUM_COLUMNS
    } StationList;
    
    /* Initialize store. */
    store = gtk_list_store_new(STATION_NUM_COLUMNS,
    		G_TYPE_STRING,
    		G_TYPE_STRING,
    		G_TYPE_STRING,
    		G_TYPE_STRING,
    		G_TYPE_DOUBLE);/* Category Label */
 
    AprsDataRow *p_station = n_first;

    while ( (p_station) != NULL) 
    {
    	station_count++;
    	
    	gchar * comment = NULL;
    	gchar * callsign = g_strdup(p_station->call_sign);
    	gchar course_deg[8];
    	gchar * formatted_distance = NULL;
    	gdouble distance = 0;
    	
    	course_deg[0] = '\0';
    	 
    	
    	if(p_station->coord_lat != 0 && p_station->coord_lon != 0)
    	{
    		distance = distance_from_my_station(callsign, course_deg, sizeof(course_deg));
    		
	        if(_units == UNITS_KM)
	        	formatted_distance = g_strdup_printf("%.01f km", distance);
	        else if(_units == UNITS_MI)
	        	formatted_distance = g_strdup_printf("%.01f miles", distance);
	        else if(_units == UNITS_NM)
	        	formatted_distance = g_strdup_printf("%.01f nm", distance); 
    	}
    	else
    	{
    		formatted_distance = g_strdup_printf("");
    	}
    	
    	if(p_station->comment_data) comment = g_strdup(p_station->comment_data->text_ptr);
    	else comment = g_strdup("");
    	
  
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
        		STATION_CALLSIGN, callsign,
        		STATION_DISTANCE, formatted_distance,
        		STATION_BEARING, course_deg,
        		STATION_COMMENT, comment,
        		STATION_DISTANCE_NUM, distance,
                -1);
        num_cats++;
        
        g_free(comment);
        g_free(callsign);
        g_free(formatted_distance);
        
        (p_station) = (p_station)->n_next;  // Next element in list
    } // End of while loop

    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store), STATION_DISTANCE_NUM, GTK_SORT_ASCENDING);
    


    if(dialog == NULL)
    {
        dialog = gtk_dialog_new_with_buttons(_("Stations"),
                GTK_WINDOW(_window), GTK_DIALOG_MODAL,
                "Details", GTK_RESPONSE_ACCEPT,
                GTK_STOCK_CLOSE, GTK_RESPONSE_REJECT,
                NULL);


        gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area),
        		btn_panto = gtk_button_new_with_mnemonic(_("C_entre Map...")));
                
        
        
        gtk_window_set_default_size(GTK_WINDOW(dialog), 500, 300);

        sw = gtk_scrolled_window_new (NULL, NULL);
        
        gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw),
                GTK_SHADOW_ETCHED_IN);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                GTK_POLICY_NEVER,
                GTK_POLICY_AUTOMATIC);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
                sw, TRUE, TRUE, 0);

        list = gtk_tree_view_new();
        gtk_container_add(GTK_CONTAINER(sw), list);

        gtk_tree_selection_set_mode(
                gtk_tree_view_get_selection(GTK_TREE_VIEW(list)),
                GTK_SELECTION_SINGLE);
        gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(list), TRUE);

        
        //////
        renderer = gtk_cell_renderer_text_new();
        column = gtk_tree_view_column_new_with_attributes(
                _("Callsign"), renderer, "text", STATION_CALLSIGN, NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(list), column);

        ///////
        renderer = gtk_cell_renderer_text_new();
        column = gtk_tree_view_column_new_with_attributes(
                _("Distance"), renderer, "text", STATION_DISTANCE, NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(list), column);

        /////////
        renderer = gtk_cell_renderer_text_new();
        column = gtk_tree_view_column_new_with_attributes(
                _("Bearing"), renderer, "text", STATION_BEARING, NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(list), column);
        

        /////////
        renderer = gtk_cell_renderer_text_new();
        column = gtk_tree_view_column_new_with_attributes(
                _("Comment"), renderer, "text", STATION_COMMENT, NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(list), column);
        
        

        selected_station.dialog = dialog;
        selected_station.tree_view = list;
        selected_station.call_sign = NULL;
        selected_station.column_index = STATION_CALLSIGN;
        
        g_signal_connect(G_OBJECT(btn_panto), "clicked",
        		G_CALLBACK(panto_station), &selected_station);
                
    }

    gtk_tree_view_set_model(GTK_TREE_VIEW(list), GTK_TREE_MODEL(store));
    g_object_unref(G_OBJECT(store));

    
    //
    gchar *title = g_strdup_printf("Stations (Total: %u)", station_count);
    gtk_window_set_title(GTK_WINDOW(dialog), title);
    g_free(title);
    
    gtk_widget_show_all(dialog);

    while(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(dialog)))
    {

    	if(gtk_tree_selection_get_selected(
                gtk_tree_view_get_selection(GTK_TREE_VIEW(list)),
                NULL, &iter))
        {
    		// Find the callsign
    		p_station = n_first;
	    	while(p_station != NULL)
	    	{
	    		gchar * callsign = NULL;
	            gtk_tree_model_get(GTK_TREE_MODEL(store), &iter,
	                STATION_CALLSIGN, &(callsign),
	                -1);

	            if(strcmp(p_station->call_sign,callsign) == 0)
	    		{
	    			ShowAprsStationPopup(p_station);
	    			break;
	    			
	    		}

	    		
	            p_station = p_station->n_next;
	    	}

        }



    }

    gtk_widget_hide(dialog);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}




static gboolean
send_message(GtkWidget *widget)
{
	fprintf(stderr, "Send message...");
	return FALSE;
}

static gboolean
panto_station(GtkWidget *widget, AprsStationSelectInfo *aprs_station_selected)
{
    GtkTreeModel *store;
    GtkTreeIter iter;
    GtkTreeSelection *selection;
	gchar * callsign = NULL;
    AprsDataRow *p_station = n_first;
    

    printf("%s()\n", __PRETTY_FUNCTION__);

    
	if(aprs_station_selected->call_sign != NULL)
		callsign = aprs_station_selected->call_sign;
	else
	{
		store = gtk_tree_view_get_model(GTK_TREE_VIEW(aprs_station_selected->tree_view));
	    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(aprs_station_selected->tree_view));
	    
	    if(gtk_tree_selection_get_selected(selection, &store, &iter))
	    {

            gtk_tree_model_get(GTK_TREE_MODEL(store), &iter,
                aprs_station_selected->column_index, &(callsign),
                -1);

	    }
	}
	
	// Now findout the location of callsign
	
	p_station = n_first;
	while(p_station != NULL)
	{
        if(strcmp(p_station->call_sign,callsign) == 0)
		{
        	if(p_station->coord_lat == 0 && p_station->coord_lon == 0)
        	{
        		// Invalid position
        	}
        	else
        	{	
	    		gdouble d_lat = convert_lat_l2d(p_station->coord_lat);
	    		gdouble d_lon = convert_lon_l2d(p_station->coord_lon);
	    		Point unit;
	
	        	
	            latlon2unit(d_lat, d_lon, unit.unitx, unit.unity);
	
	            if(_center_mode > 0)
	                gtk_check_menu_item_set_active(
	                        GTK_CHECK_MENU_ITEM(_menu_view_ac_none_item), TRUE);
	
	            map_center_unit(unit);

        	}
	
        	
			break;
			
		}

        p_station = p_station->n_next;
	}

	
		
    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

void list_messages()
{
	static GtkWidget *dialog = NULL;
//	static GtkWidget *btn_send = NULL;
    static GtkWidget *list = NULL;
    static GtkWidget *sw = NULL;
    static GtkTreeViewColumn *column = NULL;
    static GtkCellRenderer *renderer = NULL;
    GtkListStore *store = NULL;
	GtkTreeIter iter;


    printf("%s()\n", __PRETTY_FUNCTION__);

    typedef enum
    {
        MSG_FROM,
        MSG_TO,
        MSG_TEXT,
        MSG_TIMESTAMP,
        MSG_NUM_COLUMNS
    } MessageList;
    
    /* Initialize store. */
    store = gtk_list_store_new(MSG_NUM_COLUMNS,
    		G_TYPE_STRING,
    		G_TYPE_STRING,
    		G_TYPE_STRING,
    		G_TYPE_DOUBLE);

    
    // Loop through each message
    
    gint  i = 0;
    for (i = 0; i < msg_index_end; i++) 
    {
    	gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
        		MSG_FROM, msg_data[msg_index[i]].from_call_sign,
        		MSG_TO,   msg_data[msg_index[i]].call_sign,
        		MSG_TEXT, msg_data[msg_index[i]].message_line,
        		MSG_TIMESTAMP, (gdouble)msg_data[msg_index[i]].sec_heard,
                -1);
        
    }
    
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store), MSG_TIMESTAMP, GTK_SORT_ASCENDING);
        
    if(dialog == NULL)
    {
        dialog = gtk_dialog_new_with_buttons(_("Messages"),
                GTK_WINDOW(_window), GTK_DIALOG_MODAL,
                GTK_STOCK_CLOSE, GTK_RESPONSE_REJECT,
                NULL);
        
        gtk_window_set_default_size(GTK_WINDOW(dialog), 550, 300);

//        gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area),
//        		btn_send = gtk_button_new_with_label(_("Send...")));
        
//        g_signal_connect(G_OBJECT(btn_send), "clicked",
//                        G_CALLBACK(send_message), dialog);
        
	        sw = gtk_scrolled_window_new (NULL, NULL);
	        
	        gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw),
	                GTK_SHADOW_ETCHED_IN);
	        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
	                GTK_POLICY_NEVER,
	                GTK_POLICY_AUTOMATIC);
	        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
	                sw, TRUE, TRUE, 0);
	        
	
	        list = gtk_tree_view_new();
	        gtk_container_add(GTK_CONTAINER(sw), list);
	
	        gtk_tree_selection_set_mode(
	                gtk_tree_view_get_selection(GTK_TREE_VIEW(list)),
	                GTK_SELECTION_SINGLE);
	        gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(list), TRUE);

        

	        renderer = gtk_cell_renderer_text_new();
	        column = gtk_tree_view_column_new_with_attributes(
	                _("From"), renderer, "text", MSG_FROM, NULL);
	        gtk_tree_view_append_column(GTK_TREE_VIEW(list), column);

	        renderer = gtk_cell_renderer_text_new();
	        column = gtk_tree_view_column_new_with_attributes(
	                _("To"), renderer, "text", MSG_TO, NULL);
	        gtk_tree_view_append_column(GTK_TREE_VIEW(list), column);

	        renderer = gtk_cell_renderer_text_new();
	        column = gtk_tree_view_column_new_with_attributes(
	                _("Message"), renderer, "text", MSG_TEXT, NULL);
	        gtk_tree_view_append_column(GTK_TREE_VIEW(list), column);
	        //////
	    }


    	gtk_tree_view_set_model(GTK_TREE_VIEW(list), GTK_TREE_MODEL(store));
	    g_object_unref(G_OBJECT(store));
	
    
	    gtk_widget_show_all(dialog);

	    while(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(dialog)))
	    {
	    }

	    gtk_widget_hide(dialog);

	    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
	
}

#endif //INCLUDE_APRS
