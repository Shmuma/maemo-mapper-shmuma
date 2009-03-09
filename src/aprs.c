/*
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

#ifdef HAVE_CONFIG_H
#    include "config.h"
#endif

#ifdef INCLUDE_APRS

#define _GNU_SOURCE

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "data.h"

#include <pthread.h>

#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-inet-connection.h>
#include <errno.h>
#include "aprs_display.h"

#ifndef LEGACY
#    include <hildon/hildon-note.h>
#    include <hildon/hildon-banner.h>
#else
#    include <hildon-widgets/hildon-note.h>
#    include <hildon-widgets/hildon-banner.h>
#endif

#include "types.h"
#include "data.h"
#include "defines.h"

#include "display.h"
#include "aprs.h"
#include "gps.h"

#ifdef HAVE_LIBGPSBT
#    include "gpsbt.h"
#endif

#include "path.h"
#include "util.h"

#include "aprs_decode.h"

static volatile GThread*	_aprs_inet_thread = NULL;
static volatile GThread*    _aprs_tty_thread = NULL;

static GMutex* 				_aprs_inet_init_mutex = NULL;
static GMutex*				_aprs_tty_init_mutex = NULL;

static gint 				_aprs_rcvr_retry_count = 0;

static guint _aprs_inet_beacon_timer;
static guint _aprs_tty_beacon_timer;


#define VERSIONFRM 	"APRS"
extern AprsDataRow*			n_first;  // pointer to first element in name sorted station list



TWriteBuffer _write_buffer[APRS_PORT_COUNT];

gboolean send_line(gchar* text, gint text_len, TAprsPort port);
void port_read() ;
void aprs_tty_disconnect();

static gboolean aprs_handle_error_idle(gchar *error)
{
    printf("%s(%s)\n", __PRETTY_FUNCTION__, error);

    /* Ask for re-try. */
    if(++_aprs_rcvr_retry_count > 2)
    {
        GtkWidget *confirm;
        gchar buffer[BUFFER_SIZE];

        /* Reset retry count. */
        _aprs_rcvr_retry_count = 0;

        snprintf(buffer, sizeof(buffer), "%s\nRetry?", error);
        confirm = hildon_note_new_confirmation(GTK_WINDOW(_window), buffer);

        aprs_server_disconnect();

        if(GTK_RESPONSE_OK == gtk_dialog_run(GTK_DIALOG(confirm)))
        {
            aprs_server_connect(); /* Try again. */
        }
        else
        {
            /* Reset Connect to APRS menu item. */
            gtk_check_menu_item_set_active(
                    GTK_CHECK_MENU_ITEM(_menu_enable_aprs_inet_item), FALSE);
        }

        /* Ask user to re-connect. */
        gtk_widget_destroy(confirm);
    }
    else
    {
        aprs_server_disconnect();
        aprs_server_connect(); /* Try again. */
    }

    g_free(error);

    vprintf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
    return FALSE;
}


/**
 * Set the connection state.  This function controls all connection-related
 * banners.
 */
void set_aprs_tty_conn_state(ConnState new_conn_state)
{
    printf("%s(%d)\n", __PRETTY_FUNCTION__, new_conn_state);

    switch(_aprs_tty_state = new_conn_state)
    {
        case RCVR_OFF:
        case RCVR_FIXED:
        case RCVR_UP:  
          if(_connect_banner)
            {
                gtk_widget_destroy(_connect_banner);
                _connect_banner = NULL;
            }
          
          
          	if(_aprs_tty_beacon_timer>0) g_source_remove(_aprs_tty_beacon_timer);
          	
          	if(_aprs_enable && _aprs_tty_enable 
          			&& _aprs_enable_tty_tx && _aprs_tty_beacon_interval>0)
          			_aprs_tty_beacon_timer = g_timeout_add(_aprs_tty_beacon_interval*1000 , timer_callback_aprs_tty, NULL);
          	
          	
          	_aprs_rcvr_retry_count = 0;
            break;
        case RCVR_DOWN:
            if(!_connect_banner)
                _connect_banner = hildon_banner_show_animation(
                        _window, NULL, _("Attempting to connect to TNC"));
            
            if(_aprs_tty_beacon_timer>0) g_source_remove(_aprs_tty_beacon_timer);
            	
            	
            break;

        default: ; /* to quell warning. */
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

/**
 * Set the connection state.  This function controls all connection-related
 * banners.
 */
void set_aprs_inet_conn_state(ConnState new_conn_state)
{
    printf("%s(%d)\n", __PRETTY_FUNCTION__, new_conn_state);

    switch(_aprs_inet_state = new_conn_state)
    {
        case RCVR_OFF:
        case RCVR_FIXED:
        case RCVR_UP:  
          if(_connect_banner)
            {
                gtk_widget_destroy(_connect_banner);
                _connect_banner = NULL;
            }
            if(_fix_banner)
            {
                gtk_widget_destroy(_fix_banner);
                _fix_banner = NULL;
            }
            
            if(_aprs_inet_beacon_timer>0) g_source_remove(_aprs_inet_beacon_timer);
            	
            if(_aprs_enable && _aprs_inet_enable 
            		&& _aprs_enable_inet_tx && _aprs_inet_beacon_interval>0)
            	_aprs_inet_beacon_timer = g_timeout_add(_aprs_inet_beacon_interval*1000 , timer_callback_aprs_inet, NULL);
            		
            	
            break;
        case RCVR_DOWN:
            if(_fix_banner)
            {
                gtk_widget_destroy(_fix_banner);
                _fix_banner = NULL;
            }
            if(!_connect_banner)
                _connect_banner = hildon_banner_show_animation(
                        _window, NULL, _("Attempting to connect to APRS server"));
            
            if(_aprs_inet_beacon_timer>0) g_source_remove(_aprs_inet_beacon_timer);
            break;

        default: ; /* to quell warning. */
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}


static gboolean aprs_parse_server_packet(gchar *packet)
{
    decode_ax25_line(packet, APRS_PORT_INET);

    g_free(packet);

    return FALSE;
}


void update_aprs_inet_options(gboolean force)
{
	// If auto filter is not or we are not connected then stop
	if(!_aprs_server_auto_filter_on 
			|| !_aprs_enable 
			|| !_aprs_inet_enable 
			|| _aprs_server_auto_filter_km <= 0) return ;
	

	// Disconnect
	aprs_server_disconnect();
	//Re-connect
	aprs_server_connect();
	
}

gchar *create_aprs_inet_options_string()
{
	gint current_lat = (gint)round(_gps.lat);
	gint current_lon = (gint)round(_gps.lon);
	gchar *filter = NULL;
	
	filter = g_strdup_printf("user %s pass %s vers %s v%s filter r/%d/%d/%d \r\n ",
			_aprs_mycall, _aprs_inet_server_validation, PACKAGE, VERSION,
			current_lat, current_lon, _aprs_server_auto_filter_km );
	
	return filter;
}


static void thread_read_server()
{
    gchar buf[APRS_BUFFER_SIZE];
    gchar *buf_curr = buf;
    gchar *buf_last = buf + sizeof(buf) - 1;
    GnomeVFSFileSize bytes_read;
    GnomeVFSResult vfs_result;
    GnomeVFSInetConnection *iconn = NULL;
    GnomeVFSSocket *socket = NULL;
    GThread *my_thread = g_thread_self();
    gboolean error = FALSE;

    printf("%s(%s)\n", __PRETTY_FUNCTION__, _aprs_server);

    
    //fprintf(stderr, "Starting thread...\n");
    
    /* Lock/Unlock the mutex to ensure that _aprs_inet_thread is done being set. */
    g_mutex_lock(_aprs_inet_init_mutex);
    g_mutex_unlock(_aprs_inet_init_mutex);
    
    if(!error && my_thread == _aprs_inet_thread && _aprs_inet_thread != NULL)
    {
        gint tryno;

        /* Attempt to connect to APRS server. */
        for(tryno = 0; tryno < 10; tryno++)
        {
            /* Create a socket to interact with server. */
            GTimeVal timeout = { 1000, 0 };
            gchar *filter = create_aprs_inet_options_string();
            //fprintf(stderr, filter);              

            if(GNOME_VFS_OK != (vfs_result = gnome_vfs_inet_connection_create(
                            &iconn,
                            _aprs_server,
                            _aprs_server_port,
                            NULL))
               || NULL == ( socket = gnome_vfs_inet_connection_to_socket(iconn))
               || GNOME_VFS_OK != (vfs_result = gnome_vfs_socket_set_timeout(
            		   socket, &timeout, NULL))
               || GNOME_VFS_OK != (vfs_result = gnome_vfs_socket_write( socket,
						filter, strlen(filter), &bytes_read, NULL))
              )
            {
            	g_free(filter);
                sleep(1);
            }
            else
            {
            	g_free(filter);
                break;
            }
        }


        if(!iconn)
        {
            g_printerr("Error connecting to APRS server: (%d) %s\n",
                    vfs_result, gnome_vfs_result_to_string(vfs_result));
            g_idle_add((GSourceFunc)aprs_handle_error_idle,
                    g_strdup_printf("%s",
                    _("Error connecting to APRS server.")));
            error = TRUE;
        }
    }
    

    if(!error && my_thread == _aprs_inet_thread && _aprs_inet_thread != NULL)
    {
    	
        set_aprs_inet_conn_state(RCVR_UP);

        while(my_thread == _aprs_inet_thread && _aprs_inet_thread != NULL)
        {
            gchar *eol;
            
            	
            vfs_result = gnome_vfs_socket_read( 
            		socket,
                    buf,
                    buf_last - buf_curr,
                    &bytes_read,
                    NULL);
            
  
            if(vfs_result != GNOME_VFS_OK)
            {
                if(my_thread == _aprs_inet_thread)
                {
                    // Error wasn't user-initiated. 
                    g_idle_add((GSourceFunc)aprs_handle_error_idle,
                            g_strdup_printf("%s %u",
                                _("Error reading APRS data."), vfs_result));

                }

                fprintf(stderr, "Read error: %s\n", gnome_vfs_result_to_string(vfs_result));
                error = TRUE;
                break;
            }

            /* Loop through the buffer and read each packet. */
            buf_curr += bytes_read;
            *buf_curr = '\0'; /* append a \0 so we can read as string */
            while(!error && my_thread == _aprs_inet_thread && _aprs_inet_thread != NULL 
            		&& (eol = strchr(buf, '\n')))
            {
                /* This is the beginning of a sentence; okay to parse. */
                *eol = '\0'; /* overwrite \n with \0 */

                if(my_thread == _aprs_inet_thread)
                	//g_idle_add_full(G_PRIORITY_HIGH, (GSourceFunc)aprs_parse_server_packet, g_strdup(buf), NULL );
                    g_idle_add((GSourceFunc)aprs_parse_server_packet, g_strdup(buf));

                /* If eol is at or after (buf_curr - 1) */
                if(eol >= (buf_curr - 1))
                {
                    /* Last read was a newline - reset read buffer */
                    buf_curr = buf;
                    *buf_curr = '\0';
                }
                else
                {
                    /* Move the next line to the front of the buffer. */
                    memmove(buf, eol + 1,
                            buf_curr - eol); /* include terminating 0 */
                    /* Subtract _curr so that it's pointing at the new \0. */
                    buf_curr -= (eol - buf + 1);
                }
            }
            _aprs_rcvr_retry_count = 0;
   
            // Send any packets queued
			// try to get lock, otherwise try next time
			if(g_mutex_trylock (_write_buffer[APRS_PORT_INET].write_lock))
			{
	            	// Store the current end pointer as it may change
				
                gint quantity = 0;
                gchar tmp_write_buffer[MAX_DEVICE_BUFFER];
                while (_write_buffer[APRS_PORT_INET].write_in_pos != _write_buffer[APRS_PORT_INET].write_out_pos) {

                	tmp_write_buffer[quantity] = _write_buffer[APRS_PORT_INET].device_write_buffer[_write_buffer[APRS_PORT_INET].write_out_pos];

                	_write_buffer[APRS_PORT_INET].write_out_pos++;
                    if (_write_buffer[APRS_PORT_INET].write_out_pos >= MAX_DEVICE_BUFFER)
                    	_write_buffer[APRS_PORT_INET].write_out_pos = 0;

                    quantity++;
                }

                if(quantity>0)
                {
                	sleep(2);
                	
                	GnomeVFSFileSize bytes_read = 0;                                    
					if(GNOME_VFS_OK == gnome_vfs_socket_write( socket,
							tmp_write_buffer, quantity, &bytes_read, NULL))
					{
						// OK
						//fprintf(stderr, "Send packet success: %s (%u)\n", tmp_write_buffer, quantity);
					}
					else
					{
						// Failed
						fprintf(stderr, "Failed to send packet: %s (%u)\n", tmp_write_buffer, quantity);
					}
					
	                sleep(1);
                }
              	            	
	            g_mutex_unlock(_write_buffer[APRS_PORT_INET].write_lock);
			}            

        }
        
        //fprintf(stderr, "Exiting thread...\n");
    }

    /* Error, or we're done reading APRS data. */

    /* Clean up. */
    if(iconn)
    	gnome_vfs_inet_connection_destroy(iconn, NULL);
        //gnome_vfs_inet_connection_free(iconn, NULL);
    
    iconn = NULL;
    
    //g_thread_exit(0);
    
    
    printf("%s(): return\n", __PRETTY_FUNCTION__);
    
    return;
}

/**
 * Disconnect from the receiver.  This method cleans up any and everything
 * that might be associated with the receiver.
 */
void aprs_server_disconnect()
{
    gboolean exit_now = FALSE;

    printf("%s()\n", __PRETTY_FUNCTION__);

    GThread *my_thread = g_thread_self();

    if(my_thread == _aprs_inet_thread)
    {
        exit_now = TRUE;
    }

    g_mutex_lock(_aprs_inet_init_mutex);
    _aprs_inet_thread = NULL;
    g_mutex_unlock(_aprs_inet_init_mutex);

    
    
    if(_window)
        set_aprs_inet_conn_state(RCVR_OFF);
    vprintf("%s(): return\n", __PRETTY_FUNCTION__);

    if(exit_now) 
    {
    	fprintf(stderr, "Stopping own thread - APRS server\n");
    	exit(0);
    	fprintf(stderr, "Stop Failed\n");
    	
    }
}

/**
 * Connect to the server.
 * This method assumes that _fd is -1 and _channel is NULL.  If unsure, call
 * rcvr_disconnect() first.
 * Since this is an idle function, this function returns whether or not it
 * should be called again, which is always FALSE.
 */
gboolean aprs_server_connect()
{
    printf("%s(%d)\n", __PRETTY_FUNCTION__, _aprs_inet_state);

    if(_aprs_inet_enable && _aprs_inet_state == RCVR_OFF)
    {
        set_aprs_inet_conn_state(RCVR_DOWN);

        /* Lock/Unlock the mutex to ensure that the thread doesn't
         * start until _gps_thread is set. */
        g_mutex_lock(_aprs_inet_init_mutex);

        
        _aprs_inet_thread = g_thread_create((GThreadFunc)thread_read_server,
                NULL, TRUE, NULL); /* Joinable. */
        
//        g_thread_set_priority(_aprs_inet_thread, G_THREAD_PRIORITY_LOW);
        
        g_mutex_unlock(_aprs_inet_init_mutex);
    }

    vprintf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
    return FALSE;
}

void aprs_init()
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    _aprs_inet_init_mutex = g_mutex_new();
    _aprs_tty_init_mutex  = g_mutex_new();
    _write_buffer[APRS_PORT_INET].write_lock = g_mutex_new();
    _write_buffer[APRS_PORT_TTY].write_lock = g_mutex_new();

    vprintf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
}

void aprs_destroy(gboolean last)
{
    static GThread* tmp = NULL;
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(!last)
    {
        if(_aprs_inet_thread)
        {
            tmp = (GThread*)_aprs_inet_thread;
            _aprs_inet_thread = NULL;
        }
    }
    else if(tmp)
        g_thread_join(tmp);

    vprintf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
}

gboolean select_aprs(gint unitx, gint unity, gboolean quick)
{	
    gint x, y;
    gdouble lat1, lon1, lat2, lon2;
    static GtkWidget *dialog = NULL;
    static GtkWidget *list = NULL;
    static GtkWidget *sw = NULL;
    static GtkTreeViewColumn *column = NULL;
    static GtkCellRenderer *renderer = NULL;
    GtkListStore *store = NULL;
    GtkTreeIter iter;
    gboolean selected = FALSE;
    gint num_stations = 0;
    AprsStationList *first_station = NULL;
    AprsStationList *last_station = NULL;


    printf("%s()\n", __PRETTY_FUNCTION__);

    x = unitx - pixel2unit(3 * _draw_width);
    y = unity + pixel2unit(3 * _draw_width);
    unit2latlon(x, y, lat1, lon1);

    x = unitx + pixel2unit(3 * _draw_width);
    y = unity - pixel2unit(3 * _draw_width);
    unit2latlon(x, y, lat2, lon2);
    gdouble lat, lon;

    
    AprsDataRow *p_station = (AprsDataRow *)n_first;

    // Look for all stations in selected area
    while ( (p_station) != NULL) 
    { 
        lat = convert_lat_l2d(p_station->coord_lat);
        lon = convert_lon_l2d(p_station->coord_lon);

        if ( ( lat2 >= lat && lat >= lat1 ) && (lon2 >= lon && lon >= lon1) )
        {
            // This may have been clicked on
    		AprsStationList * p_list_item = (AprsStationList *)malloc(sizeof(AprsStationList));

    		p_list_item->station = p_station;
    		p_list_item->next = NULL;
    		
        	if(first_station == NULL)
        	{

        		first_station = p_list_item;
        		last_station = p_list_item;
        	}
        	else
        	{
        		last_station->next = p_list_item;
        		last_station = p_list_item;
        	}
        	
        	num_stations++;
        }

        (p_station) = (p_station)->n_next;  // Next element in list
    } // End of while loop

    selected = FALSE;
    
    if(num_stations==0)
    {
    	// No station found, maybe a POI was selected?
    }
    else if(num_stations == 1)
    {
    	// Only one station was found, so display it's info
    	if(first_station->station != NULL)
    	{
	    	ShowAprsStationPopup(first_station->station);
    	}
    	selected = TRUE;
    }
    else
    {
    	// Multiple possibilities, therefore ask the user which one
    	
    	// Initialize store. 
    	store = gtk_list_store_new(APRSPOI_NUM_COLUMNS,
                               G_TYPE_BOOLEAN, //Selected 
                               G_TYPE_STRING // Callsign
                        );

    	AprsStationList * p_list_item = first_station;
    	
    	while(p_list_item != NULL)
    	{
    		if(p_list_item->station != NULL)
    		{
	    		gtk_list_store_append(store, &iter);
	
		        gtk_list_store_set(store, &iter,
		            APRSPOI_CALLSIGN, g_strdup(p_list_item->station->call_sign),
		            -1);
    		}
    		p_list_item = p_list_item->next;
    	}
  
    	
	    if(dialog == NULL)
	    {
	        dialog = gtk_dialog_new_with_buttons(_("Select APRS Station"),
	                GTK_WINDOW(_window), GTK_DIALOG_MODAL,
	                GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
	                GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
	                NULL);
	
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

	        renderer = gtk_cell_renderer_text_new();
	        column = gtk_tree_view_column_new_with_attributes(
	                _("Callsign"), renderer, "text", APRSPOI_CALLSIGN, NULL);
	        gtk_tree_view_append_column(GTK_TREE_VIEW(list), column);

	    }

	    
	    gtk_tree_view_set_model(GTK_TREE_VIEW(list), GTK_TREE_MODEL(store));
	    g_object_unref(G_OBJECT(store));
	
	    gtk_widget_show_all(dialog);
	    
	    if(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(dialog)))
	    {
	    	if(gtk_tree_selection_get_selected(
                    gtk_tree_view_get_selection(GTK_TREE_VIEW(list)),
                    NULL, &iter))
	        {
	    		// Find the callsign
	    		p_list_item = first_station;
		    	while(p_list_item != NULL)
		    	{
		    		if(p_list_item->station != NULL)
		    		{	
			    		gchar * callsign = NULL;
			            gtk_tree_model_get(GTK_TREE_MODEL(store), &iter,
			                APRSPOI_CALLSIGN, &(callsign),
			                -1);
	
			            if(strcmp(p_list_item->station->call_sign,callsign) == 0)
			    		{
			            	gtk_widget_hide(dialog);
			            	
			    			ShowAprsStationPopup(p_list_item->station);
			    			selected = TRUE;
			    			break;
			    			
			    		}
		    		}
		    		
		    		p_list_item = p_list_item->next;
		    	}

	        }

	    }

    	// Ensure it has been closed
    	gtk_widget_hide(dialog);
    }


    // Free the list, but not the stations
    if(first_station)
    {
	    AprsStationList * p_list_item = first_station;
	    
	    while(first_station)
	    {
	    	// Store pointer to delete contents after next pointer is stored	
	    	p_list_item = first_station;
	    
	    	// Move pointer to next
	    	first_station = p_list_item->next;
	    	
	    	free(p_list_item);
	    }
    }


    vprintf("%s(): return %d\n", __PRETTY_FUNCTION__, selected);
    return selected;

}



//*****************************************************************
// distance_from_my_station - compute distance from my station and
//       course with a given call
//
// return distance and course
//
// Returns 0.0 for distance if station not found in database or the
// station hasn't sent out a posit yet.
//*****************************************************************

double distance_from_my_station(char *call_sign, gchar *course_deg, gint course_len) {
    AprsDataRow *p_station;
    double distance;
    float value;
    double d_lat, d_lon;

    distance = 0.0;
    p_station = NULL;
    if (search_station_name(&p_station,call_sign,1)) {
        // Check whether we have a posit yet for this station
        if ( (p_station->coord_lat == 0l)
                && (p_station->coord_lon == 0l) ) {
            distance = 0.0;
        }
        else {
        	d_lat = convert_lat_l2d(p_station->coord_lat);
        	d_lon = convert_lon_l2d(p_station->coord_lon);
        	
        	value = (float)calculate_distance(_gps.lat, _gps.lon, d_lat, d_lon);
        	
        	snprintf(course_deg,  course_len,
        			"%.01f°",
        			calculate_bearing(_gps.lat, _gps.lon, d_lat, d_lon));
        	
            if(_units == UNITS_KM)
            	distance = value * 1.852;           // nautical miles to km
            else if(_units == UNITS_MI)
                distance = value * 1.15078;         // nautical miles to miles
            else if(_units == UNITS_NM)
            	distance = value; 
            else
                distance = 0.0; // Should be unreachable
            
                
        }
    }
    else {  // Station not found
        distance = 0.0;
    }


    return(distance);
}



void pad_callsign(char *callsignout, char *callsignin) {
    int i,l;

    l=(int)strlen(callsignin);
    for(i=0; i<9;i++) {
        if(i<l) {
            if(isalnum((int)callsignin[i]) || callsignin[i]=='-') {
                callsignout[i]=callsignin[i];
            }
            else {
                callsignout[i] = ' ';
            }
        }
        else {
            callsignout[i] = ' ';
        }
    }
    callsignout[i] = '\0';
}
/////////// TX functionality



// This routine changes callsign chars to proper uppercase chars or
// numerals, fixes the callsign to six bytes, shifts the letters left by
// one bit, and puts the SSID number into the proper bits in the seventh
// byte.  The callsign as processed is ready for inclusion in an
// AX.25 header.
//
void fix_up_callsign(unsigned char *data, int data_size) {
    unsigned char new_call[8] = "       ";  // Start with seven spaces
    int ssid = 0;
    int i;
    int j = 0;
    int digipeated_flag = 0;


    // Check whether we've digipeated through this callsign yet.
    if (strstr((const char *)data,"*") != 0) {
         digipeated_flag++;
    }

    // Change callsign to upper-case and pad out to six places with
    // space characters.
    for (i = 0; i < (int)strlen((const char *)data); i++) {
        toupper(data[i]);

        if (data[i] == '-') {   // Stop at '-'
            break;
        }
        else if (data[i] == '*') {
        }
        else {
            new_call[j++] = data[i];
        }
    }
    new_call[7] = '\0';

    //fprintf(stderr,"new_call:(%s)\n",new_call);

    // Handle SSID.  'i' should now be pointing at a dash or at the
    // terminating zero character.
    if ( (i < (int)strlen((const char *)data)) && (data[i++] == '-') ) {   // We might have an SSID
        if (data[i] != '\0')
            ssid = atoi((const char *)&data[i]);
//            ssid = data[i++] - 0x30;    // Convert from ascii to int
//        if (data[i] != '\0')
//            ssid = (ssid * 10) + (data[i] - 0x30);
    }

//fprintf(stderr,"SSID:%d\t",ssid);

    if (ssid >= 0 && ssid <= 15) {
        new_call[6] = ssid | 0x30;  // Set 2 reserved bits
    }
    else {  // Whacko SSID.  Set it to zero
        new_call[6] = 0x30;     // Set 2 reserved bits
    }

    if (digipeated_flag) {
        new_call[6] = new_call[6] | 0x40; // Set the 'H' bit
    }
 
    // Shift each byte one bit to the left
    for (i = 0; i < 7; i++) {
        new_call[i] = new_call[i] << 1;
        new_call[i] = new_call[i] & 0xfe;
    }

//fprintf(stderr,"Last:%0x\n",new_call[6]);

    // Write over the top of the input string with the newly
    // formatted callsign
    xastir_snprintf((char *)data,
        data_size,
        "%s",
        new_call);
}




// Create an AX25 frame and then turn it into a KISS packet.  Dump
// it into the transmit queue.
//
void send_ax25_frame(TAprsPort port, gchar *source, gchar *destination, gchar *path, gchar *data) {
    unsigned char temp_source[15];
    unsigned char temp_dest[15];
    unsigned char temp[15];
    unsigned char control[2], pid[2];
    unsigned char transmit_txt[MAX_LINE_SIZE*2];
    unsigned char transmit_txt2[MAX_LINE_SIZE*2];
    unsigned char c;
    int i, j;
    int erd;
    int write_in_pos_hold;


//fprintf(stderr,"KISS String:%s>%s,%s:%s\n",source,destination,path,data);

    // Check whether transmits are disabled globally
//    if (transmit_disable) {
//        return;
//    }    

    // Check whether transmit has been enabled for this interface.
    // If not, get out while the gettin's good.
//    if (devices[port].transmit_data != 1) {
//        return;
//    }

    transmit_txt[0] = '\0';

    // Format the destination callsign
    snprintf((char *)temp_dest,
        sizeof(temp_dest),
        "%s",
        destination);
    fix_up_callsign(temp_dest, sizeof(temp_dest));
    
    snprintf((char *)transmit_txt,
        sizeof(transmit_txt),
        "%s",
        temp_dest);

    // Format the source callsign
    snprintf((char *)temp_source,
        sizeof(temp_source),
        "%s",
        source);
    fix_up_callsign(temp_source, sizeof(temp_source));
    
    strncat((char *)transmit_txt,
        (char *)temp_source,
        sizeof(transmit_txt) - strlen((char *)transmit_txt));

    // Break up the path into individual callsigns and send them one
    // by one to fix_up_callsign().  If we get passed an empty path,
    // we merely skip this section and no path gets added to
    // "transmit_txt".
    j = 0;
    temp[0] = '\0'; // Start with empty path
    if ( (path != NULL) && (strlen(path) != 0) ) {
        while (path[j] != '\0') {
            i = 0;
            while ( (path[j] != ',') && (path[j] != '\0') ) {
                temp[i++] = path[j++];
            }
            temp[i] = '\0';

            if (path[j] == ',') {   // Skip over comma
                j++;
            }

            fix_up_callsign(temp, sizeof(temp));
            strncat((char *)transmit_txt,
                (char *)temp,
                sizeof(transmit_txt) - strlen((char *)transmit_txt));
        }
    }

    // Set the end-of-address bit on the last callsign in the
    // address field
    transmit_txt[strlen((const char *)transmit_txt) - 1] |= 0x01;

    // Add the Control byte
    control[0] = 0x03;
    control[1] = '\0';
    strncat((char *)transmit_txt,
        (char *)control,
        sizeof(transmit_txt) - strlen((char *)transmit_txt));

    // Add the PID byte
    pid[0] = 0xf0;
    pid[1] = '\0';
    strncat((char *)transmit_txt,
        (char *)pid,
        sizeof(transmit_txt) - strlen((char *)transmit_txt));

    // Append the information chars
    strncat((char *)transmit_txt,
        data,
        sizeof(transmit_txt) - strlen((char *)transmit_txt));

    //fprintf(stderr,"%s\n",transmit_txt);

    // Add the KISS framing characters and do the proper escapes.
    j = 0;
    transmit_txt2[j++] = KISS_FEND;

    // Note:  This byte is where different interfaces would be
    // specified:
    transmit_txt2[j++] = 0x00;

    for (i = 0; i < (int)strlen((const char *)transmit_txt); i++) {
        c = transmit_txt[i];
        if (c == KISS_FEND) {
            transmit_txt2[j++] = KISS_FESC;
            transmit_txt2[j++] = KISS_TFEND;
        }
        else if (c == KISS_FESC) {
            transmit_txt2[j++] = KISS_FESC;
            transmit_txt2[j++] = KISS_TFESC;
        }
        else {
            transmit_txt2[j++] = c;
        }
    }
    transmit_txt2[j++] = KISS_FEND;

    // Terminate the string, but don't increment the 'j' counter.
    // We don't want to send the NULL byte out the KISS interface,
    // just make sure the string is terminated in all cases.
    //
    transmit_txt2[j] = '\0';

//-------------------------------------------------------------------
// Had to snag code from port_write_string() below because our string
// needs to have 0x00 chars inside it.  port_write_string() can't
// handle that case.  It's a good thing the transmit queue stuff
// could handle it.
//-------------------------------------------------------------------

    erd = 0;


    

	port_write_string(
			transmit_txt2,
			j/*length*/,
		APRS_PORT_TTY);

	
  	
/*
    g_mutex_lock (_write_buffer[port].write_lock);
    {
	
	    write_in_pos_hold = _write_buffer[port].write_in_pos;
	
	    for (i = 0; i < j && !erd; i++) {
	    	_write_buffer[port].device_write_buffer[_write_buffer[port].write_in_pos++] = transmit_txt2[i];
	        if (_write_buffer[port].write_in_pos >= MAX_DEVICE_BUFFER)
	        	_write_buffer[port].write_in_pos = 0;
	
	        if (_write_buffer[port].write_in_pos == _write_buffer[port].write_out_pos) {
	
	            // clear this restore original write_in pos and dump this string 
	        	_write_buffer[port].write_in_pos = write_in_pos_hold;
	        	_write_buffer[port].errors++;
	            erd = 1;
	        }
	    }
	    
	    g_mutex_unlock (_write_buffer[port].write_lock);
    }
*/
}




// convert latitude from long to string 
// Input is in Xastir coordinate system
//
// CONVERT_LP_NOSP      = DDMM.MMN
// CONVERT_HP_NOSP      = DDMM.MMMN
// CONVERT_VHP_NOSP     = DDMM.MMMMN
// CONVERT_LP_NORMAL    = DD MM.MMN
// CONVERT_HP_NORMAL    = DD MM.MMMN
// CONVERT_UP_TRK       = NDD MM.MMMM
// CONVERT_DEC_DEG      = DD.DDDDDN
// CONVERT_DMS_NORMAL   = DD MM SS.SN
// CONVERT_DMS_NORMAL_FORMATED   = DD'MM'SS.SN
// CONVERT_HP_NORMAL_FORMATED   = DD'MM.MMMMN
//
void convert_lat_l2s(long lat, char *str, int str_len, int type) {
    char ns;
    float deg, min, sec;
    int ideg, imin;
    long temp;


    str[0] = '\0';
    deg = (float)(lat - 32400000l) / 360000.0;
 
    // Switch to integer arithmetic to avoid floating-point
    // rounding errors.
    temp = (long)(deg * 100000);

    ns = 'S';
    if (temp <= 0) {
        ns = 'N';
        temp = labs(temp);
    }   

    ideg = (int)temp / 100000;
    min = (temp % 100000) * 60.0 / 100000.0;

    // Again switch to integer arithmetic to avoid floating-point
    // rounding errors.
    temp = (long)(min * 1000);
    imin = (int)(temp / 1000);
    sec = (temp % 1000) * 60.0 / 1000.0;

    switch (type) {

        case(CONVERT_LP_NOSP): /* do low P w/no space */
            xastir_snprintf(str,
                str_len,
                "%02d%05.2f%c",
                ideg,
//                min+0.001, // Correct possible unbiased rounding
                min,
                ns);
            break;

        case(CONVERT_LP_NORMAL): /* do low P normal */
            xastir_snprintf(str,
                str_len,
                "%02d %05.2f%c",
                ideg,
//                min+0.001, // Correct possible unbiased rounding
                min,
                ns);
            break;

        case(CONVERT_HP_NOSP): /* do HP w/no space */
            xastir_snprintf(str,
                str_len,
                "%02d%06.3f%c",
                ideg,
//                min+0.0001, // Correct possible unbiased rounding
                min,
                ns);
            break;

        case(CONVERT_VHP_NOSP): /* do Very HP w/no space */
            xastir_snprintf(str,
                str_len,
                "%02d%07.4f%c",
                ideg,
//                min+0.00001, // Correct possible unbiased rounding
                min,
                ns);
            break;

        case(CONVERT_UP_TRK): /* for tracklog files */
            xastir_snprintf(str,
                str_len,
                "%c%02d %07.4f",
                ns,
                ideg,
//                min+0.00001); // Correct possible unbiased rounding
                min);
            break;

        case(CONVERT_DEC_DEG):
            xastir_snprintf(str,
                str_len,
                "%08.5f%c",
//                (ideg+min/60.0)+0.000001, // Correct possible unbiased rounding
                ideg+min/60.0,
                ns);
            break;

        case(CONVERT_DMS_NORMAL):
            xastir_snprintf(str,
                str_len,
                "%02d %02d %04.1f%c",
                ideg,
                imin,
//                sec+0.01, // Correct possible unbiased rounding
                sec,
                ns);
            break;
        
        case(CONVERT_DMS_NORMAL_FORMATED):
            xastir_snprintf(str,
                str_len,
                "%02d°%02d\'%04.1f%c",
                ideg,
                imin,
//                sec+0.01, // Correct possible unbiased rounding
                sec,
                ns);
            break;

        case(CONVERT_HP_NORMAL_FORMATED):
            xastir_snprintf(str,
                str_len,
                "%02d°%06.3f%c",
                ideg,
//                min+0.0001, // Correct possible unbiased roundin
                min,
                ns);
            break;
        
        case(CONVERT_HP_NORMAL):
        default: /* do HP normal */
            xastir_snprintf(str,
                str_len,
                "%02d %06.3f%c",
                ideg,
//                min+0.0001, // Correct possible unbiased rounding
                min,
                ns);
            break;
    }
}





// convert longitude from long to string
// Input is in Xastir coordinate system
//
// CONVERT_LP_NOSP      = DDDMM.MME
// CONVERT_HP_NOSP      = DDDMM.MMME
// CONVERT_VHP_NOSP     = DDDMM.MMMME
// CONVERT_LP_NORMAL    = DDD MM.MME
// CONVERT_HP_NORMAL    = DDD MM.MMME
// CONVERT_UP_TRK       = EDDD MM.MMMM
// CONVERT_DEC_DEG      = DDD.DDDDDE
// CONVERT_DMS_NORMAL   = DDD MM SS.SN
// CONVERT_DMS_NORMAL_FORMATED   = DDD'MM'SS.SN
//
void convert_lon_l2s(long lon, char *str, int str_len, int type) {
    char ew;
    float deg, min, sec;
    int ideg, imin;
    long temp;

    str[0] = '\0';
    deg = (float)(lon - 64800000l) / 360000.0;

    // Switch to integer arithmetic to avoid floating-point rounding
    // errors.
    temp = (long)(deg * 100000);

    ew = 'E';
    if (temp <= 0) {
        ew = 'W';
        temp = labs(temp);
    }

    ideg = (int)temp / 100000;
    min = (temp % 100000) * 60.0 / 100000.0;

    // Again switch to integer arithmetic to avoid floating-point
    // rounding errors.
    temp = (long)(min * 1000);
    imin = (int)(temp / 1000);
    sec = (temp % 1000) * 60.0 / 1000.0;

    switch(type) {

        case(CONVERT_LP_NOSP): /* do low P w/nospacel */
            xastir_snprintf(str,
                str_len,
                "%03d%05.2f%c",
                ideg,
//                min+0.001, // Correct possible unbiased rounding
                min,
                ew);
            break;

        case(CONVERT_LP_NORMAL): /* do low P normal */
            xastir_snprintf(str,
                str_len,
                "%03d %05.2f%c",
                ideg,
//                min+0.001, // Correct possible unbiased rounding
                min,
                ew);
            break;

        case(CONVERT_HP_NOSP): /* do HP w/nospace */
            xastir_snprintf(str,
                str_len,
                "%03d%06.3f%c",
                ideg,
//                min+0.0001, // Correct possible unbiased rounding
                min,
                ew);
            break;

        case(CONVERT_VHP_NOSP): /* do Very HP w/nospace */
            xastir_snprintf(str,
                str_len,
                "%03d%07.4f%c",
                ideg,
//                min+0.00001, // Correct possible unbiased rounding
                min,
                ew);
            break;

        case(CONVERT_UP_TRK): /* for tracklog files */
            xastir_snprintf(str,
                str_len,
                "%c%03d %07.4f",
                ew,
                ideg,
//                min+0.00001); // Correct possible unbiased rounding
                min);
            break;

        case(CONVERT_DEC_DEG):
            xastir_snprintf(str,
                str_len,
                "%09.5f%c",
//                (ideg+min/60.0)+0.000001, // Correct possible unbiased rounding
                ideg+min/60.0,
                ew);
            break;

        case(CONVERT_DMS_NORMAL):
            xastir_snprintf(str,
                str_len,
                "%03d %02d %04.1f%c",
                ideg,
                imin,
//                sec+0.01, // Correct possible unbiased rounding
                sec,
                ew);
            break;

        case(CONVERT_DMS_NORMAL_FORMATED):
            xastir_snprintf(str,
                str_len,
                "%03d°%02d\'%04.1f%c",
                ideg,
                imin,
//                sec+0.01, // Correct possible unbiased rounding
                sec,
                ew);
            break;
        
        case(CONVERT_HP_NORMAL_FORMATED):
            xastir_snprintf(str,
                str_len,
                "%03d°%06.3f%c",
                ideg,
//                min+0.0001, // Correct possible unbiased rounding
                min,
                ew);
            break;

        case(CONVERT_HP_NORMAL):
        default: /* do HP normal */
            xastir_snprintf(str,
                str_len,
                "%03d %06.3f%c",
                ideg,
//                min+0.0001, // Correct possible unbiased rounding
                min,
                ew);
            break;
    }
}






/*************************************************************************/
/* output_lat - format position with position_amb_chars for transmission */
/*************************************************************************/
/*
char *output_lat(char *in_lat, int comp_pos) {
    int i,j;
    int position_amb_chars = 0;
//fprintf(stderr,"in_lat:%s\n", in_lat);

    if (!comp_pos) {
        // Don't do this as it results in truncation!
        //in_lat[7]=in_lat[8]; // Shift N/S down for transmission
    }
    else if (position_amb_chars>0) {
        in_lat[7]='0';
    }

    j=0;
    if (position_amb_chars>0 && position_amb_chars<5) {
        for (i=6;i>(6-position_amb_chars-j);i--) {
            if (i==4) {
                i--;
                j=1;
            }
            if (!comp_pos) {
                in_lat[i]=' ';
            } else
                in_lat[i]='0';
        }
    }

    if (!comp_pos) {
        in_lat[8] = '\0';
    }

    return(in_lat);
}
*/



/**************************************************************************/
/* output_long - format position with position_amb_chars for transmission */
/**************************************************************************/
/*
char *output_long(char *in_long, int comp_pos) {
    int i,j;
int position_amb_chars = 0;
//fprintf(stderr,"in_long:%s\n", in_long);

    if (!comp_pos) {
        // Don't do this as it results in truncation!
        //in_long[8]=in_long[9]; // Shift e/w down for transmission
    }
    else if (position_amb_chars>0) {
        in_long[8]='0';
    }

    j=0;
    if (position_amb_chars>0 && position_amb_chars<5) {
        for (i=7;i>(7-position_amb_chars-j);i--) {
            if (i==5) {
                i--;
                j=1;
            }
            if (!comp_pos) {
                in_long[i]=' ';
            } else
                in_long[i]='0';
        }
    }

    if (!comp_pos)
        in_long[9] = '\0';

    return(in_long);
}
*/



//***********************************************************
// output_my_aprs_data
// This is the function responsible for sending out my own
// posits.  The next function below this one handles objects,
// messages and the like (output_my_data).
//***********************************************************/

/*
void create_output_lat_long(gchar *my_output_lat, gchar *my_output_long )
{
    _aprs_transmit_compressed_posit = FALSE;
    gchar *my_lat = g_strdup_printf("%lf", _gps.lat);
    gchar *my_long = g_strdup_printf("%lf", _gps.lon);

    
    // Format latitude string for transmit later
    if (_aprs_transmit_compressed_posit) {    // High res version
    	// TODO - enable compressed beacon
        snprintf(my_output_lat,
            sizeof(my_output_lat),
            "%s",
            my_lat);
 
    }
    else {  // Create a low-res version of the latitude string
        long my_temp_lat;
        char temp_data[20];

        // Convert to long
        my_temp_lat = convert_lat_s2l(my_lat);

        // Convert to low-res string
        convert_lat_l2s(my_temp_lat,
            temp_data,
            sizeof(temp_data),
            CONVERT_LP_NORMAL);

        snprintf(my_output_lat,
            sizeof(my_output_lat),
            "%c%c%c%c.%c%c%c",
            temp_data[0],
            temp_data[1],
            temp_data[3],
            temp_data[4],
            temp_data[6],
            temp_data[7],
            temp_data[8]);
    }

    (void)output_lat(my_output_lat, _aprs_transmit_compressed_posit);

    // Format longitude string for transmit later
    if (_aprs_transmit_compressed_posit) {    // High res version
        snprintf(my_output_long,
            sizeof(my_output_long),
            "%s",
            my_long);
    }
    else {  // Create a low-res version of the longitude string
        long my_temp_long;
        char temp_data[20];

        // Convert to long
        my_temp_long = convert_lon_s2l(my_long);

        // Convert to low-res string
        convert_lon_l2s(my_temp_long,
            temp_data,
            sizeof(temp_data),
            CONVERT_LP_NORMAL);

        snprintf(my_output_long,
            sizeof(my_output_long),
            "%c%c%c%c%c.%c%c%c",
            temp_data[0],
            temp_data[1],
            temp_data[2],
            temp_data[4],
            temp_data[5],
            temp_data[7],
            temp_data[8],
            temp_data[9]);
    }

    (void)output_long(my_output_long, _aprs_transmit_compressed_posit);

}


void output_my_aprs_data_tty() {
//TODO
	return ;

	gchar my_output_lat[MAX_LAT];
    gchar my_output_long[MAX_LONG];
//    gchar header_txt[MAX_LINE_SIZE+5];
//    gchar header_txt_save[MAX_LINE_SIZE+5];
    gchar path_txt[MAX_LINE_SIZE+5];
    gchar data_txt[MAX_LINE_SIZE+5];    
    gchar temp[MAX_LINE_SIZE+5];
    gchar *unproto_path = "";
    gchar data_txt2[5];
    struct tm *day_time;
    gchar my_pos[256];
    gchar output_net[256];
    gchar output_phg[10];
    gchar output_cs[10];
    gchar output_alt[20];
    gchar output_brk[3];
    int ok;
    gchar my_comment_tx[APRS_MAX_COMMENT+1];
    int interfaces_ok_for_transmit = 0;
    gchar my_phg[10];

    time_t sec;
    gchar header_txt[MAX_LINE_SIZE+5];
    gchar header_txt_save[MAX_LINE_SIZE+5];

    gchar data_txt_save[MAX_LINE_SIZE+5];


    if (!(port_data.status == DEVICE_UP 
    		&& _aprs_tty_enable && _aprs_enable && _aprs_enable_tty_tx)) return ;

    header_txt_save[0] = '\0';
    data_txt_save[0] = '\0';
    
    sec = sec_now();
    
    my_phg[0] = '\0';
	
    create_output_lat_long(my_output_lat, my_output_long);
	

    // First send any header/path info we might need out the port,
    // set up TNC's to the proper mode, etc.
    ok = 1;

    // clear this for a TNC 
    output_net[0] = '\0';

    // Set my call sign 
    // The leading \r is sent to normal serial TNCs.  The only
    // reason for it is that some folks' D700s are getting 
    // garbage in the input buffer, and the result is the mycall
    // line is rejected.  The \r at the beginning clears out the 
    // junk and lets it go through.  But data_out_ax25 tries to 
    // parse the MYCALL line, and the presence of a leading \r 
    // breaks it.
    snprintf(header_txt,
        sizeof(header_txt),
        "%c%s %s\r",
        '\3',
        ((port_data.device_type !=DEVICE_AX25_TNC)?
                    "\rMYCALL":"MYCALL"),
                    _aprs_mycall);

    // Send the callsign out to the TNC only if the interface is up and tx is enabled???
    // We don't set it this way for KISS TNC interfaces.
    if ( (port_data.device_type != DEVICE_SERIAL_KISS_TNC)
            && (port_data.device_type != DEVICE_SERIAL_MKISS_TNC)
            && (port_data.status == DEVICE_UP)
            //&& (devices.transmit_data == 1)
            //&& !transmit_disable
            //&& !posit_tx_disable
            ) {
        port_write_string(header_txt, APRS_PORT_TTY);
//send_line(gchar* text, gint text_len, TAprsPort port)
    }

    // Set unproto path:  Get next unproto path in
    // sequence.
    
    snprintf(header_txt,
            sizeof(header_txt),
            "%c%s %s VIA %s\r",
            '\3',
            "UNPROTO",
            VERSIONFRM,
            _aprs_unproto_path);

    snprintf(header_txt_save,
            sizeof(header_txt_save),
            "%s>%s,%s:",
            _aprs_mycall,
            VERSIONFRM,
            _aprs_unproto_path);

    snprintf(path_txt,
            sizeof(path_txt),
            "%s",
            _aprs_unproto_path);


    // Send the header data to the TNC.  This sets the
    // unproto path that'll be used by the next packet.
    // We don't set it this way for KISS TNC interfaces.
    if ( (port_data.device_type != DEVICE_SERIAL_KISS_TNC)
            && (port_data.device_type != DEVICE_SERIAL_MKISS_TNC)
            && (port_data.status == DEVICE_UP)
            //&& (devices.transmit_data == 1)
            //&& !transmit_disable
            //&& !posit_tx_disable
            ) {
        port_write_string(header_txt, APRS_PORT_TTY);
    }


    // Set converse mode.  We don't need to do this for
    // KISS TNC interfaces.  One european TNC (tnc2-ui)
    // doesn't accept "conv" but does accept the 'k'
    // command.  A Kantronics KPC-2 v2.71 TNC accepts
    // the "conv" command but not the 'k' command.
    // Figures!
    // 
    snprintf(header_txt, sizeof(header_txt), "%c%s\r", '\3', APRS_CONVERSE_MODE);
 
        if ( (port_data.device_type != DEVICE_SERIAL_KISS_TNC)
                && (port_data.device_type != DEVICE_SERIAL_MKISS_TNC)
                && (port_data.status == DEVICE_UP)
                //&& (devices.transmit_data == 1)
                //&& !transmit_disable
            //&& !posit_tx_disable
        ) {
        port_write_string(header_txt, APRS_PORT_TTY);
    }
    // sleep(1);



    // Set up some more strings for later transmission

    // send station info 
    output_cs[0] = '\0';
    output_phg[0] = '\0';
    output_alt[0] = '\0';
    output_brk[0] = '\0';


    if (_aprs_transmit_compressed_posit)
    {
    	// TOOD - enable compressed beacon support

//    	snprintf(my_pos,
//            sizeof(my_pos),
//            "%s",
//            compress_posit(my_output_lat,
//                my_group,
//                my_output_long,
//                my_symbol,
//                my_last_course,
//                my_last_speed,  // In knots
//                my_phg));

    }
    else { // standard non compressed mode 
        snprintf(my_pos,
            sizeof(my_pos),
            "%s%c%s%c",
            my_output_lat,
            _aprs_beacon_group,
            my_output_long,
            _aprs_beacon_symbol);
        // get PHG, if used for output 
        if (strlen(my_phg) >= 6)
            snprintf(output_phg,
                sizeof(output_phg),
                "%s",
                my_phg);

        // get CSE/SPD, Always needed for output even if 0 
        snprintf(output_cs,
            sizeof(output_cs),
            "%03d/%03d/",
            _gps.heading,
            _gps.speed);    // Speed in knots

        // get altitude 
// TODO
//        if (my_last_altitude_time > 0)
//            snprintf(output_alt,
//                sizeof(output_alt),
//                "A=%06ld/",
//                 my_last_altitude);
    }


    // APRS_MOBILE LOCAL TIME 

// TODO
//    if((strlen(output_cs) < 8) && (my_last_altitude_time > 0)) {
//        xastir_snprintf(output_brk,
//            sizeof(output_brk),
//            "/");
//    }

    day_time = localtime(&sec);

    snprintf(data_txt_save,
        sizeof(data_txt_save),
        "@%02d%02d%02d/%s%s%s%s%s",
        day_time->tm_mday,
        day_time->tm_hour,
        day_time->tm_min,
        my_pos,
        output_cs,
        output_brk,
        output_alt,
        my_comment_tx);

//WE7U2:
    // Truncate at max length for this type of APRS
    // packet.
    if (_aprs_transmit_compressed_posit) {
        if (strlen(data_txt_save) > 61) {
            data_txt_save[61] = '\0';
        }
    }
    else { // Uncompressed lat/long
        if (strlen(data_txt_save) > 70) {
            data_txt_save[70] = '\0';
        }
    }

    // Add '\r' onto end.
    strncat(data_txt_save, "\r", 1);

    snprintf(data_txt,
        sizeof(data_txt),
        "%s%s",
        output_net,
        data_txt_save);


    if (ok) {
        // Here's where the actual transmit of the posit occurs.  The
        // transmit string has been set up in "data_txt" by this point.

        // If transmit or posits have been turned off, don't transmit posit
        if ( (port_data.status == DEVICE_UP)
//                && (devices.transmit_data == 1)
//                    && !transmit_disable
//                    && !posit_tx_disable
                ) {

            interfaces_ok_for_transmit++;

// WE7U:  Change so that path is passed as well for KISS TNC
// interfaces:  header_txt_save would probably be the one to pass,
// or create a new string just for KISS TNC's.

            if ( (port_data.device_type == DEVICE_SERIAL_KISS_TNC)
                    || (port_data.device_type == DEVICE_SERIAL_MKISS_TNC) ) {

                // Note:  This one has callsign & destination in the string

                // Transmit the posit out the KISS interface
                send_ax25_frame(APRS_PORT_TTY, 
                				_aprs_mycall,    // source
                                VERSIONFRM,     // destination
                                path_txt,       // path
                                data_txt);      // data
            }
            else {  // Not a Serial KISS TNC interface


                port_write_string(data_txt, APRS_PORT_TTY);  // Transmit the posit
            }


            // Put our transmitted packet into the Incoming Data
            // window as well.  This way we can see both sides of a
            // conversation.  data_port == -1 for x_spider port,
            // normal interface number otherwise.  -99 to get a "**"
            // display meaning all ports.
            //
            // For packets that we're igating we end up with a CR or
            // LF on the end of them.  Remove that so the display
            // looks nice.
            //snprintf(temp,
            //    sizeof(temp),
            //    "%s>%s,%s:%s",
            //    my_callsign,
            //    VERSIONFRM,
            //    unproto_path,
            //    data_txt);
            //makePrintable(temp);
            //packet_data_add("TX ", temp, port);

        }
    } // End of posit transmit: "if (ok)"
}
*/

void create_output_pos_packet(TAprsPort port, gchar **packet, int *length)
{
	
//	gchar encodedPos[MAX_LINE_SIZE];
	
	if(_aprs_transmit_compressed_posit)
	{
		// TODO
	}
	else
	{
		//!5122.09N/00008.42W&APRS4R IGATE RUNNING ON NSLU2
		
		// For now just use a simple packet
		
		gchar slat[10];
		gchar slon[10];
		
		gdouble pos = (_gps.lat > 0 ? _gps.lat : 0-_gps.lat);
		
		gdouble min = (pos - (int)pos)*60.0;
		sprintf(slat, "%02d%02d.%02.0f", (int)pos, (int)min,
		                    ((min - (int)min)*100.0) );
		            
		pos = (_gps.lon > 0 ? _gps.lon : 0-_gps.lon);
		
		min = (pos - (int)pos)*60.0;
		sprintf(slon, "%03d%02d.%02.0f", (int)pos, (int)min,
				                    ((min - (int)min)*100.0) );
		
		*packet = g_strdup_printf(
			"%c%s%c%c%s%c%c%s%c",
			'=',
			slat, 
			(_gps.lat > 0 ? 'N' : 'S'),
			_aprs_beacon_group,
			slon,
			(_gps.lon > 0 ? 'E' : 'W'),
			_aprs_beacon_symbol,
			(port == APRS_PORT_INET ? _aprs_inet_beacon_comment : _aprs_beacon_comment),
			(char)0
			);
	}
	
	*length = strlen(*packet);
}


void send_packet(TAprsPort port, gchar* to_call, gchar* path, gchar* packet, gint packet_length)
{
	if(port == APRS_PORT_INET 
			|| !(port_data.device_type == DEVICE_SERIAL_KISS_TNC
	        || port_data.device_type == DEVICE_SERIAL_MKISS_TNC) )
	{
		gchar *packet_header
			=  g_strdup_printf(
	        "%s>%s,%s:",
	        _aprs_mycall,
	        to_call,
	        path);
		
		gchar *full_packet = g_strdup_printf("%s%s\r\n", packet_header, packet);
		
		
		send_line(full_packet, strlen(packet_header)+packet_length+2,  port);
	}
	else
	{
		send_ax25_frame(port, _aprs_mycall, to_call, path, packet);
	}
	
}

void output_my_aprs_data(TAprsPort port) {
	

	gchar *packet;
	int length = 0;
	
	
    //create_output_lat_long(my_output_lat, my_output_long);
	create_output_pos_packet(port, &packet, &length);

	
	send_packet(port, VERSIONFRM, _aprs_unproto_path, packet, length);
	
	if(packet != NULL) g_free(packet);
}

/////////// TTY functionality

int serial_init();
int _aprs_tnc_retry_count = 0;

static gboolean aprs_tnc_handle_error_idle (gchar *error)
{
	printf("%s(%s)\n", __PRETTY_FUNCTION__, error);
	
	/* Ask for re-try. */
    if(++_aprs_tnc_retry_count > 2)
    {
        GtkWidget *confirm;
        gchar buffer[BUFFER_SIZE];

        /* Reset retry count. */
        _aprs_tnc_retry_count = 0;

        snprintf(buffer, sizeof(buffer), "%s\nRetry?", error);
        confirm = hildon_note_new_confirmation(GTK_WINDOW(_window), buffer);

        aprs_tty_disconnect();

        if(GTK_RESPONSE_OK == gtk_dialog_run(GTK_DIALOG(confirm)))
            aprs_tty_connect(); /* Try again. */
        else
        {
            /* Disable GPS. */
            gtk_check_menu_item_set_active(
                    GTK_CHECK_MENU_ITEM(_menu_enable_aprs_tty_item), FALSE);
        }

        /* Ask user to re-connect. */
        gtk_widget_destroy(confirm);
    }
    else
    {
        aprs_tty_disconnect();
        aprs_tty_connect(); /* Try again. */
    }

    g_free(error);

    vprintf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
    return FALSE;

}

void close_tnc_port();

static void thread_read_tty()
{
//	gint max_retries = 5;
	GThread *my_thread = g_thread_self();
	
	//fprintf(stderr, "in thread_read_tty\n");
	
	if(/*max_retries>0 &&*/ _aprs_tty_thread == my_thread )
	{
		if( serial_init() >= 0 )
		{
			// Success
			//fprintf(stderr, "TTY port open \n");
			port_read();
			
			if(_aprs_tty_thread == my_thread)
			{
	            g_idle_add((GSourceFunc)aprs_tnc_handle_error_idle,
	                    g_strdup_printf("%s",
	                    _("Error reading data from TNC Port.")));

			}
		}
		else
		{
			// Failed
			fprintf(stderr, "Failed to init serial port\n");
			
            g_idle_add((GSourceFunc)aprs_tnc_handle_error_idle,
                    g_strdup_printf("%s",
                    _("Error connecting to TNC Port.")));

		}

		close_tnc_port();
//		max_retries--;
//		sleep(50);
	}
	
//	if(max_retries==0)
//	{
		// Failed
//		set_aprs_tty_conn_state(RCVR_OFF);
//		MACRO_BANNER_SHOW_INFO(_window, _("Failed to connect to TNC!")); \
//	}

}

gboolean aprs_tty_connect()
{
    printf("%s(%d)\n", __PRETTY_FUNCTION__, _aprs_tty_state);

    if(_aprs_tty_enable && _aprs_tty_state == RCVR_OFF)
    {
        set_aprs_tty_conn_state(RCVR_DOWN);

        // Lock/Unlock the mutex to ensure that the thread doesn't
        // start until _gps_thread is set. 
        g_mutex_lock(_aprs_tty_init_mutex);

        _aprs_tty_thread = g_thread_create((GThreadFunc)thread_read_tty,
                NULL, TRUE, NULL); // Joinable. 

        g_mutex_unlock(_aprs_tty_init_mutex);
    }

    vprintf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
    return FALSE;

}

void aprs_tty_disconnect()
{
    gboolean exit_now = FALSE;

    printf("%s()\n", __PRETTY_FUNCTION__);

    GThread *my_thread = g_thread_self();

    if(my_thread == _aprs_tty_thread)
    {
        exit_now = TRUE;
        close_tnc_port();
    }

    _aprs_tty_thread = NULL;

    if(_window)
        set_aprs_tty_conn_state(RCVR_OFF);
    vprintf("%s(): return\n", __PRETTY_FUNCTION__);

    if(exit_now) exit(0);

}


//***********************************************************
// port_write_string()
//
// port is port# used
// data is the string to write
//***********************************************************

void port_write_string(gchar *data, gint len, TAprsPort port) {
//    int i,erd, 
    int retval;
//    int write_in_pos_hold;

    if (data == NULL)
        return;

    if (data[0] == '\0')
        return;
    
    
    
    if(port == APRS_PORT_TTY)
    {
    
    	if(g_mutex_trylock (_write_buffer[port].write_lock))
    	{
    		//fprintf(stderr, "TTY Write... ");
		    retval = (int)write(port_data.channel,
		        data,
		        len);
		    //fprintf(stderr, "done... ");
		    g_mutex_unlock (_write_buffer[port].write_lock);
		    //fprintf(stderr, "Unlocked\n");
    	}
    	else
    		fprintf(stderr, "Failed to get lock\n");
    }
    else
    {
    	send_line(data, len, port);
    }
}


gboolean send_line(gchar* text, gint text_len, TAprsPort port)
{
	if(APRS_PORT_INET == port && !_aprs_enable_inet_tx) return FALSE;
	else if (APRS_PORT_TTY == port && !_aprs_enable_tty_tx) return FALSE;
	
	if(APRS_PORT_TTY == port)
	{
	
	}
	gboolean error = FALSE;
	gint i;
    gint write_in_pos_hold = _write_buffer[port].write_in_pos;
    
    // Lock the mutex 
    g_mutex_lock(_write_buffer[port].write_lock);
    
    for (i = 0; i < text_len && !error; i++) {
    	_write_buffer[port].device_write_buffer[_write_buffer[port].write_in_pos++] 
    	                                        = text[i];
    	
        if (_write_buffer[port].write_in_pos >= MAX_DEVICE_BUFFER)
        	_write_buffer[port].write_in_pos = 0;

        if (_write_buffer[port].write_in_pos == _write_buffer[port].write_out_pos) {
            fprintf(stderr,"Port %d Buffer overrun\n",port);

            /* clear this restore original write_in pos and dump this string */
            _write_buffer[port].write_in_pos = write_in_pos_hold;
            _write_buffer[port].errors++;
            error = TRUE;
        }
    }
	
    g_mutex_unlock(_write_buffer[port].write_lock);
    
    return error;
}


//***********************************************************
// port_read()
//
//
// This function becomes the long-running thread that snags
// characters from an interface and passes them off to the
// decoding routines.  One copy of this is run for each read
// thread for each interface.
//***********************************************************
gboolean read_port_data();

void port_read() {
//    unsigned char cin, last;
//    gint i;
    struct timeval tmv;
    fd_set rd;

//    cin = (unsigned char)0;
//    last = (unsigned char)0;
    gboolean success = TRUE;
    GThread *my_thread = g_thread_self();

    fprintf(stderr, "Enter port_read\n");
    
    // We stay in this read loop until the port is shut down
    while(port_data.active == DEVICE_IN_USE && _aprs_tty_thread == my_thread
    		&& RCVR_UP == _aprs_tty_state && success == TRUE){

        if (port_data.status == DEVICE_UP){

            port_data.read_in_pos = 0;
            port_data.scan = 1;
            
            
            while (port_data.scan >= 0
            		&& success == TRUE
            		//&& RCVR_UP == _aprs_tty_state
                    && (port_data.read_in_pos < (MAX_DEVICE_BUFFER - 1) )
                    && (port_data.status == DEVICE_UP)
                    && (_aprs_tty_thread == my_thread) 
                ) 
            {
    
                success = read_port_data();
            }
            
        }
        if (port_data.active == DEVICE_IN_USE)  {

            
        	
        	// We need to delay here so that the thread doesn't use
            // high amounts of CPU doing nothing.

// This select that waits on data and a timeout, so that if data
// doesn't come in within a certain period of time, we wake up to
// check whether the socket has gone down.  Else, we go back into
// the select to wait for more data or a timeout.  FreeBSD has a
// problem if this is less than 1ms.  Linux works ok down to 100us.
// We don't need it anywhere near that short though.  We just need
// to check whether the main thread has requested the interface be
// closed, and so need to have this short enough to have reasonable
// response time to the user.

//sched_yield();  // Yield to other threads

            // Set up the select to block until data ready or 100ms
            // timeout, whichever occurs first.
            FD_ZERO(&rd);
            FD_SET(port_data.channel, &rd);
            tmv.tv_sec = 0;
            tmv.tv_usec = 100000;    // 100 ms
            (void)select(0,&rd,NULL,NULL,&tmv);
        }
    }

    fprintf(stderr, "End of port_read\n");
}


gboolean aprs_send_beacon_inet()
{

	aprs_send_beacon(APRS_PORT_INET);

	return TRUE;
}

gboolean aprs_send_beacon(TAprsPort port)
{
	if(_aprs_enable)
	{
		output_my_aprs_data(port);
		
		//fprintf(stderr, "Beacon sent\n" );
	}
	
	return TRUE;
}


gboolean timer_callback_aprs_inet (gpointer data) {

	if(_aprs_inet_enable && _aprs_enable_inet_tx && _aprs_inet_beacon_interval>0)
	{
		aprs_send_beacon(APRS_PORT_INET);
		return TRUE; // Continue timer
	}
	
	return FALSE; // Stop timer
}



gboolean timer_callback_aprs_tty (gpointer data) {

	if(_aprs_tty_enable && _aprs_enable_tty_tx && _aprs_tty_beacon_interval>0)
	{
		fprintf(stderr, "Sending beacon for TNC...\n");
		aprs_send_beacon(APRS_PORT_TTY);
		return TRUE; // Continue timer
	}
	
	return FALSE; // Stop timer
}


void aprs_timer_init()
{
	// disable timer if exists
	if(_aprs_inet_beacon_timer>0) g_source_remove(_aprs_inet_beacon_timer);
	if(_aprs_tty_beacon_timer>0) g_source_remove(_aprs_tty_beacon_timer);
	
	if(_aprs_enable)
	{
		if(_aprs_inet_enable && _aprs_enable_inet_tx && _aprs_inet_beacon_interval>0)
			_aprs_inet_beacon_timer = g_timeout_add(_aprs_inet_beacon_interval*1000 , timer_callback_aprs_inet, NULL);
		
		if(_aprs_tty_enable && _aprs_enable_tty_tx && _aprs_tty_beacon_interval>0)
			_aprs_tty_beacon_timer = g_timeout_add(_aprs_tty_beacon_interval*1000 , timer_callback_aprs_tty, NULL);
	}
}

#endif //INCLUDE_APRS
