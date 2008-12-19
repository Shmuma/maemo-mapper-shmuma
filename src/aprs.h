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

#ifndef MAEMO_MAPPER_APRS_H
#define MAEMO_MAPPER_APRS_H

#include "types.h"
#include <termios.h>

#define MAX_LINE_SIZE 			512
#define MAX_STATUS_LINES 		20
#define MAX_COMMENT_LINES 		20
#define EARTH_RADIUS_METERS     6378138.0
#define MAX_TACTICAL_CALL 		20
#define TACTICAL_HASH_SIZE 		1024


#define START_STR " }"
#define LBRACE '{'
#define RBRACE '}'



#define MAX_DEVICE_NAME 128
#define MAX_DEVICE_BUFFER_UNTIL_BINARY_SWITCH 700
#define MAX_DEVICE_HOSTNM 40
#define MAX_DEVICE_HOSTPW 40

// KISS Protocol Special Characters & Commands:
#define KISS_FEND           0xc0  // Frame End
#define KISS_FESC           0xdb  // Frame Escape
#define KISS_TFEND          0xdc  // Transposed Frame End
#define KISS_TFESC          0xdd  // Transposed Frame Escape
#define KISS_DATA           0x00
#define KISS_TXDELAY        0x01
#define KISS_PERSISTENCE    0x02
#define KISS_SLOTTIME       0x03
#define KISS_TXTAIL         0x04
#define KISS_FULLDUPLEX     0x05
#define KISS_SETHARDWARE    0x06
#define KISS_RETURN         0xff


enum Device_Types {
    DEVICE_NONE,
    DEVICE_SERIAL_TNC,
    DEVICE_NET_STREAM,
    DEVICE_AX25_TNC,
    DEVICE_SERIAL_KISS_TNC,     // KISS TNC on serial port (not ax.25 kernel device)
    DEVICE_SERIAL_MKISS_TNC     // Multi-port KISS TNC, like the Kantronics KAM
};

enum Device_Active {
    DEVICE_NOT_IN_USE,
    DEVICE_IN_USE
};

enum Device_Status {
    DEVICE_DOWN,
    DEVICE_UP,
    DEVICE_ERROR
};




typedef struct {
    int    device_type;                           /* device type                             */
    int    active;                                /* channel in use                          */
    int    status;                                /* current status (up or down)             */
    char   device_name[MAX_DEVICE_NAME+1];        /* device name                             */
    char   device_host_name[MAX_DEVICE_HOSTNM+1]; /* device host name for network            */
    unsigned long int address;                    /* socket address for network              */
    int    thread_status;                         /* thread status for connect thread        */
    int    connect_status;                        /* connect status for connect thread       */
    int    decode_errors;                         /* decode error count, used for data type  */
    int    data_type;                             /* 0=normal 1=wx_binary                    */
    int    socket_port;                           /* socket port# for network                */
    char   device_host_pswd[MAX_DEVICE_HOSTPW+1]; /* host password                           */
    int    channel;                               /* for serial and net ports                */
    int    channel2;                              /* for AX25 ports                          */
    char   ui_call[30];                           /* current call for this port              */
    struct termios t,t_old;                       /* terminal struct for serial port         */
    int    dtr;                                   /* dtr signal for HSP cable (status)       */
    int    sp;                                    /* serial port speed                       */
    int    style;                                 /* serial port style                       */
    int    scan;                                  /* data read available                     */
    int    errors;                                /* errors for this port                    */
    int    reconnect;                             /* reconnect on net failure                */
    int    reconnects;                            /* total number of reconnects by this port */
    unsigned long   bytes_input;                  /* total bytes read by this port           */
    unsigned long   bytes_output;                 /* total bytes written by this port        */
    unsigned long   bytes_input_last;             /* total bytes read last check             */
    unsigned long   bytes_output_last;            /* total bytes read last check             */
    int    port_activity;                         /* 0 if no activity between checks         */
    pthread_t read_thread;                        /* read thread                             */
    int    read_in_pos;                           /* current read buffer input pos           */
    int    read_out_pos;                          /* current read buffer output pos          */
    char   device_read_buffer[MAX_DEVICE_BUFFER]; /* read buffer for this port               */
    pthread_mutex_t read_lock;                       /* Lock for reading the port data          */
    pthread_t write_thread;                       /* write thread                            */
    int    write_in_pos;                          /* current write buffer input pos          */
    int    write_out_pos;                         /* current write buffer output pos         */
    pthread_mutex_t write_lock;                      /* Lock for writing the port data          */
    char   device_write_buffer[MAX_DEVICE_BUFFER];/* write buffer for this port              */
} iface;

extern iface port_data;     // shared port data
extern TWriteBuffer _write_buffer[APRS_PORT_COUNT];

// Incoming data queue
typedef struct _incoming_data_record {
    int length;   // Used for binary strings such as KISS
    int port;
    unsigned char data[MAX_LINE_SIZE];
} incoming_data_record;

gboolean aprs_server_connect(void); // Called from menu.c 
void aprs_server_disconnect(void);  // Called from menu.c

void aprs_init(void);				// Called form main.c

void map_render_aprs();
double distance_from_my_station(char *call_sign, char *course_deg, gint course_len);

void pad_callsign(char *callsignout, char *callsignin);
gboolean aprs_send_beacon(TAprsPort port);
gboolean aprs_send_beacon_inet();
void update_aprs_inet_options(gboolean force);
void port_write_string(gchar *data, gint len, TAprsPort port);

extern AprsDataRow *station_shortcuts[16384];

#endif /* ifndef MAEMO_MAPPER_APRS_H */

#endif // INCLUDE_APRS
