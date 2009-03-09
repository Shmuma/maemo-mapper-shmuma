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

#include "aprs_kiss.h"
#include "aprs.h"
#include "defines.h"
#include "data.h"
#include <sys/types.h>
#include <sys/ioctl.h>

#include <pthread.h>


#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <unistd.h>
#include <dirent.h>
#include <signal.h>
#include <termios.h>
#include <pwd.h>
#include <termios.h>
#include <setjmp.h>
#include <sys/socket.h>
#include <fcntl.h>

#include <netinet/in.h>     // Moved ahead of inet.h as reports of some *BSD's not
                            // including this as they should.


#define _GNU_SOURCE

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <time.h>

#define DBUS_API_SUBJECT_TO_CHANGE
#include <dbus/dbus-glib.h>


#define DISABLE_SETUID_PRIVILEGE do { \
seteuid(getuid()); \
setegid(getgid()); \
} while(0)
#define ENABLE_SETUID_PRIVILEGE do { \
seteuid(euid); \
setegid(egid); \
} while(0)




#define MAX_INPUT_QUEUE 1000
gint decode_ax25_line(gchar *line, TAprsPort port);

typedef struct
{
    pthread_mutex_t lock;
    pthread_t threadID;
} xastir_mutex;

xastir_mutex connect_lock;              // Protects port_data[].thread_status and port_data[].connect_status

// Read/write pointers for the circular input queue
/*
static int incoming_read_ptr = 0;
static int incoming_write_ptr = 0;
static int queue_depth = 0;
static int push_count = 0;
static int pop_count = 0;
*/
uid_t euid;
gid_t egid;



iface port_data;     // shared port data


//WE7U2
// We feed a raw 7-byte string into this routine.  It decodes the
// callsign-SSID and tells us whether there are more callsigns after
// this.  If the "asterisk" input parameter is nonzero it'll add an
// asterisk to the callsign if it has been digipeated.  This
// function is called by the decode_ax25_header() function.
//
// Inputs:  string          Raw input string
//          asterisk        1 = add "digipeated" asterisk
//
// Outputs: callsign        Processed string
//          returned int    1=more callsigns follow, 0=end of address field
//
gint decode_ax25_address(gchar *string, gchar *callsign, gint asterisk) {
    gint i,j;
    gchar ssid;
    gchar t;
    gint more = 0;
    gint digipeated = 0;

    // Shift each of the six callsign characters right one bit to
    // convert to ASCII.  We also get rid of the extra spaces here.
    j = 0;
    for (i = 0; i < 6; i++) {
        t = ((unsigned char)string[i] >> 1) & 0x7f;
        if (t != ' ') {
            callsign[j++] = t;
        }
    }

    // Snag out the SSID byte to play with.  We need more than just
    // the 4 SSID bits out of it.
    ssid = (unsigned char)string[6];

    // Check the digipeat bit
    if ( (ssid & 0x80) && asterisk)
        digipeated++;   // Has been digipeated

    // Check whether it is the end of the address field
    if ( !(ssid & 0x01) )
        more++; // More callsigns to come after this one

    // Snag the four SSID bits
    ssid = (ssid >> 1) & 0x0f;

    // Construct the SSID number and add it to the end of the
    // callsign if non-zero.  If it's zero we don't add it.
    if (ssid) {
        callsign[j++] = '-';
        if (ssid > 9) {
            callsign[j++] = '1';
        }
        ssid = ssid % 10;
        callsign[j++] = '0' + ssid;
    }

    // Add an asterisk if the packet has been digipeated through
    // this callsign
    if (digipeated)
        callsign[j++] = '*';

    // Terminate the string
    callsign[j] = '\0';

    return(more);
}


// Function which receives raw AX.25 packets from a KISS interface and
// converts them to a printable TAPR-2 (more or less) style string.
// We receive the packet with a KISS Frame End character at the
// beginning and a "\0" character at the end.  We can end up with
// multiple asterisks, one for each callsign that the packet was
// digipeated through.  A few other TNC's put out this same sort of
// format.
//
// Note about KISS & CRC's:  The TNC checks the CRC.  If bad, it
// drops the packet.  If good, it sends it to the computer WITHOUT
// the CRC bytes.  There's no way at the computer end to check
// whether the packet was corrupted over the serial channel between
// the TNC and the computer.  Upon sending a KISS packet to the TNC,
// the TNC itself adds the CRC bytes back on before sending it over
// the air.  In Xastir we can just assume that we're getting
// error-free packets from the TNC, ignoring possible corruption
// over the serial line.
//
// Some versions of KISS can encode the radio channel (for
// multi-port TNC's) in the command byte.  How do we know we're
// running those versions of KISS though?  Here are the KISS
// variants that I've been able to discover to date:
//
// KISS               No CRC, one radio port
//
// SMACK              16-bit CRC, multiport TNC's
//
// KISS-CRC
//
// 6-PACK
//
// KISS Multi-drop (Kantronics) 8-bit XOR Checksum, multiport TNC's (AGWPE compatible)
// BPQKISS (Multi-drop)         8-bit XOR Checksum, multiport TNC's
// XKISS (Kantronics)           8-bit XOR Checksum, multiport TNC's
//
// JKISS              (AGWPE and BPQ32 compatible)
//
// MKISS              Linux driver which supports KISS/BPQ and
//                    hardware handshaking?  Also Paccomm command to
//                    immediately enter KISS mode.
//
// FlexKISS           -,
// FlexCRC            -|-- These are all the same!
// RMNC-KISS          -|
// CRC-RMNC           -'
//
//
// It appears that none of the above protocols implement any form of
// hardware flow control.
// 
// 
// Compare this function with interface.c:process_ax25_packet() to
// see if we're missing anything important.
//
//
// Inputs:  data_string         Raw string (must be MAX_LINE_SIZE or bigger)
//          length              Length of raw string (may get changed here)
//
// Outputs: int                 0 if it is a bad packet,
//                              1 if it is good
//          data_string         Processed string
//
gint decode_ax25_header(
		unsigned char *data_string, 
		gint *length) {
    gchar temp[20];
    gchar result[MAX_LINE_SIZE+100];
    gchar dest[15];
    gint i, ptr;
    gchar callsign[15];
    gchar more;
    gchar num_digis = 0;


    // Do we have a string at all?
    if (data_string == NULL)
        return(0);

    // Drop the packet if it is too long.  Note that for KISS packets
    // we can't use strlen() as there can be 0x00 bytes in the
    // data itself.
    if (*length > 1024) {
        data_string[0] = '\0';
        *length = 0;
        return(0);
    }

    // Start with an empty string for the result
    result[0] = '\0';

    ptr = 0;

    // Process the destination address
    for (i = 0; i < 7; i++)
        temp[i] = data_string[ptr++];
    temp[7] = '\0';
    more = decode_ax25_address(temp, callsign, 0); // No asterisk
    snprintf(dest,sizeof(dest),"%s",callsign);

    // Process the source address
    for (i = 0; i < 7; i++)
        temp[i] = data_string[ptr++];
    temp[7] = '\0';
    more = decode_ax25_address(temp, callsign, 0); // No asterisk

    // Store the two callsigns we have into "result" in the correct
    // order
    snprintf(result,sizeof(result),"%s>%s",callsign,dest);

    // Process the digipeater addresses (if any)
    num_digis = 0;
    while (more && num_digis < 8) {
        for (i = 0; i < 7; i++)
            temp[i] = data_string[ptr++];
        temp[7] = '\0';

        more = decode_ax25_address(temp, callsign, 1); // Add asterisk
        strncat(result,
            ",",
            sizeof(result) - strlen(result));
        
        strncat(result,
            callsign,
            sizeof(result) - strlen(result));
        num_digis++;
    }

    strncat(result,
        ":",
        sizeof(result) - strlen(result));


    // Check the Control and PID bytes and toss packets that are
    // AX.25 connect/disconnect or information packets.  We only
    // want to process UI packets in Xastir.


    // Control byte should be 0x03 (UI Frame).  Strip the poll-bit
    // from the PID byte before doing the comparison.
    if ( (data_string[ptr++] & (~0x10)) != 0x03) {
        return(0);
    }


    // PID byte should be 0xf0 (normal AX.25 text)
    if (data_string[ptr++] != 0xf0)
        return(0);


// WE7U:  We get multiple concatenated KISS packets sometimes.  Look
// for that here and flag when it happens (so we know about it and
// can fix it someplace earlier in the process).  Correct the
// current packet so we don't get the extra garbage tacked onto the
// end.
    for (i = ptr; i < *length; i++) {
        if (data_string[i] == KISS_FEND) {
            fprintf(stderr,"***Found concatenated KISS packets:***\n");
            data_string[i] = '\0';    // Truncate the string
            break;
        }
    }

    // Add the Info field to the decoded header info
    strncat(result,
        (char *)(&data_string[ptr]),
        sizeof(result) - strlen(result));

    // Copy the result onto the top of the input data.  Note that
    // the length can sometimes be longer than the input string, so
    // we can't just use the "length" variable here or we'll
    // truncate our string.  Make sure the data_string variable is
    // MAX_LINE_SIZE or bigger.
    //
    snprintf((char *)data_string,
        MAX_LINE_SIZE,
        "%s",
        result);

    // Write out the new length
    *length = strlen(result); 

//fprintf(stderr,"%s\n",data_string);

    return(1);
}

















// Added by KB6MER for KAM XL(SERIAL_TNC_AUX_GPS) support
// buf is a null terminated string
// returns buf as a null terminated string after cleaning.
// Currently:
//    removes leading 'cmd:' prompts from TNC if needed
// Can be used to add any additional data cleaning functions desired.
// Currently only called for SERIAL_TNC_AUX_GPS, but could be added
// to other device routines to improve packet decode on other devices.
//
// Note that the length of "buf" can be up to MAX_DEVICE_BUFFER,
// which is currently set to 4096.
//
void tnc_data_clean(gchar *buf) {

    while (!strncmp(buf,"cmd:",4)) {
        int ii;

        // We're _shortening_ the string here, so we don't need to
        // know the length of the buffer unless it has no '\0'
        // terminator to begin with!  In that one case we could run
        // off the end of the string and get a segfault or cause
        // other problems.
        for (ii = 0; ; ii++) {
            buf[ii] = buf[ii+4];
            if (buf[ii] == '\0')
                break;
        }
    }
}



static gboolean aprs_parse_tty_packet(gchar *packet)
{
    decode_ax25_line(packet, APRS_PORT_TTY);

    g_free(packet);

    return FALSE;
}


static gboolean kiss_parse_packet(unsigned char *data_string, gint data_length)
{
	
	
	//fprintf(stderr, "Parse: %s\n", data_string);
	
	gint devicetype = port_data.device_type;
	
	
	switch(devicetype)
	{
    case DEVICE_SERIAL_KISS_TNC:
    case DEVICE_SERIAL_MKISS_TNC:
	    if ( !decode_ax25_header( data_string,
	            &data_length ) ) {
	        // Had a problem decoding it.  Drop
	        // it on the floor.
	        break;
	    }
	    else {
	        // Good decode.  Drop through to the
	        // next block to log and decode the
	        // packet.
	    }
	
	case DEVICE_SERIAL_TNC:
	    tnc_data_clean((char *)data_string);

	case DEVICE_AX25_TNC:	



		
//fprintf(stderr, "Decoded kiss: %s\n", data_string);
      g_idle_add((GSourceFunc)aprs_parse_tty_packet, data_string);
//		decode_ax25_line(data_string, "T", 0);
		break;
		
	default:
		break;
	}

    
    
	return FALSE; 
}

// Add one record to the circular queue.  Returns 1 if queue is
// full, 0 if successful.
//
/*
int push_incoming_data(unsigned char *data_string, int length) {
	
    int next_write_ptr = (incoming_write_ptr + 1) % MAX_INPUT_QUEUE;

    // Check whether queue is full
    if (incoming_read_ptr == next_write_ptr) {
        // Yep, it's full!
        return(1);
    }

    // Advance the write pointer
    incoming_write_ptr = next_write_ptr;

    incoming_data_queue[incoming_write_ptr].length = length;

//    incoming_data_queue[incoming_write_ptr].port = port;

    snprintf((char *)incoming_data_queue[incoming_write_ptr].data,
        (length < MAX_LINE_SIZE) ? length : MAX_LINE_SIZE,
        "%s",
        data_string);

    queue_depth++;
    push_count++;

    return(0);
}
*/









//***********************************************************
// channel_data()
//
// Takes data read in from a port and adds it to the
// incoming_data_queue.  If queue is full, waits for queue to have
// space before continuing.
//
// port #                                                    
// string is the string of data
// length is the length of the string.  If 0 then use strlen()
// on the string itself to determine the length.
//
// Note that decode_ax25_header() and perhaps other routines may
// increase the length of the string while processing.  We need to
// send a COPY of our input string off to the decoding routines for
// this reason, and the size of the buffer must be MAX_LINE_SIZE
// for this reason also.
//***********************************************************
void channel_data(unsigned char *string, int length) {
    int max;
//    struct timeval tmv;
    // Some messiness necessary because we're using xastir_mutex's
    // instead of pthread_mutex_t's.
    int process_it = 0;


    //fprintf(stderr,"channel_data: %x %d\n",string[0],length);

    
    max = 0;

    if (string == NULL)
    {
        return;
    }

    if (string[0] == '\0')
    {
        return;
    }

    if (length == 0) {
        // Compute length of string including terminator
        length = strlen((const char *)string) + 1;
    }

    // Check for excessively long packets.  These might be TCP/IP
    // packets or concatenated APRS packets.  In any case it's some
    // kind of garbage that we don't want to try to parse.

    // Note that for binary data (WX stations and KISS packets), the
    // strlen() function may not work correctly.
    if (length > MAX_LINE_SIZE) {   // Too long!
//    	fprintf(stderr, "Too long");
        string[0] = '\0';   // Truncate it to zero length
        return;
    }


    // This protects channel_data from being run by more than one
    // thread at the same time.

    if (length > 0) {


        // Install the cleanup routine for the case where this
        // thread gets killed while the mutex is locked.  The
        // cleanup routine initiates an unlock before the thread
        // dies.  We must be in deferred cancellation mode for the
        // thread to have this work properly.  We must first get the
        // pthread_mutex_t address.



        // If it's any of three types of GPS ports and is a GPRMC or
        // GPGGA string, just stick it in one of two global
        // variables for holding such strings.  UpdateTime() can
        // come along and process/clear-out those strings at the
        // gps_time interval.
        //
        process_it++;

        // Remove the cleanup routine for the case where this thread
        // gets killed while the mutex is locked.  The cleanup
        // routine initiates an unlock before the thread dies.  We
        // must be in deferred cancellation mode for the thread to
        // have this work properly.
  //      pthread_cleanup_pop(0);

 
//fprintf(stderr,"Channel data on Port [%s]\n",(char *)string);

        if (process_it) {

            // Wait for empty space in queue
//fprintf(stderr,"\n== %s", string);
        	
        	
/*
            while (push_incoming_data(string, length) && max < 5400) {
                sched_yield();  // Yield to other threads
                tmv.tv_sec = 0;
                tmv.tv_usec = 2;  // 2 usec
                (void)select(0,NULL,NULL,NULL,&tmv);
                max++;
            }
*/
        	
        	kiss_parse_packet(g_strdup(string), length);
            //g_idle_add((GSourceFunc)kiss_parse_packet, g_strdup(string));

            
        }
//        else
//        {
//        	fprintf(stderr,"Channel data on Port [%s]\n",(char *)string);
//        }
    }

}




//****************************************************************
// get device name only (the portion at the end of the full path)
// device_name current full name of device
//****************************************************************

char *get_device_name_only(char *device_name) {
    int i,len,done;

    if (device_name == NULL)
        return(NULL);

    done = 0;
    len = (int)strlen(device_name);
    for(i = len; i > 0 && !done; i--){
        if(device_name[i] == '/'){
            device_name += (i+1);
            done = 1;
        }
    }
    return(device_name);
}


int filethere(char *fn) {
    FILE *f;
    int ret;

    ret =0;
    f=fopen(fn,"r");
    if (f != NULL) {
        ret=1;
        (void)fclose(f);
    }
    return(ret);
}



/*
 * Close the serial port
 * */
gint serial_detach() {
    int ok;
    ok = -1;

    if (port_data.active == DEVICE_IN_USE && port_data.status == DEVICE_UP) 
    {
        // Close port first
        (void)tcsetattr(port_data.channel, TCSANOW, &port_data.t_old);
        if (close(port_data.channel) == 0) 
        {
            port_data.status = DEVICE_DOWN;
            usleep(200);
            port_data.active = DEVICE_NOT_IN_USE;
            ok = 1;

        }
        else 
        {
            fprintf(stderr,"Could not close port %s\n",port_data.device_name);

            port_data.status = DEVICE_DOWN;
            usleep(200);
            port_data.active = DEVICE_NOT_IN_USE;

        }

    }

    return(ok);
}


typedef struct {
  char *adapter;  /* do not free this, it is freed somewhere else */
  char *bonding;  /* allocated from heap, you must free this */
} bonding_t;

static inline DBusGConnection *get_dbus_gconn(GError **error)
{
  DBusGConnection *conn;

  conn = dbus_g_bus_get(DBUS_BUS_SYSTEM, error);
  return conn;
}



void close_tnc_port()
{

	if (port_data.device_name != NULL) {
		fprintf(stderr, "in close_tnc_port()\n");
		serial_detach();
		
	    int skip_dbus = 0;
	    DBusGConnection *bus = NULL;
	    DBusGProxy *proxy = NULL;
	    GError *error = NULL;

	    bus = get_dbus_gconn(&error);
	    if (!bus) {
	      errno = ECONNREFUSED; /* close enough :) */
	      skip_dbus = 1;
	    }


	    if (!skip_dbus) {
	        /* Disconnect the device */
	        proxy = dbus_g_proxy_new_for_name(bus,
	            BTCOND_DBUS, BTCOND_PATH, BTCOND_INTERFACE);
	        error = NULL;
	        if(!dbus_g_proxy_call(proxy, BTCOND_DISCONNECT, &error,
	              G_TYPE_STRING, port_data.device_name, G_TYPE_INVALID, G_TYPE_INVALID)
	            || error){
//	        	PDEBUG("Cannot send msg (service=%s, object=%s, interface=%s, "
//	        			"method=%s) [%s]\n",
//	                 BTCOND_DBUS,
//	                 BTCOND_PATH,
//	                 BTCOND_INTERFACE,
//	                 BTCOND_DISCONNECT,
//	                 error->message ? error->message : "<no error msg>");
	        }
	        g_object_unref(proxy);
	    }

	    free(port_data.device_name);
	    port_data.device_name[0]=0;
	    

	    if (bus) {
	      dbus_g_connection_unref(bus);
	    }
	}
}

gboolean open_bluetooth_tty_connection(gchar *bda, gchar **aprs_bt_port)
{
	gint i, st, num_bondings = 0, num_rfcomms = 0, num_posdev = 0;  
	GError *error = NULL;
	DBusGConnection *bus = NULL;
	DBusGProxy *proxy = NULL;
	gchar **rfcomms = 0;
	bonding_t *bondings = 0; /* points to array of bonding_t */
	bonding_t *posdev = 0; /* bondings with positioning bit on */
	gchar *tmp;
	const gchar const *spp="SPP";
  
	/* Use the dbus interface to get the BT information */
	

#if (__GNUC__ > 2) && ((__GNUC__ > 3) || (__GNUC_MINOR__ > 2))
#define ERRSTR(fmt, args...)                                     \
  if (error_buf && error_buf_max_len>0) {                        \
    set_error_msg(error_buf, error_buf_max_len, fmt, args);      \
  } else {                                                       \
    PDEBUG(fmt, args);                                           \
  }
#else
#define ERRSTR(fmt, args...)                                     \
  if (error_buf && error_buf_max_len>0) {                        \
    set_error_msg(error_buf, error_buf_max_len, fmt, ##args);    \
  } else {                                                       \
    PDEBUG(fmt, ##args);                                         \
  }
#endif  


	error = NULL;
	bus = dbus_g_bus_get(DBUS_BUS_SYSTEM, &error);

	if (error) 
	{
	    st = -1;
	    errno = ECONNREFUSED; /* close enough :) */
	    //ERRSTR("%s", error->message);
	    //PDEBUG("Cannot get reply message [%s]\n", error->message);
	    goto OUT;
    }

	/* We need BT information only if the caller does not specify
	 * the BT address. If address is defined, it is assumed that
	 * it is already bonded and we just create RFCOMM connection
	 * to it.
	 */
	if (!bda) 
	{
		// May want to support auto detect for serial ports?
	} else {  /* if (!bda) */
	    /* Caller supplied BT address so use it */
	    num_posdev = 1;
	    posdev = calloc(1, sizeof(bonding_t *));
	    if (!posdev) 
	    {
	      st = -1;
	      errno = ENOMEM;
	      goto OUT;
	    }
	    posdev[0].bonding = strdup(bda);

	    /* Adapter information is not needed */
	    posdev[0].adapter = "<not avail>";
	}
	
	/* For each bondend BT GPS device, try to create rfcomm */
	for (i=0; i<num_posdev; i++) 
	{
		/* Note that bluez does not provide this interface (its defined but not
	     * yet implemented) so we use btcond for creating rfcomm device(s)
	     */

	    proxy = dbus_g_proxy_new_for_name(bus,
	        BTCOND_DBUS, BTCOND_PATH, BTCOND_INTERFACE);

	    error = NULL;
	    tmp = NULL;
	    if(!dbus_g_proxy_call(proxy, BTCOND_CONNECT, &error,
	          G_TYPE_STRING, posdev[i].bonding,
	          G_TYPE_STRING, spp,
              G_TYPE_BOOLEAN, TRUE,
	          G_TYPE_INVALID,
	          G_TYPE_STRING, &tmp,
	          G_TYPE_INVALID)
	        || error || !tmp || !*tmp) 
	    {
	    	/*PDEBUG("dbus_g_proxy_call returned an error: (error=(%d,%s), tmp=%s\n",
	          error ? error->code : -1,
	          error ? error->message : "<null>",
	          tmp ? tmp : "<null>");
*/
	    	/* No error if already connected */
	    	if (error && !strstr(error->message,
	            "com.nokia.btcond.error.connected")) 
	    	{
	    		ERROR:
	    			fprintf(stderr, "Cannot send msg (service=%s, object=%s, interface=%s, "
	    					"method=%s) [%s]\n",
	    					BTCOND_DBUS,
	    					BTCOND_PATH,
	    					BTCOND_INTERFACE,
	    					BTCOND_CONNECT,
	    					error->message ? error->message : "<no error msg>");

	    			continue;
	    	} 
	    	else if(!tmp || !*tmp) 
	    	{
	    		/* hack: rfcommX device name is at the end of error message */
	    		char *last_space = strstr(error->message, " rfcomm");
	    		if (!last_space) 
	    		{
	    			goto ERROR;
	    		}

	    		g_free(tmp);
	    		tmp = g_strdup_printf("/dev/%s", last_space+1);
	    	}
	    }
	    g_object_unref(proxy);

	    if (tmp && tmp[0]) 
	    {
	    	rfcomms = (char **)realloc(rfcomms, (num_rfcomms+1)*sizeof(char *));
	    	if (!rfcomms) 
	    	{
	    		st = -1;
	    		errno = ENOMEM;
	    		goto OUT;
	    	}

	    	rfcomms[num_rfcomms] = tmp;
	    	num_rfcomms++;

	    	fprintf(stderr, "BT addr=%s, RFCOMM %s now exists (adapter=%s)\n",
	              posdev[i].bonding, tmp, posdev[i].adapter);

	    	tmp = NULL;
	    }
	    else 
	    {
	    	g_free(tmp);
	    }
	}

	if (num_rfcomms==0) 
	{
	    /* serial device creation failed */
	    fprintf(stderr, "No rfcomm created\n");
	    st = -1;
	    errno = EINVAL;

	} 
	else 
	{
	    /* Add null at the end */
	    rfcomms = (char **)realloc(rfcomms, (num_rfcomms+1)*sizeof(char *));
	    if (!rfcomms) 
	    {
	    	st = -1;
	    	errno = ENOMEM;

	    } 
	    else 
	    {
	    	rfcomms[num_rfcomms] = NULL;

	    	/* Just start the beast (to be done if everything is ok) */
	    	st = 0;
	    		    	
	    	*aprs_bt_port = g_strdup_printf("%s",rfcomms[0]);

	    }
	}

OUT:
/*	if (adapters) 
	{
	    g_strfreev(adapters);
	}
*/
	if (posdev) 
	{
	    for (i=0; i<num_posdev; i++) 
	    {
	    	if (posdev[i].bonding) 
	    	{
	    		free(posdev[i].bonding);
	    		memset(&posdev[i], 0, sizeof(bonding_t)); /* just in case */
	    	}
	    }
	    free(posdev);
	    posdev = 0;
	}

	if (bondings) 
	{
	    for (i=0; i<num_bondings; i++) 
	    {
	    	if (bondings[i].bonding) 
	    	{
	    		free(bondings[i].bonding);
	    		memset(&bondings[i], 0, sizeof(bonding_t)); /* just in case */
	    	}
	    }
	    free(bondings);
	    bondings = 0;
	}

	if (rfcomms) 
	{
		for (i=0; i<num_rfcomms; i++) 
		{
			if (rfcomms[i]) 
			{
				free(rfcomms[i]);
				rfcomms[i]=0;
			}
	   }
	   free(rfcomms);
	   rfcomms = 0;
	}

	if (bus) 
	{
		dbus_g_connection_unref(bus);
	}

	return st>-1;
}







//***********************************************************
// Serial port INIT
//***********************************************************
void update_aprs_tty_status()
{
	/*
	_aprs_tty_enable = (_aprs_tty_state == RCVR_UP);
	
	gtk_check_menu_item_set_active(
	    GTK_CHECK_MENU_ITEM(_menu_enable_aprs_tty_item), _aprs_tty_enable);
	    */
}

int serial_init () {
    int speed;
    
    euid = geteuid();
    egid = getegid();

    fprintf(stderr, "in serial_init\n");

    // clear port_channel
    port_data.channel = -1;

    // clear port active
    port_data.active = DEVICE_NOT_IN_USE;
    
    // clear port status
    port_data.status = DEVICE_DOWN;
    //set_aprs_tty_conn_state(RCVR_DOWN);
    
    update_aprs_tty_status();

    //gw-obex.h
    if(_aprs_tnc_method == TNC_CONNECTION_BT)
    {
    	// Bluetooth connection
    	gchar * aprs_bt_port = NULL;
    	
    	//fprintf(stderr, "Connecting to BT device...\n");
    	
    	if(!open_bluetooth_tty_connection(_aprs_tnc_bt_mac, &aprs_bt_port) || aprs_bt_port == NULL)
    	{
    		fprintf(stderr, "Failed to connect to BT device\n");
    		// Failed to connect
    		return -1;
    	}
    	
    	
    	snprintf(port_data.device_name, MAX_DEVICE_NAME, aprs_bt_port);
    	g_free(aprs_bt_port);
    	
    	fprintf(stderr, "BT Port: %s\n", port_data.device_name);
    }
    else
    {
    	snprintf(port_data.device_name, MAX_DEVICE_NAME, _aprs_tty_port);
    }
    
    
    // TODO - make these configurable
    port_data.device_type = DEVICE_SERIAL_KISS_TNC;
    port_data.sp = B9600; 
    

    // check for lock file
    
    // Try to open the serial port now
    ENABLE_SETUID_PRIVILEGE;
    port_data.channel = open(port_data.device_name, O_RDWR|O_NOCTTY);
    DISABLE_SETUID_PRIVILEGE;
    if (port_data.channel == -1){

        fprintf(stderr,"Could not open channel on port!\n");

        return (-1);
    }
    
    // get port attributes for new and old
    if (tcgetattr(port_data.channel, &port_data.t) != 0) {
        fprintf(stderr,"Could not get t port attributes for port!\n");

        // Here we should close the port and remove the lock.
        serial_detach();

        return (-1);
    }

    if (tcgetattr(port_data.channel, &port_data.t_old) != 0) {

        fprintf(stderr,"Could not get t_old port attributes for port!\n");

        // Here we should close the port and remove the lock.
        serial_detach();

        return (-1);
    }

    // set time outs
    port_data.t.c_cc[VMIN] = (cc_t)0;
    port_data.t.c_cc[VTIME] = (cc_t)20;

    // set port flags
    port_data.t.c_iflag &= ~(BRKINT | IGNPAR | PARMRK | INPCK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    port_data.t.c_iflag = (tcflag_t)(IGNBRK | IGNPAR);

    port_data.t.c_oflag = (0);
    port_data.t.c_lflag = (0);

#ifdef    CBAUD
    speed = (int)(port_data.t.c_cflag & CBAUD);
#else   // CBAUD
    speed = 0;
#endif  // CBAUD
    
    port_data.t.c_cflag = (tcflag_t)(HUPCL|CLOCAL|CREAD);
    port_data.t.c_cflag &= ~PARENB;
    switch (port_data.style){
        case(0):
            // No parity (8N1)
            port_data.t.c_cflag &= ~CSTOPB;
            port_data.t.c_cflag &= ~CSIZE;
            port_data.t.c_cflag |= CS8;
            break;

        case(1):
            // Even parity (7E1)
            port_data.t.c_cflag &= ~PARODD;
            port_data.t.c_cflag &= ~CSTOPB;
            port_data.t.c_cflag &= ~CSIZE;
            port_data.t.c_cflag |= CS7;
            break;

        case(2):
            // Odd parity (7O1):
            port_data.t.c_cflag |= PARODD;
            port_data.t.c_cflag &= ~CSTOPB;
            port_data.t.c_cflag &= ~CSIZE;
            port_data.t.c_cflag |= CS7;
            break;

        default:
            break;
    }

    port_data.t.c_cflag |= speed;
    // set input and out put speed
    if (cfsetispeed(&port_data.t, port_data.sp) == -1) 
    {
    	fprintf(stderr,"Could not set port input speed for port!\n");

        // Here we should close the port and remove the lock.
        serial_detach();

        return (-1);
    }

    if (cfsetospeed(&port_data.t, port_data.sp) == -1) {

        fprintf(stderr,"Could not set port output speed for port!\n");

        // Here we should close the port and remove the lock.
        serial_detach();

        return (-1);
    }

    if (tcflush(port_data.channel, TCIFLUSH) == -1) {

        fprintf(stderr,"Could not flush data for port!\n");

        // Here we should close the port and remove the lock.
        serial_detach();

        return (-1);
    }

    if (tcsetattr(port_data.channel,TCSANOW, &port_data.t) == -1) 
    {
        fprintf(stderr,"Could not set port attributes for port!\n");

        // Here we should close the port and remove the lock.
        serial_detach();

        return (-1);
    }

    // clear port active
    port_data.active = DEVICE_IN_USE;

    // clear port status
    port_data.status = DEVICE_UP;
    set_aprs_tty_conn_state(RCVR_UP);
    
    // Show the latest status in the interface control dialog
    update_aprs_tty_status();

    // Ensure we are in KISS mode
    if(port_data.device_type == DEVICE_SERIAL_KISS_TNC)
    {
    	// Send KISS init string
    	gchar * cmd = g_strdup("\nINT KISS\nRESTART\n");
    	port_write_string(cmd, strlen(cmd), APRS_PORT_TTY);
    }
    
    // return good condition
    return (1);
}

gboolean read_port_data()
{
    unsigned char cin, last;
    gint i;
//    struct timeval tmv;
//    fd_set rd;

    cin = (unsigned char)0;
    last = (unsigned char)0;

	int skip = 0;
	
    // Handle all EXCEPT AX25_TNC interfaces here
    // Get one character
//fprintf(stderr,"waiting for tty in... ");
	port_data.scan = (int)read(port_data.channel,&cin,1);
  
	if(port_data.scan == 0) return TRUE;
	else if(port_data.scan < 0) return FALSE;
	
//fprintf(stderr,"%02x \n",cin);


    // Below is code for ALL types of interfaces
    if (port_data.scan > 0 && port_data.status == DEVICE_UP ) {

        if (port_data.device_type != DEVICE_AX25_TNC)
            port_data.bytes_input += port_data.scan;      // Add character to read buffer



        // Handle all EXCEPT AX25_TNC interfaces here
        if (port_data.device_type != DEVICE_AX25_TNC){


            // Do special KISS packet processing here.
            // We save the last character in
            // port_data.channel2, as it is
            // otherwise only used for AX.25 ports.

            if ( (port_data.device_type == DEVICE_SERIAL_KISS_TNC)
                    || (port_data.device_type == DEVICE_SERIAL_MKISS_TNC) ) {


                if (port_data.channel2 == KISS_FESC) { // Frame Escape char
                    if (cin == KISS_TFEND) { // Transposed Frame End char

                        // Save this char for next time
                        // around
                    	port_data.channel2 = cin;

                        cin = KISS_FEND;
                    }
                    else if (cin == KISS_TFESC) { // Transposed Frame Escape char

                        // Save this char for next time
                        // around
                    	port_data.channel2 = cin;

                        cin = KISS_FESC;
                    }
                    else {
                    	port_data.channel2 = cin;
                    }
                }
                else if (port_data.channel2 == KISS_FEND) { // Frame End char
                    // Frame start or frame end.  Drop
                    // the next character which should
                    // either be another frame end or a
                    // type byte.

// Note this "type" byte is where it specifies which KISS interface
// the packet came from.  We may want to use this later for
// multi-drop KISS or other types of KISS protocols.

                    // Save this char for next time
                    // around
                	port_data.channel2 = cin;

                    skip++;
                }
                else if (cin == KISS_FESC) { // Frame Escape char
                	port_data.channel2 = cin;
                    skip++;
                }
                else {
                	port_data.channel2 = cin;
                }
            }   // End of first special KISS processing


            // We shouldn't see any AX.25 flag
            // characters on a KISS interface because
            // they are stripped out by the KISS code.
            // What we should see though are KISS_FEND
            // characters at the beginning of each
            // packet.  These characters are where we
            // should break the data apart in order to
            // send strings to the decode routines.  It
            // may be just fine to still break it on \r
            // or \n chars, as the KISS_FEND should
            // appear immediately afterwards in
            // properly formed packets.


            if ( (!skip)
                    && (cin == (unsigned char)'\r'
                        || cin == (unsigned char)'\n'
                        || port_data.read_in_pos >= (MAX_DEVICE_BUFFER - 1)
                        || ( (cin == KISS_FEND) && (port_data.device_type == DEVICE_SERIAL_KISS_TNC) )
                        || ( (cin == KISS_FEND) && (port_data.device_type == DEVICE_SERIAL_MKISS_TNC) ) )
                   && port_data.data_type == 0) {     // If end-of-line

// End serial/net type data send it to the decoder Put a terminating
// zero at the end of the read-in data

            	port_data.device_read_buffer[port_data.read_in_pos] = (char)0;

                if (port_data.status == DEVICE_UP && port_data.read_in_pos > 0) {
                    int length;

                    // Compute length of string in
                    // circular queue

                    //fprintf(stderr,"%d\t%d\n",port_data.read_in_pos,port_data.read_out_pos);

                    // KISS TNC sends binary data
                    if ( (port_data.device_type == DEVICE_SERIAL_KISS_TNC)
                            || (port_data.device_type == DEVICE_SERIAL_MKISS_TNC) ) {

                        length = port_data.read_in_pos - port_data.read_out_pos;
                        if (length < 0)
                            length = (length + MAX_DEVICE_BUFFER) % MAX_DEVICE_BUFFER;

                        length++;
                    }
                    else {  // ASCII data
                        length = 0;
                    }

                    channel_data(
                        (unsigned char *)port_data.device_read_buffer,
                        length);   // Length of string
                }

                for (i = 0; i <= port_data.read_in_pos; i++)
                    port_data.device_read_buffer[i] = (char)0;

                port_data.read_in_pos = 0;
            }
            else if (!skip) {

                // Check for binary WX station data
                if (cin == '\0')    // OWW WX daemon sends 0x00's!
                    cin = '\n';

                if (port_data.read_in_pos < (MAX_DEVICE_BUFFER - 1) ) {
                    port_data.device_read_buffer[port_data.read_in_pos] = (char)cin;
                    port_data.read_in_pos++;
                    port_data.device_read_buffer[port_data.read_in_pos] = (char)0;
                }
                else {
                    port_data.read_in_pos = 0;
                }
            }

        }   // End of non-AX.25 interface code block


        return TRUE;
    }
//    else if (port_data.status == DEVICE_UP) {    /* error or close on read */
//    	// cause re-connect
//
//
//    }
	
    return TRUE;
}


#endif //INCLUDE_APRS
