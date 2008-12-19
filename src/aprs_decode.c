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

#include "aprs_decode.h"
#include "data.h"
#include "aprs.h"
#include "types.h"
#include "math.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "hashtable.h"
#include "poi.h"
#include "display.h"
#include "aprs_message.h"
//#include "aprs_weather.h"

#define WX_TYPE 'X'

float gust[60];                     // High wind gust for each min. of last hour
int gust_write_ptr = 0;
int gust_read_ptr = 0;
int gust_last_write = 0;

float rain_base[24];                // hundredths of an inch
float rain_minute[60];              // Total rain for each min. of last hour, hundredths of an inch
int rain_check = 0;                 // Flag for re-checking rain_total each hour

int redraw_on_new_data;         // Station redraw request
int wait_to_redraw;             /* wait to redraw until system is up */
time_t posit_next_time;

/////////////
float rain_minute_total = 0.0;      // Total for last hour, hundredths of an inch
int rain_minute_last_write = -1;    // Write pointer for rain_minute[] array, set to an invalid number
float rain_00 = 0.0;                // hundredths of an inch
float rain_24 = 0.0;                // hundredths of an inch


int station_count;              // number of stored stations
int station_count_save = 0;     // old copy of above
AprsDataRow *n_first;               // pointer to first element in name sorted station list
AprsDataRow *n_last;                // pointer to last  element in name sorted station list
AprsDataRow *t_oldest;              // pointer to first element in time sorted station list (oldest)
AprsDataRow *t_newest;              // pointer to last  element in time sorted station list (newest)
time_t last_station_remove;     // last time we did a check for station removing
time_t last_sec,curr_sec;       // for comparing if seconds in time have changed
int next_time_sn;               // time serial number for unique time index

int stations_heard = 0;

////////////

double cvt_m2len  = 3.28084;   // m to ft   // from meter
double cvt_kn2len = 1.0;       // knots to knots;  // from knots
double cvt_mi2len = 0.8689607; // mph to knots / mi to nm;  // from miles
double cvt_dm2len = 0.328084;  // dm to ft  // from decimeter
double cvt_hm2len = 0.0539957; // hm to nm;  // from hectometer


int emergency_distance_check = 1;
float emergency_range = 280.0;  // Default is 4hrs @ 70mph distance


int decoration_offset_x = 0;
int decoration_offset_y = 0;
int last_station_info_x = 0;
int last_station_info_y = 0;
int fcc_lookup_pushed = 0;
int rac_lookup_pushed = 0;

time_t last_object_check = 0;   // Used to determine when to re-transmit objects/items

time_t last_emergency_time = 0;
char last_emergency_callsign[MAX_CALLSIGN+1];
int st_direct_timeout = 60 * 60;        // 60 minutes.

// Used in search_station_name() function.  Shortcuts into the
// station list based on the least-significant 7 bits of the first
// two letters of the callsign/object name.
AprsDataRow *station_shortcuts[16384];

// calculate every half hour, display in status line every 5 minutes
#define ALOHA_CALC_INTERVAL 1800 
#define ALOHA_STATUS_INTERVAL 300

int process_emergency_packet_again = 0;


////////////

int  altnet;
char altnet_call[MAX_CALLSIGN+1];
static struct hashtable *tactical_hash = NULL;


char echo_digis[6][MAX_CALLSIGN+1];
int  tracked_stations = 0;       // A count variable used in debug code only

int trail_segment_distance;     // Segment missing if greater distance
int trail_segment_time;         // Segment missing if above this time (mins)
int skip_dupe_checking;


void station_shortcuts_update_function(int hash_key_in, AprsDataRow *p_rem);
void delete_station_memory(AprsDataRow *p_del);
void decode_Peet_Bros(int from, unsigned char *data, AprsWeatherRow *weather, int type);
void decode_U2000_P(int from, unsigned char *data, AprsWeatherRow *weather);
void decode_U2000_L(int from, unsigned char *data, AprsWeatherRow *weather);

void init_weather(AprsWeatherRow *weather) {    // clear weather data

    weather->wx_sec_time             = (time_t)0;
    weather->wx_storm                = 0;
    weather->wx_time[0]              = '\0';
    weather->wx_course[0]            = '\0';
    weather->wx_speed[0]             = '\0';
    weather->wx_speed_sec_time       = 0; // ??
    weather->wx_gust[0]              = '\0';
    weather->wx_hurricane_radius[0]  = '\0';
    weather->wx_trop_storm_radius[0] = '\0';
    weather->wx_whole_gale_radius[0] = '\0';
    weather->wx_temp[0]              = '\0';
    weather->wx_rain[0]              = '\0';
    weather->wx_rain_total[0]        = '\0';
    weather->wx_snow[0]              = '\0';
    weather->wx_prec_24[0]           = '\0';
    weather->wx_prec_00[0]           = '\0';
    weather->wx_hum[0]               = '\0';
    weather->wx_baro[0]              = '\0';
    weather->wx_fuel_temp[0]         = '\0';
    weather->wx_fuel_moisture[0]     = '\0';
    weather->wx_type                 = '\0';
    weather->wx_station[0]           = '\0';
}


int tactical_keys_equal(void *key1, void *key2) {

    if (strlen((char *)key1) == strlen((char *)key2)
            && strncmp((char *)key1,(char *)key2,strlen((char *)key1))==0) {
        return(1);
    }
    else {
        return(0);
    }
}


// Multiply all the characters in the callsign, truncated to
// TACTICAL_HASH_SIZE
//
unsigned int tactical_hash_from_key(void *key) {
    unsigned char *jj = key;
    unsigned int tac_hash = 1;

    while (*jj != '\0') {
       tac_hash = tac_hash * (unsigned int)*jj++;
    }

    tac_hash = tac_hash % TACTICAL_HASH_SIZE;

    return (tac_hash);
}




/* valid characters for APRS weather data fields */
int is_aprs_chr(char ch) {

    if (g_ascii_isdigit(ch) || ch==' ' || ch=='.' || ch=='-')
    return(1);
    else
    return(0);
}




//
// Search station record by callsign
// Returns a station with a call equal or after the searched one
//
// We use a doubly-linked list for the stations, so we can traverse
// in either direction.  We also use a 14-bit hash table created
// from the first two letters of the call to dump us into the
// beginning of the correct area that may hold the callsign, which
// reduces search time quite a bit.  We end up doing a linear search
// only through a small area of the linked list.
//
// DK7IN:  I don't look at case, objects and internet names could
// have lower case.
//
int search_station_name(AprsDataRow **p_name, char *call, int exact) {
    int kk;
    int hash_key;
    int result;
    int ok = 1;

    (*p_name) = n_first;                                // start of alphabet

    if (call[0] == '\0') {
        // If call we're searching for is empty, return n_first as
        // the pointer.
        return(0);
    }

    // We create the hash key out of the lower 7 bits of the first
    // two characters, creating a 14-bit key (1 of 16384)
    //
 
    hash_key = (int)((call[0] & 0x7f) << 7);
    hash_key = hash_key | (int)(call[1] & 0x7f);

    // Look for a match using hash table lookup
    //
    (*p_name) = station_shortcuts[hash_key];

    if ((*p_name) == NULL) {    // No hash-table entry found.
        int mm;

        // No index found for that letter.  Walk the array until
        // we find an entry that is filled.  That'll be our
        // potential insertion point (insertion into the list will
        // occur just ahead of the hash entry).
        for (mm = hash_key+1; mm < 16384; mm++) {
            if (station_shortcuts[mm] != NULL) 
            {
            	(*p_name) = station_shortcuts[mm];
                
                break;
            }
        }
    }


    // If we got to this point, we either have a NULL pointer or a
    // real hash-table pointer entry.  A non-NULL pointer means that
    // we have a match for the lower seven bits of the first two
    // characters of the callsign.  Check the rest of the callsign,
    // and jump out of the loop if we get outside the linear search
    // area (if first two chars are different).

    kk = (int)strlen(call);

    // Search linearly through list.  Stop at end of list or break.
    while ( (*p_name) != NULL) {

        if (exact) {
            // Check entire string for exact match
            result = strcmp( call, (*p_name)->call_sign );
        }
        else {
            // Check first part of string for match
            result = strncmp( call, (*p_name)->call_sign, kk );
        }

        if (result < 0) {   // We went past the right location.
                            // We're done.
            ok = 0;

            break;
        }
        else if (result == 0) { // Found a possible match
            break;
        }
        else {  // Result > 0.  We haven't found it yet.
        	
        	(*p_name) = (*p_name)->n_next;  // Next element in list
        }
    }

    // Did we find anything?
    if ( (*p_name) == NULL) {
        ok = 0;
        return(ok); // Nope.  No match found.
    }

    // If "exact" is set, check that the string lengths match as
    // well.  If not, we didn't find it.
    if (exact && ok && strlen((*p_name)->call_sign) != strlen(call))
        ok = 0;

    return(ok);         // if not ok: p_name points to correct insert position in name list
}



/*
 *  Check if current packet is a delayed echo
 */
int is_trailpoint_echo(AprsDataRow *p_station) {
    int packets = 1;
    time_t checktime;
    AprsTrackRow *ptr;


    // Check whether we're to skip checking for dupes (reading in
    // objects/items from file is one such case).
    //
    if (skip_dupe_checking) {
        return(0);  // Say that it isn't an echo
    }

    // Start at newest end of linked list and compare.  Return if we're
    // beyond the checktime.
    ptr = p_station->newest_trackpoint;

    if (ptr == NULL)
        return(0);  // first point couldn't be an echo

    checktime = p_station->sec_heard - TRAIL_ECHO_TIME*60;

    while (ptr != NULL) {

        if (ptr->sec < checktime)
            return(0);  // outside time frame, no echo found

        if ((p_station->coord_lon == ptr->trail_long_pos)
                && (p_station->coord_lat == ptr->trail_lat_pos)
                && (p_station->speed == '\0' || ptr->speed < 0
                        || (long)(atof(p_station->speed)*18.52) == ptr->speed)
                        // current: char knots, trail: long 0.1m (-1 is undef)
                && (p_station->course == '\0' || ptr->course <= 0
                        || atoi(p_station->course) == ptr->course)
                        // current: char, trail: int (-1 is undef)
                && (p_station->altitude == '\0' || ptr->altitude <= -99999l
                        || atoi(p_station->altitude)*10 == ptr->altitude)) {
                        // current: char, trail: int (-99999l is undef)

            return(1);              // we found a delayed echo
        }
        ptr = ptr->prev;
        packets++;
    }
    return(0);                      // no echo found
}


/*
 *  Keep track of last six digis that echo my transmission
 */
void upd_echo(char *path) {
    int i,j,len;

    if (echo_digis[5][0] != '\0') {
        for (i=0;i<5;i++) {
            xastir_snprintf(echo_digis[i],
                MAX_CALLSIGN+1,
                "%s",
                echo_digis[i+1]);

        }
        echo_digis[5][0] = '\0';
    }
    for (i=0,j=0;i < (int)strlen(path);i++) {
        if (path[i] == '*')
            break;
        if (path[i] == ',')
            j=i;
    }
    if (j > 0)
        j++;                    // first char of call
    if (i > 0 && i-j <= 9) {
        len = i-j;
        for (i=0;i<5;i++) {     // look for free entry
            if (echo_digis[i][0] == '\0')
                break;
        }
        substr(echo_digis[i],path+j,len);
    }
}


//
// Store one trail point.  Allocate storage for the new data.
//
// We now store track data in a doubly-linked list.  Each record has a
// pointer to the previous and the next record in the list.  The main
// station record has a pointer to the oldest and the newest end of the
// chain, and the chain can be traversed in either order.
//
int store_trail_point(AprsDataRow *p_station,
                      long lon,
                      long lat,
                      time_t sec,
                      char *alt,
                      char *speed,
                      char *course,
                      short stn_flag) {
// TODO - trails are currently disabled
// This seems to fall over!!!
	return 1;
	
	
    char flag;
    AprsTrackRow *ptr;


    // Allocate storage for the new track point
    ptr = malloc(sizeof(AprsTrackRow));
    if (ptr == NULL) {
        return(0); // Failed due to malloc
    }
 
    // Check whether we have any track data saved
    if (p_station->newest_trackpoint == NULL) {
        // new trail, do initialization

        tracked_stations++;

        // Assign a new trail color 'cuz it's a new trail
        p_station->trail_color = new_trail_color(p_station->call_sign);
    }

    // Start linking the record to the new end of the chain
    ptr->prev = p_station->newest_trackpoint;   // Link to record or NULL
    ptr->next = NULL;   // Newest end of chain

    // Have an older record already?
    if (p_station->newest_trackpoint != NULL) { // Yes
        p_station->newest_trackpoint->next = ptr;
    }
    else {  // No, this is our first record
        p_station->oldest_trackpoint = ptr;
    }

    // Link it in as our newest record
    p_station->newest_trackpoint = ptr;

    ptr->trail_long_pos = lon;
    ptr->trail_lat_pos  = lat;
    ptr->sec            = sec;

    if (alt[0] != '\0')
            ptr->altitude = atoi(alt)*10;
    else            
            ptr->altitude = -99999l;

    if (speed[0] != '\0')
            ptr->speed  = (long)(atof(speed)*18.52);
    else
            ptr->speed  = -1;

    if (course[0] != '\0')
            ptr->course = (int)(atof(course) + 0.5);    // Poor man's rounding
    else
            ptr->course = -1;

    flag = '\0';                    // init flags

    if ((stn_flag & ST_DIRECT) != 0)
            flag |= TR_LOCAL;           // set "local" flag

    if (ptr->prev != NULL) {    // we have at least two points...
        // Check whether distance between points is too far.  We
        // must convert from degrees to the Xastir coordinate system
        // units, which are 100th of a second.
        if (    abs(lon - ptr->prev->trail_long_pos) > (trail_segment_distance * 60*60*100) ||
                abs(lat - ptr->prev->trail_lat_pos)  > (trail_segment_distance * 60*60*100) ) {

            // Set "new track" flag if there's
            // "trail_segment_distance" degrees or more between
            // points.  Originally was hard-coded to one degree, now
            // set by a slider in the timing dialog.
            flag |= TR_NEWTRK;
        }
        else {
            // Check whether trail went above our maximum time
            // between points.  If so, don't draw segment.
            if (abs(sec - ptr->prev->sec) > (trail_segment_time *60)) {

                // Set "new track" flag if long delay between
                // reception of two points.  Time is set by a slider
                // in the timing dialog.
                flag |= TR_NEWTRK;
            }
        }
        
        // Since we have more then 1 previous track point, ensure we don't go over the configured max
        // (_aprs_std_pos_hist)
        AprsTrackRow *ptr_tmp = ptr;
        AprsTrackRow *ptr_tmp_previous = ptr;
        gint trackCount = 0;
        
        while( (ptr_tmp = ptr_tmp->prev) )
        {
        	if(trackCount > _aprs_std_pos_hist)
        	{
        		fprintf(stderr, "DEBUG: Deleting old track point\n");
        		// Remove track
        		ptr_tmp->prev->next = ptr_tmp->next;
        		if(ptr_tmp->next) 
        		{
        			ptr_tmp->next->prev = ptr_tmp->prev;
        		}
        		ptr_tmp_previous = ptr_tmp->prev;
        		free( ptr_tmp);
        		ptr_tmp = ptr_tmp_previous;
        		
        		
        	}
        	else
        	{
        		trackCount ++;
        	}
        	
        	ptr_tmp = ptr_tmp->prev;
        }
        
    }
    else {
        // Set "new track" flag for first point received.
        flag |= TR_NEWTRK;
    }
    ptr->flag = flag;

    return(1);  // We succeeded
}



/*
 *  Substring function WITH a terminating NULL char, needs a string of at least size+1
 */
void substr(char *dest, char *src, int size) 
{
    snprintf(dest, size+1, "%s", src);
}



/*
 *  Remove element from name ordered list
 */
void remove_name(AprsDataRow *p_rem) {      // todo: return pointer to next element
    int update_shortcuts = 0;
    int hash_key;   // We use a 14-bit hash key


    // Do a quick check to see if we're removing a station record
    // that is pointed to by our pointer shortcuts array.
    // If so, update our pointer shortcuts after we're done.
    //
    // We create the hash key out of the lower 7 bits of the first
    // two characters, creating a 14-bit key (1 of 16384)
    //
    hash_key = (int)((p_rem->call_sign[0] & 0x7f) << 7);
    hash_key = hash_key | (int)(p_rem->call_sign[1] & 0x7f);

    if (station_shortcuts[hash_key] == p_rem) {
        // Yes, we're trying to remove a record that a hash key
        // directly points to.  We'll need to redo that hash key
        // after we remove the record.
        update_shortcuts++;
    }


    // Proceed to the station record removal
    //
    if (p_rem->n_prev == NULL) { // Appears to be first element in list

        if (n_first == p_rem) {  // Yes, head of list

            // Make list head point to 2nd element in list (or NULL)
            // so that we can delete the current record.
            n_first = p_rem->n_next;
        }
        else {  // No, not first element in list.  Problem!  The
                // list pointers are inconsistent for some reason.
                // The chain has been broken and we have dangling
                // pointers.

            fprintf(stderr,
                "remove_name(): ERROR: p->n_prev == NULL but p != n_first\n");

abort();    // Cause a core dump at this point
// Perhaps we could do some repair to the list pointers here?  Start
// at the other end of the chain and navigate back to this end, then
// fix up n_first to point to it?  This is at the risk of a memory
// leak, but at least Xastir might continue to run.

        }
    }
    else {  // Not the first element in the list.  Fix up pointers
            // to skip the current record.
        p_rem->n_prev->n_next = p_rem->n_next;
    }


    if (p_rem->n_next == NULL) { // Appears to be last element in list

        if (n_last == p_rem) {   // Yes, tail of list

            // Make list tail point to previous element in list (or
            // NULL) so that we can delete the current record.
            n_last = p_rem->n_prev;
        }
        else {  // No, not last element in list.  Problem!  The list
                // pointers are inconsistent for some reason.  The
                // chain has been broken and we have dangling
                // pointers.

            fprintf(stderr,
                "remove_name(): ERROR: p->n_next == NULL but p != n_last\n");

abort();    // Cause a core dump at this point
// Perhaps we could do some repair to the list pointers here?  Start
// at the other end of the chain and navigate back to this end, then
// fix up n_last to point to it?  This is at the risk of a memory
// leak, but at least Xastir might continue to run.

        }
    }
    else {  // Not the last element in the list.  Fix up pointers to
            // skip the current record.
        p_rem->n_next->n_prev = p_rem->n_prev;
    }


    // Update our pointer shortcuts.  Pass the removed hash_key to
    // the function so that we can try to redo just that hash_key
    // pointer.
    if (update_shortcuts) {
//fprintf(stderr,"\t\t\t\t\t\tRemoval of hash key: %i\n", hash_key);

        // The -1 tells the function to redo all of the hash table
        // pointers because we deleted one of them.  Later we could
        // optimize this so that only the specific pointer is fixed
        // up.
        station_shortcuts_update_function(-1, NULL);
    }


}



/*
 *  Station Capabilities, Queries and Responses      [APRS Reference, chapter 15]
 */
//
// According to Bob Bruninga we should wait a random time between 0
// and 120 seconds before responding to a general query.  We use the
// delayed-ack mechanism to add this randomness.
//
// NOTE:  We may end up sending these to RF when the query came in
// over the internet.  We should check that.
//
int process_query( /*@unused@*/ char *call_sign, /*@unused@*/ char *path,char *message,TAprsPort port, /*@unused@*/ int third_party) {
    char temp[100];
    int ok = 0;
    float randomize;


    // Generate a random number between 0.0 and 1.0
    randomize = rand() / (float)RAND_MAX;

    // Convert to between 0 and 120 seconds
    randomize = randomize * 120.0;

    // Check for proper usage of the ?APRS? query
//
// NOTE:  We need to add support in here for the radius circle as
// listed in the spec for general queries.  Right now we respond to
// all queries, whether we're inside the circle or not.  Spec says
// this:
//
// ?Query?Lat,Long,Radius
// 1  n  1 n 1 n  1  4 Bytes
//
// i.e. ?APRS? 34.02,-117.15,0200
//
// Note leading space in latitude as its value is positive.
// Lat/long are floating point degrees.  N/E are positive, indicated
// by a leading space.  S/W are negative.  Radius is in miles
// expressed as a fixed 4-digit number in whole miles.  All stations
// inside the specified circle should respond with a position report
// and a status report.
//
    if (!ok && strncmp(message,"APRS?",5)==0) {
        //
        // Initiate a delayed transmit of our own posit.
        // UpdateTime() uses posit_next_time to decide when to
        // transmit, so we'll just muck with that.
        //
        if ( posit_next_time - sec_now() < randomize ) {
            // Skip setting it, as we'll transmit soon anyway
        }
        else {
            posit_next_time = (size_t)(sec_now() + randomize);
        }
        ok = 1;
    }
    // Check for illegal case for the ?APRS? query
    if (!ok && g_strncasecmp(message,"APRS?",5)==0) {
        ok = 1;
//        fprintf(stderr,
//            "%s just queried us with an illegal query: %s\n",
//            call_sign,
//            message),
//        fprintf(stderr,
//            "Consider sending a message, asking them to follow the spec\n");
    }

    // Check for proper usage of the ?WX? query
    if (!ok && strncmp(message,"WX?",3)==0) {
    	// WX is not supported

        ok = 1;
    }
    
    // Check for illegal case for the ?WX? query
    if (!ok && g_strncasecmp(message,"WX?",3)==0) {
        ok = 1;
//        fprintf(stderr,
//            "%s just queried us with an illegal query: %s\n",
//            call_sign,
//            message),
//        fprintf(stderr,
//            "Consider sending a message, asking them to follow the spec\n");
    }

    return(ok);
}



/*
 *  Get new trail color for a call
 */
int new_trail_color(char *call) {
    // TODO - stub

    return(0);
}


/*
 *  Insert existing element into time ordered list before p_time
 *  The p_new record ends up being on the "older" side of p_time when
 *  all done inserting (closer in the list to the t_oldest pointer).
 *  If p_time == NULL, insert at newest end of list.
 */
void insert_time(AprsDataRow *p_new, AprsDataRow *p_time) {
	
	
    // Set up pointer to next record (or NULL), sorted by time
    p_new->t_newer = p_time;

    if (p_time == NULL) {               // add to end of list (becomes newest station)

        p_new->t_older = t_newest;         // connect to previous end of list

        if (t_newest == NULL)             // if list empty, create list
            t_oldest = p_new;            // it's now our only station on the list
        else
            t_newest->t_newer = p_new;     // list not empty, link original last record to our new one

        t_newest = p_new;                 // end of list (newest record pointer) points to our new record
    }

    else {                            // Else we're inserting into the middle of the list somewhere

        p_new->t_older = p_time->t_older;

        if (p_time->t_older == NULL)     // add to end of list (new record becomes oldest station)
            t_oldest = p_new;
        else
            p_time->t_older->t_newer = p_new; // else 

        p_time->t_older = p_new;
    }
    
    // TODO - this may need implementing? 
    /*
    if(_aprs_max_stations > 0 && stations_heard > _aprs_max_stations)
    {
    	// Delete the oldest
    	if(t_oldest != NULL)
    	{
  //  		fprintf(stderr, "DEBUG: oldest station deleted\n");
    		
    		delete_station_memory(t_oldest);
    	}
    	else
    	{
    		stations_heard++;
    	}
    }
    else
    {
//    	fprintf(stderr, "DEBUG: Stations in memory: %d\n", stations_heard);
    	
    	stations_heard++; // Should really be done in the new station method
    }
    */
}




/*
 *  Remove element from time ordered list
 */
void remove_time(AprsDataRow *p_rem) {      // todo: return pointer to next element

    if (p_rem->t_older == NULL) { // Appears to be first element in list

        if (t_oldest == p_rem) {  // Yes, head of list (oldest)

            // Make oldest list head point to 2nd element in list (or NULL)
            // so that we can delete the current record.
            t_oldest = p_rem->t_newer;
        }
        else {  // No, not first (oldest) element in list.  Problem!
                // The list pointers are inconsistent for some
                // reason.  The chain has been broken and we have
                // dangling pointers.

            fprintf(stderr,
                "remove_time(): ERROR: p->t_older == NULL but p != t_oldest\n");

abort();    // Cause a core dump at this point
// Perhaps we could do some repair to the list pointers here?  Start
// at the other end of the chain and navigate back to this end, then
// fix up t_oldest to point to it?  This is at the risk of a memory
// leak, but at least Xastir might continue to run.

        }
    }
    else {  // Not the first (oldest) element in the list.  Fix up
            // pointers to skip the current record.
        p_rem->t_older->t_newer = p_rem->t_newer;
    }


    if (p_rem->t_newer == NULL) { // Appears to be last (newest) element in list

        if (t_newest == p_rem) {   // Yes, head of list (newest)

            // Make newest list head point to previous element in
            // list (or NULL) so that we can delete the current
            // record.
            t_newest = p_rem->t_older;
        }
        else {  // No, not newest element in list.  Problem!  The
                // list pointers are inconsistent for some reason.
                // The chain has been broken and we have dangling
                // pointers.

            fprintf(stderr,
                "remove_time(): ERROR: p->t_newer == NULL but p != t_newest\n");

abort();    // Cause a core dump at this point
// Perhaps we could do some repair to the list pointers here?  Start
// at the other end of the chain and navigate back to this end, then
// fix up t_newest to point to it?  This is at the risk of a memory
// leak, but at least Xastir might continue to run.

        }
    }
    else {  // Not the newest element in the list.  Fix up pointers
            // to skip the current record.
        p_rem->t_newer->t_older = p_rem->t_older;
    }
    
    stations_heard--;
}




/*
 *  Move station record before p_time in time ordered list
 */
void move_station_time(AprsDataRow *p_curr, AprsDataRow *p_time) {

    if (p_curr != NULL) {               // need a valid record
        remove_time(p_curr);
        insert_time(p_curr,p_time);
    }
}


/*
 *  Extract powergain and/or range from APRS info field:
 * "PHG1234/", "PHG1234", or "RNG1234" from APRS data extension.
 */
int extract_powergain_range(char *info, char *phgd) {
    int i,found,len;
    char *info2;


//fprintf(stderr,"Info:%s\n",info);

    // Check whether two strings of interest are present and snag a
    // pointer to them.
    info2 = strstr(info,"RNG");
    if (!info2)
        info2 = strstr(info,"PHG");
    if (!info2) {
        phgd[0] = '\0';
        return(0);
    }

    found=0;
    len = (int)strlen(info2);

    if (len >= 9 && strncmp(info2,"PHG",3)==0
            && info2[7]=='/'
            && info2[8]!='A'  // trailing '/' not defined in Reference...
            && g_ascii_isdigit(info2[3])
            && g_ascii_isdigit(info2[4])
            && g_ascii_isdigit(info2[5])
            && g_ascii_isdigit(info2[6])) {
        substr(phgd,info2,7);
        found = 1;
        for (i=0;i<=len-8;i++)        // delete powergain from data extension field
            info2[i] = info2[i+8];
    }
    else {
        if (len >= 7 && strncmp(info2,"PHG",3)==0
                && g_ascii_isdigit(info2[3])
                && g_ascii_isdigit(info2[4])
                && g_ascii_isdigit(info2[5])
                && g_ascii_isdigit(info2[6])) {
            substr(phgd,info2,7);
            found = 1;
            for (i=0;i<=len-7;i++)        // delete powergain from data extension field
                info2[i] = info2[i+7];
        }
        else if (len >= 7 && strncmp(info2,"RNG",3)==0
                && g_ascii_isdigit(info2[3])
                && g_ascii_isdigit(info2[4])
                && g_ascii_isdigit(info2[5])
                && g_ascii_isdigit(info2[6])) {
            substr(phgd,info2,7);
            found = 1;
            for (i=0;i<=len-7;i++)        // delete powergain from data extension field
                info2[i] = info2[i+7];
        }
        else {
            phgd[0] = '\0';
        }
    }
    return(found);
}


/*
 *  Extract omnidf from APRS info field          "DFS1234/"    from APRS data extension
 */
int extract_omnidf(char *info, char *phgd) {
    int i,found,len;

    found=0;
    len = (int)strlen(info);
    if (len >= 8 && strncmp(info,"DFS",3)==0 && info[7]=='/'    // trailing '/' not defined in Reference...
                 && g_ascii_isdigit(info[3]) && g_ascii_isdigit(info[5]) && g_ascii_isdigit(info[6])) {
        substr(phgd,info,7);
        for (i=0;i<=len-8;i++)        // delete omnidf from data extension field
            info[i] = info[i+8];
        return(1);
    }
    else {
        phgd[0] = '\0';
        return(0);
    }
}


//
//  Extract speed and/or course from beginning of info field
//
// Returns course in degrees, speed in KNOTS.
//
int extract_speed_course(char *info, char *speed, char *course) {
    int i,found,len;

    len = (int)strlen(info);
    found = 0;
    if (len >= 7) {
        found = 1;
        for(i=0; found && i<7; i++) {           // check data format
            if (i==3) {                         // check separator
                if (info[i]!='/')
                    found = 0;
            }
            else {
                if( !( g_ascii_isdigit(info[i])
                        || (info[i] == ' ')     // Spaces and periods are allowed.  Need these
                        || (info[i] == '.') ) ) // here so that we can get the field deleted
                    found = 0;
            }
        }
    }
    if (found) {
        substr(course,info,3);
        substr(speed,info+4,3);
        for (i=0;i<=len-7;i++)        // delete speed/course from info field
            info[i] = info[i+7];
    }
    if (!found || atoi(course) < 1) {   // course 0 means undefined
//        speed[0] ='\0';   // Don't do this!  We can have a valid
//        speed without a valid course.
        course[0]='\0';
    }
    else {  // recheck data format looking for undefined fields
        for(i=0; i<2; i++) {
            if( !(g_ascii_isdigit(speed[i]) ) )
                speed[0] = '\0';
            if( !(g_ascii_isdigit(course[i]) ) )
                course[0] = '\0';
        }
    }

    return(found);
}





/*
 *  Extract Area Object
 */
void extract_area(AprsDataRow *p_station, char *data) {
    int i, val, len;
    unsigned int uval;
    AprsAreaObject temp_area;

    /* NOTE: If we are here, the symbol was the area symbol.  But if this
       is a slightly corrupted packet, we shouldn't blow away the area info
       for this station, since it could be from a previously received good
       packet.  So we will work on temp_area and only copy to p_station at
       the end, returning on any error as we parse. N7TAP */

    //fprintf(stderr,"Area Data: %s\n", data);

    len = (int)strlen(data);
    val = data[0] - '0';
    if (val >= 0 && val <= AREA_MAX) {
        temp_area.type = val;
        val = data[4] - '0';
        if (data[3] == '/') {
            if (val >=0 && val <= 9) {
                temp_area.color = val;
            }
            else {
                return;
            }
        }
        else if (data[3] == '1') {
            if (val >=0 && val <= 5) {
                temp_area.color = 10 + val;
            }
            else {
                return;
            }
        }

        val = 0;
        if (isdigit((int)data[1]) && isdigit((int)data[2])) {
            val = (10 * (data[1] - '0')) + (data[2] - '0');
        }
        else {
            return;
        }
        temp_area.sqrt_lat_off = val;

        val = 0;
        if (isdigit((int)data[5]) && isdigit((int)data[6])) {
            val = (10 * (data[5] - '0')) + (data[6] - '0');
        }
        else {
            return;
        }
        temp_area.sqrt_lon_off = val;

        for (i = 0; i <= len-7; i++) // delete area object from data extension field
            data[i] = data[i+7];
        len -= 7;

        if (temp_area.type == AREA_LINE_RIGHT || temp_area.type == AREA_LINE_LEFT) {
            if (data[0] == '{') {
                if (sscanf(data, "{%u}", &uval) == 1) {
                    temp_area.corridor_width = uval & 0xffff;
                    for (i = 0; i <= len; i++)
                        if (data[i] == '}')
                            break;
                    uval = i+1;
                    for (i = 0; i <= (int)(len-uval); i++)
                        data[i] = data[i+uval]; // delete corridor width
                }
                else {
                    temp_area.corridor_width = 0;
                    return;
                }
            }
            else {
                temp_area.corridor_width = 0;
            }
        }
        else {
            temp_area.corridor_width = 0;
        }
    }
    else {
        return;
    }

    memcpy(&(p_station->aprs_symbol.area_object), &temp_area, sizeof(AprsAreaObject));

}



/*
 *  Extract probability_max data from APRS info field: "Pmax1.23,"
 *  Please note the ending comma.  We use it to delimit the field.
 */
int extract_probability_max(char *info, char *prob_max, int prob_max_size) {
    int len,done;
    char *c;
    char *d;



    len = (int)strlen(info);
    if (len < 6) {          // Too short
        prob_max[0] = '\0';
        return(0);
    }

    c = strstr(info,"Pmax");
    if (c == NULL) {        // Pmax not found
        prob_max[0] = '\0';
        return(0);
    }

    c = c+4;    // Skip the Pmax part
    // Find the ending comma
    d = c;
    done = 0;
    while (!done) {
        if (*d == ',') {    // We're done
            done++;
        }
        else {
            d++;
        }

        // Check for string too long
        if ( ((d-c) > 10) && !done) {    // Something is wrong, we should be done by now
            prob_max[0] = '\0';
            return(0);
        }
    }

    // Copy the substring across
    snprintf(prob_max,
        prob_max_size,
        "%s",
        c);
    prob_max[d-c] = '\0';
    prob_max[10] = '\0';    // Just to make sure

    // Delete data from data extension field 
    d++;    // Skip the comma
    done = 0;
    while (!done) {
        *(c-4) = *d;
        if (*d == '\0')
            done++;
        c++;
        d++;
    }
 
    return(1);
}




/*
 *  Extract probability_min data from APRS info field: "Pmin1.23,"
 *  Please note the ending comma.  We use it to delimit the field.
 */
int extract_probability_min(char *info, char *prob_min, int prob_min_size) {
    int len,done;
    char *c;
    char *d;

 
    len = (int)strlen(info);
    if (len < 6) {          // Too short
        prob_min[0] = '\0';
        return(0);
    }

    c = strstr(info,"Pmin");
    if (c == NULL) {        // Pmin not found
        prob_min[0] = '\0';
        return(0);
    }

    c = c+4;    // Skip the Pmin part
    // Find the ending comma
    d = c;
    done = 0;
    while (!done) {
        if (*d == ',') {    // We're done
            done++;
        }
        else {
            d++;
        }

        // Check for string too long
        if ( ((d-c) > 10) && !done) {    // Something is wrong, we should be done by now
            prob_min[0] = '\0';
            return(0);
        }
    }

    // Copy the substring across
    xastir_snprintf(prob_min,
        prob_min_size,
        "%s",
        c);
    prob_min[d-c] = '\0';
    prob_min[10] = '\0';    // Just to make sure

    // Delete data from data extension field 
    d++;    // Skip the comma
    done = 0;
    while (!done) {
        *(c-4) = *d;
        if (*d == '\0')
            done++;
        c++;
        d++;
    }

    return(1);
}




/*
 *  Extract signpost data from APRS info field: "{123}", an APRS data extension
 *  Format can be {1}, {12}, or {123}.  Letters or digits are ok.
 */
int extract_signpost(char *info, char *signpost) {
    int i,found,len,done;

//0123456
//{1}
//{12}
//{121}

    found=0;
    len = (int)strlen(info);
    if ( (len > 2)
            && (info[0] == '{')
            && ( (info[2] == '}' ) || (info[3] == '}' ) || (info[4] == '}' ) ) ) {

        i = 1;
        done = 0;
        while (!done) {                 // Snag up to three digits
            if (info[i] == '}') {       // We're done
                found = i;              // found = position of '}' character
                done++;
            }
            else {
                signpost[i-1] = info[i];
            }

            i++;

            if ( (i > 4) && !done) {    // Something is wrong, we should be done by now
                done++;
                signpost[0] = '\0';
                return(0);
            }
        }
        substr(signpost,info+1,found-1);
        found++;
        for (i=0;i<=len-found;i++) {    // delete omnidf from data extension field
            info[i] = info[i+found];
        }
        return(1);
    }
    else {
        signpost[0] = '\0';
        return(0);
    }
}



// is_altnet()
//
// Returns true if station fits the current altnet description.
//
int is_altnet(AprsDataRow *p_station) {
    char temp_altnet_call[20+1];
    char temp2[20+1];
    char *net_ptr;
    int  altnet_match;
    int  result;


    // Snag a possible altnet call out of the record for later use
    if (p_station->node_path_ptr != NULL)
        substr(temp_altnet_call, p_station->node_path_ptr, MAX_CALLSIGN);
    else
        temp_altnet_call[0] = '\0';

    // Save for later
    snprintf(temp2,
        sizeof(temp2),
        "%s",
        temp_altnet_call);

    if ((net_ptr = strchr(temp_altnet_call, ',')))
        *net_ptr = '\0';    // Chop the string at the first ',' character

    for (altnet_match = (int)strlen(altnet_call); altnet && altnet_call[altnet_match-1] == '*'; altnet_match--);

    result = (!strncmp(temp_altnet_call, altnet_call, (size_t)altnet_match)
                 || !strcmp(temp_altnet_call, "local")
                 || !strncmp(temp_altnet_call, "SPC", 3)
                 || !strcmp(temp_altnet_call, "SPECL")
                 || ( is_my_station(p_station) ) ) ;  // It's my callsign/SSID

    return(result);
}



int is_num_or_sp(char ch) 
{
    return((int)((ch >= '0' && ch <= '9') || ch == ' '));
}


char *get_time(char *time_here) {
    struct tm *time_now;
    time_t timenw;

    (void)time(&timenw);
    time_now = localtime(&timenw);
    (void)strftime(time_here,MAX_TIME,"%m%d%Y%H%M%S",time_now);
    return(time_here);
}


static void clear_area(AprsDataRow *p_station) {
    p_station->aprs_symbol.area_object.type           = AREA_NONE;
    p_station->aprs_symbol.area_object.color          = AREA_GRAY_LO;
    p_station->aprs_symbol.area_object.sqrt_lat_off   = 0;
    p_station->aprs_symbol.area_object.sqrt_lon_off   = 0;
    p_station->aprs_symbol.area_object.corridor_width = 0;
}




// Check for valid overlay characters:  'A-Z', '0-9', and 'a-j'.  If
// 'a-j', it's from a compressed posit, and we need to convert it to
// '0-9'.
void overlay_symbol(char symbol, char data, AprsDataRow *fill) {

    if ( data != '/' && data !='\\') {  // Symbol overlay

        if (data >= 'a' && data <= 'j') {
            // Found a compressed posit numerical overlay
            data = data - 'a'+'0';  // Convert to a digit
        }
        if ( (data >= '0' && data <= '9')
                || (data >= 'A' && data <= 'Z') ) {
            // Found normal overlay character
            fill->aprs_symbol.aprs_type = '\\';
            fill->aprs_symbol.special_overlay = data;
        }
        else {
            // Bad overlay character.  Don't use it.  Insert the
            // normal alternate table character instead.
            fill->aprs_symbol.aprs_type = '\\';
            fill->aprs_symbol.special_overlay='\0';
        }
    }
    else {    // No overlay character
        fill->aprs_symbol.aprs_type = data;
        fill->aprs_symbol.special_overlay='\0';
    }
    fill->aprs_symbol.aprs_symbol = symbol;
}


/*
 *  Extract Time from begin of line      [APRS Reference, chapter 6]
 *
 * If a time string is found in "data", it is deleted from the
 * beginning of the string.
 */
int extract_time(AprsDataRow *p_station, char *data, int type) {
    int len, i;
    int ok = 0;

    // todo: better check of time data ranges
    len = (int)strlen(data);
    if (type == APRS_WX2) {
        // 8 digit time from stand-alone positionless weather stations...
        if (len > 8) {
            // MMDDHHMM   zulu time
            // MM 01-12         todo: better check of time data ranges
            // DD 01-31
            // HH 01-23
            // MM 01-59
            ok = 1;
            for (i=0;ok && i<8;i++)
                if (!isdigit((int)data[i]))
                    ok = 0;
            if (ok) {
//                substr(p_station->station_time,data+2,6);
//                p_station->station_time_type = 'z';
                for (i=0;i<=len-8;i++)         // delete time from data
                    data[i] = data[i+8];
            }
        }
    }
    else {
        if (len > 6) {
            // Status messages only with optional zulu format
            // DK7IN: APRS ref says one of 'z' '/' 'h', but I found 'c' at HB9TJM-8   ???
            if (toupper(data[6])=='Z' || data[6]=='/' || toupper(data[6])=='H')
                ok = 1;
            for (i=0;ok && i<6;i++)
                if (!isdigit((int)data[i]))
                    ok = 0;
            if (ok) {
//                substr(p_station->station_time,data,6);
//                p_station->station_time_type = data[6];
                for (i=0;i<=len-7;i++)         // delete time from data
                    data[i] = data[i+7];
            }
        }
    }
    return(ok);
}



// Breaks up a string into substrings using comma as the delimiter.
// Makes each entry in the array of char ptrs point to one
// substring.  Modifies incoming string and cptr[] array.  Send a
// character constant string to it and you'll get an instant
// segfault (the function can't modify a char constant string).
//
void split_string( char *data, char *cptr[], int max ) {
  int ii;
  char *temp;
  char *current = data;


  // NULL each char pointer
  for (ii = 0; ii < max; ii++) {
    cptr[ii] = NULL;
  }

  // Save the beginning substring address
  cptr[0] = current;

  for (ii = 1; ii < max; ii++) {
    temp = strchr(current,',');  // Find next comma

    if(!temp) { // No commas found 
      return; // All done with string
    }

    // Store pointer to next substring in array
    cptr[ii] = &temp[1];
    current  = &temp[1];

    // Overwrite comma with end-of-string char and bump pointer by
    // one.
    temp[0] = '\0';
  }
}



/* check data format    123 ___ ... */
// We wish to count how many ' ' or '.' characters we find.  If it
// equals zero or the field width, it might be a weather field.  If
// not, then it might be part of a comment field or something else.
//
int is_weather_data(char *data, int len) {
    int ok = 1;
    int i;
    int count = 0;

    for (i=0;ok && i<len;i++)
        if (!is_aprs_chr(data[i]))
            ok = 0;

    // Count filler characters.  Must equal zero or field width to
    // be a weather field.  There doesn't appear to be a case where
    // a single period is allowed in any weather-related fields.
    //
    for (i=0;ok && i<len;i++) {
        if (data[i] == ' ' || data[i] == '.') {
            count++;
        }
    }
    if (count != 0 && count != len) {
        ok = 0;
    }

    return(ok);
}






/* convert latitude from string to long with 1/100 sec resolution */
//
// Input is in [D]DMM.MM[MM]N format (degrees/decimal
// minutes/direction)
//
long convert_lat_s2l(char *lat) {      /* N=0°, Ctr=90°, S=180° */
    long centi_sec;
    char copy[15];
    char n[15];
    char *p;
    char offset;


    // Find the decimal point if present
    p = strstr(lat, ".");

    if (p == NULL)  // No decimal point found
        return(0l);

    offset = p - lat;   // Arithmetic on pointers
    switch (offset) {
        case 0:     // .MM[MM]N
            return(0l); // Bad, no degrees or minutes
            break;
        case 1:     // M.MM[MM]N
            return(0l); // Bad, no degrees
            break;
        case 2:     // MM.MM[MM]N
            return(0l); // Bad, no degrees
            break;
        case 3:     // DMM.MM[MM]N
            xastir_snprintf(copy,
                sizeof(copy),
                "0%s",  // Add a leading '0'
                lat);
            break;
        case 4:     // DDMM.MM[MM]N
            xastir_snprintf(copy,
                sizeof(copy),
                "%s",   // Copy verbatim
                lat);
            break;
        default:
            break;
    }

    copy[14] = '\0';
    centi_sec=0l;
    if (copy[4]=='.'
            && (   (char)toupper((int)copy[ 5])=='N'
                || (char)toupper((int)copy[ 6])=='N'
                || (char)toupper((int)copy[ 7])=='N'
                || (char)toupper((int)copy[ 8])=='N'
                || (char)toupper((int)copy[ 9])=='N'
                || (char)toupper((int)copy[10])=='N'
                || (char)toupper((int)copy[11])=='N'
                || (char)toupper((int)copy[ 5])=='S'
                || (char)toupper((int)copy[ 6])=='S'
                || (char)toupper((int)copy[ 7])=='S'
                || (char)toupper((int)copy[ 8])=='S'
                || (char)toupper((int)copy[ 9])=='S'
                || (char)toupper((int)copy[10])=='S'
                || (char)toupper((int)copy[11])=='S')) {

        substr(n, copy, 2);       // degrees
        centi_sec=atoi(n)*60*60*100;

        substr(n, copy+2, 2);     // minutes
        centi_sec += atoi(n)*60*100;

        substr(n, copy+5, 4);     // fractional minutes
        // Keep the fourth digit if present, as it resolves to 0.6
        // of a 1/100 sec resolution.  Two counts make one count in
        // the Xastir coordinate system.
 
        // Extend the digits to full precision by adding zeroes on
        // the end.
        strncat(n, "0000", sizeof(n) - strlen(n));

        // Get rid of the N/S character
        if (!isdigit((int)n[2]))
            n[2] = '0';
        if (!isdigit((int)n[3]))
            n[3] = '0';

        // Terminate substring at the correct digit
        n[4] = '\0';
//fprintf(stderr,"Lat: %s\n", n);

        // Add 0.5 (Poor man's rounding)
        centi_sec += (long)((atoi(n) * 0.6) + 0.5);

        if (       (char)toupper((int)copy[ 5])=='N'
                || (char)toupper((int)copy[ 6])=='N'
                || (char)toupper((int)copy[ 7])=='N'
                || (char)toupper((int)copy[ 8])=='N'
                || (char)toupper((int)copy[ 9])=='N'
                || (char)toupper((int)copy[10])=='N'
                || (char)toupper((int)copy[11])=='N') {
            centi_sec = -centi_sec;
        }

        centi_sec += 90*60*60*100;
    }
    return(centi_sec);
}





/* convert longitude from string to long with 1/100 sec resolution */
//
// Input is in [DD]DMM.MM[MM]W format (degrees/decimal
// minutes/direction).
//
long convert_lon_s2l(char *lon) {     /* W=0°, Ctr=180°, E=360° */
    long centi_sec;
    char copy[16];
    char n[16];
    char *p;
    char offset;


    // Find the decimal point if present
    p = strstr(lon, ".");

    if (p == NULL)  // No decimal point found
        return(0l);

    offset = p - lon;   // Arithmetic on pointers
    switch (offset) {
        case 0:     // .MM[MM]N
            return(0l); // Bad, no degrees or minutes
            break;
        case 1:     // M.MM[MM]N
            return(0l); // Bad, no degrees
            break;
        case 2:     // MM.MM[MM]N
            return(0l); // Bad, no degrees
            break;
        case 3:     // DMM.MM[MM]N
            xastir_snprintf(copy,
                sizeof(copy),
                "00%s",  // Add two leading zeroes
                lon);
            break;
        case 4:     // DDMM.MM[MM]N
            xastir_snprintf(copy,
                sizeof(copy),
                "0%s",   // Add leading '0'
                lon);
            break;
        case 5:     // DDDMM.MM[MM]N
            xastir_snprintf(copy,
                sizeof(copy),
                "%s",   // Copy verbatim
                lon);
            break;
        default:
            break;
    }

    copy[15] = '\0';
    centi_sec=0l;
    if (copy[5]=='.'
            && (   (char)toupper((int)copy[ 6])=='W'
                || (char)toupper((int)copy[ 7])=='W'
                || (char)toupper((int)copy[ 8])=='W'
                || (char)toupper((int)copy[ 9])=='W'
                || (char)toupper((int)copy[10])=='W'
                || (char)toupper((int)copy[11])=='W'
                || (char)toupper((int)copy[12])=='W'
                || (char)toupper((int)copy[ 6])=='E'
                || (char)toupper((int)copy[ 7])=='E'
                || (char)toupper((int)copy[ 8])=='E'
                || (char)toupper((int)copy[ 9])=='E'
                || (char)toupper((int)copy[10])=='E'
                || (char)toupper((int)copy[11])=='E'
                || (char)toupper((int)copy[12])=='E')) {

        substr(n,copy,3);    // degrees 013
        centi_sec=atoi(n)*60*60*100;

        substr(n,copy+3,2);  // minutes 26
        centi_sec += atoi(n)*60*100;
        // 01326.66E  01326.660E

        substr(n,copy+6,4);  // fractional minutes 66E 660E or 6601
        // Keep the fourth digit if present, as it resolves to 0.6
        // of a 1/100 sec resolution.  Two counts make one count in
        // the Xastir coordinate system.
 
        // Extend the digits to full precision by adding zeroes on
        // the end.
        strncat(n, "0000", sizeof(n) - strlen(n));

        // Get rid of the E/W character
        if (!isdigit((int)n[2]))
            n[2] = '0';
        if (!isdigit((int)n[3]))
            n[3] = '0';

        n[4] = '\0';    // Make sure substring is terminated
//fprintf(stderr,"Lon: %s\n", n);

        // Add 0.5 (Poor man's rounding)
        centi_sec += (long)((atoi(n) * 0.6) + 0.5);

        if (       (char)toupper((int)copy[ 6])=='W'
                || (char)toupper((int)copy[ 7])=='W'
                || (char)toupper((int)copy[ 8])=='W'
                || (char)toupper((int)copy[ 9])=='W'
                || (char)toupper((int)copy[10])=='W'
                || (char)toupper((int)copy[11])=='W'
                || (char)toupper((int)copy[12])=='W') {
            centi_sec = -centi_sec;
        }

        centi_sec +=180*60*60*100;;
    }
    return(centi_sec);
}



APRS_Symbol *id_callsign(char *call_sign, char * to_call) {
    char *ptr;
    char *id = "/aUfbYX's><OjRkv";
    char hold[MAX_CALLSIGN+1];
    int index;
    static APRS_Symbol symbol;

    symbol.aprs_symbol = '/';
    symbol.special_overlay = '\0';
    symbol.aprs_type ='/';
    ptr=strchr(call_sign,'-');
    if(ptr!=NULL)                      /* get symbol from SSID */
        if((index=atoi(ptr+1))<= 15)
            symbol.aprs_symbol = id[index];

    if (strncmp(to_call, "GPS", 3) == 0 || strncmp(to_call, "SPC", 3) == 0 || strncmp(to_call, "SYM", 3) == 0) 
    {
        substr(hold, to_call+3, 3);
        if ((ptr = strpbrk(hold, "->,")) != NULL)
            *ptr = '\0';

        if (strlen(hold) >= 2) {
            switch (hold[0]) {
                case 'A':
                    symbol.aprs_type = '\\';

                case 'P':
                    if (('0' <= hold[1] && hold[1] <= '9') || ('A' <= hold[1] && hold[1] <= 'Z'))
                        symbol.aprs_symbol = hold[1];

                    break;

                case 'O':
                    symbol.aprs_type = '\\';

                case 'B':
                    switch (hold[1]) {
                        case 'B':
                            symbol.aprs_symbol = '!';
                            break;
                        case 'C':
                            symbol.aprs_symbol = '"';
                            break;
                        case 'D':
                            symbol.aprs_symbol = '#';
                            break;
                        case 'E':
                            symbol.aprs_symbol = '$';
                            break;
                        case 'F':
                            symbol.aprs_symbol = '%';
                            break;
                        case 'G':
                            symbol.aprs_symbol = '&';
                            break;
                        case 'H':
                            symbol.aprs_symbol = '\'';
                            break;
                        case 'I':
                            symbol.aprs_symbol = '(';
                            break;
                        case 'J':
                            symbol.aprs_symbol = ')';
                            break;
                        case 'K':
                            symbol.aprs_symbol = '*';
                            break;
                        case 'L':
                            symbol.aprs_symbol = '+';
                            break;
                        case 'M':
                            symbol.aprs_symbol = ',';
                            break;
                        case 'N':
                            symbol.aprs_symbol = '-';
                            break;
                        case 'O':
                            symbol.aprs_symbol = '.';
                            break;
                        case 'P':
                            symbol.aprs_symbol = '/';
                            break;
                    }
                    break;

                case 'D':
                    symbol.aprs_type = '\\';

                case 'H':
                    switch (hold[1]) {
                        case 'S':
                            symbol.aprs_symbol = '[';
                            break;
                        case 'T':
                            symbol.aprs_symbol = '\\';
                            break;
                        case 'U':
                            symbol.aprs_symbol = ']';
                            break;
                        case 'V':
                            symbol.aprs_symbol = '^';
                            break;
                        case 'W':
                            symbol.aprs_symbol = '_';
                            break;
                        case 'X':
                            symbol.aprs_symbol = '`';
                            break;
                    }
                    break;

                case 'N':
                    symbol.aprs_type = '\\';

                case 'M':
                    switch (hold[1]) {
                        case 'R':
                            symbol.aprs_symbol = ':';
                            break;
                        case 'S':
                            symbol.aprs_symbol = ';';
                            break;
                        case 'T':
                            symbol.aprs_symbol = '<';
                            break;
                        case 'U':
                            symbol.aprs_symbol = '=';
                            break;
                        case 'V':
                            symbol.aprs_symbol = '>';
                            break;
                        case 'W':
                            symbol.aprs_symbol = '?';
                            break;
                        case 'X':
                            symbol.aprs_symbol = '@';
                            break;
                    }
                    break;

                case 'Q':
                    symbol.aprs_type = '\\';

                case 'J':
                    switch (hold[1]) {
                        case '1':
                            symbol.aprs_symbol = '{';
                            break;
                        case '2':
                            symbol.aprs_symbol = '|';
                            break;
                        case '3':
                            symbol.aprs_symbol = '}';
                            break;
                        case '4':
                            symbol.aprs_symbol = '~';
                            break;
                    }
                    break;

                case 'S':
                    symbol.aprs_type = '\\';

                case 'L':
                    if ('A' <= hold[1] && hold[1] <= 'Z')
                        symbol.aprs_symbol = tolower((int)hold[1]);

                    break;
            }
            if (hold[2]) {
                if (hold[2] >= 'a' && hold[2] <= 'j') {
                    // Compressed mode numeric overlay
                    symbol.special_overlay = hold[2] - 'a';
                }
                else if ( (hold[2] >= '0' && hold[2] <= '9')
                        || (hold[2] >= 'A' && hold[2] <= 'Z') ) {
                    // Normal overlay character
                    symbol.special_overlay = hold[2];
                }
                else {
                    // Bad overlay character found
                    symbol.special_overlay = '\0';
                }
            }
            else {
                // No overlay character found
                symbol.special_overlay = '\0';
            }
        }
    }
    return(&symbol);
}



/*
 * See if position is defined
 * 90°N 180°W (0,0 in internal coordinates) is our undefined position
 * 0N/0E is excluded from trails, could be excluded from map (#define ACCEPT_0N_0E)
 */

int position_defined(long lat, long lon, int strict) {

    if (lat == 0l && lon == 0l)
        return(0);              // undefined location
#ifndef ACCEPT_0N_0E
    if (strict)
#endif  // ACCEPT_0N_0E
        if (lat == 90*60*60*100l && lon == 180*60*60*100l)      // 0N/0E
            return(0);          // undefined location
    return(1);
}


//
//  Extract data for $GPRMC, it fails if there is no position!!
//
// GPRMC,UTC-Time,status(A/V),lat,N/S,lon,E/W,SOG,COG,UTC-Date,Mag-Var,E/W,Fix-Quality[*CHK]
// GPRMC,hhmmss[.sss],{A|V},ddmm.mm[mm],{N|S},dddmm.mm[mm],{E|W},[dd]d.d[ddddd],[dd]d.d[d],ddmmyy,[ddd.d],[{E|W}][,A|D|E|N|S][*CHK]
//
// The last field before the checksum is entirely optional, and in
// fact first appeared in NMEA 2.3 (fairly recently).  Most GPS's do
// not currently put out that field.  The field may be null or
// nonexistent including the comma.  Only "A" or "D" are considered
// to be active and reliable fixes if this field is present.
// Fix-Quality:
//  A: Autonomous
//  D: Differential
//  E: Estimated
//  N: Not Valid
//  S: Simulator
//
// $GPRMC,081836,A,3751.65,S,14507.36,E,000.0,360.0,130998,011.3,E*62
// $GPRMC,104748.821,A,4301.1492,N,08803.0374,W,0.085048,102.36,010605,,*1A
// $GPRMC,104749.821,A,4301.1492,N,08803.0377,W,0.054215,74.60,010605,,*2D
//
int extract_RMC(AprsDataRow *p_station, char *data, char *call_sign, char *path, int *num_digits) {
    char temp_data[40]; // short term string storage, MAX_CALLSIGN, ...  ???
    char lat_s[20];
    char long_s[20];
    int ok;
    char *Substring[12];  // Pointers to substrings parsed by split_string()
    char temp_string[MAX_MESSAGE_LENGTH+1];
    char temp_char;


    // should we copy it before processing? it changes data: ',' gets substituted by '\0' !!
    ok = 0; // Start out as invalid.  If we get enough info, we change this to a 1.

    if ( (data == NULL) || (strlen(data) < 34) ) {  // Not enough data to parse position from.
        return(ok);
    }

    p_station->record_type = NORMAL_GPS_RMC;
    // Create a timestamp from the current time
    // get_time saves the time in temp_data
    xastir_snprintf(p_station->pos_time,
        sizeof(p_station->pos_time),
        "%s",
        get_time(temp_data));
    p_station->flag &= (~ST_MSGCAP);    // clear "message capable" flag

    /* check aprs type on call sign */
    p_station->aprs_symbol = *id_callsign(call_sign, path);

    // Make a copy of the incoming data.  The string passed to
    // split_string() gets destroyed.
    xastir_snprintf(temp_string,
        sizeof(temp_string),
        "%s",
        data);
    split_string(temp_string, Substring, 12);

    // The Substring[] array contains pointers to each substring in
    // the original data string.

// GPRMC,034728,A,5101.016,N,11359.464,W,000.0,284.9,110701,018.0,E*7D
//   0     1    2    3     4    5      6   7    8      9     10    11

    if (Substring[0] == NULL)   // No GPRMC string
        return(ok);

    if (Substring[1] == NULL)   // No time string
        return(ok);

    if (Substring[2] == NULL)   // No valid fix char
        return(ok);

    if (Substring[2][0] != 'A' && Substring[2][0] != 'V')
        return(ok);
// V is a warning but we can get good data still ?
// DK7IN: got no position with 'V' !

    if (Substring[3] == NULL)   // No latitude string
        return(ok);

    if (Substring[4] == NULL)   // No latitude N/S
        return(ok);

// Need to check lat_s for validity here.  Note that some GPS's put out another digit of precision
// (4801.1234) or leave one out (4801.12).  Next character after digits should be a ','

    // Count digits after the decimal point for latitude
    if (strchr(Substring[3],'.')) {
        *num_digits = strlen(Substring[3]) - (int)(strchr(Substring[3],'.') - Substring[3]) - 1;
    }
    else {
        *num_digits = 0;
    }

    temp_char = toupper((int)Substring[4][0]);

    if (temp_char != 'N' && temp_char != 'S')   // Bad N/S
        return(ok);

    xastir_snprintf(lat_s,
        sizeof(lat_s),
        "%s%c",
        Substring[3],
        temp_char);

    if (Substring[5] == NULL)   // No longitude string
        return(ok);

    if (Substring[6] == NULL)   // No longitude E/W
        return(ok);

// Need to check long_s for validity here.  Should be all digits.  Note that some GPS's put out another
// digit of precision.  (12201.1234).  Next character after digits should be a ','

    temp_char = toupper((int)Substring[6][0]);

    if (temp_char != 'E' && temp_char != 'W')   // Bad E/W
        return(ok);

    xastir_snprintf(long_s,
        sizeof(long_s),
        "%s%c",
        Substring[5],
        temp_char);

    p_station->coord_lat = convert_lat_s2l(lat_s);
    p_station->coord_lon = convert_lon_s2l(long_s);

    // If we've made it this far, We have enough for a position now!
    ok = 1;

    // Now that we have a basic position, let's see what other data
    // can be parsed from the packet.  The rest of it can still be
    // corrupt, so we're proceeding carefully under yellow alert on
    // impulse engines only.

// GPRMC,034728,A,5101.016,N,11359.464,W,000.0,284.9,110701,018.0,E*7D
//   0     1    2    3     4    5      6   7    8      9     10    11

    if (Substring[7] == NULL) { // No speed string
        p_station->speed[0] = '\0'; // No speed available
        return(ok);
    }
    else {
        xastir_snprintf(p_station->speed,
            MAX_SPEED,
            "%s",
            Substring[7]);
        // Is it always knots, otherwise we need a conversion!
    }

    if (Substring[8] == NULL) { // No course string
        xastir_snprintf(p_station->course,
            sizeof(p_station->course),
            "000.0");  // No course available
        return(ok);
    }
    else {
        xastir_snprintf(p_station->course,
            MAX_COURSE,
            "%s",
            Substring[8]);
    }

    return(ok);
}


//
//  Extract data for $GPGLL
//
// $GPGLL,4748.811,N,12219.564,W,033850,A*3C
// lat, long, UTCtime in hhmmss, A=Valid, checksum
//
// GPGLL,4748.811,N,12219.564,W,033850,A*3C
//   0       1    2      3    4    5   6
//
int extract_GLL(AprsDataRow *p_station,char *data,char *call_sign, char *path, int *num_digits) {
    char temp_data[40]; // short term string storage, MAX_CALLSIGN, ...  ???
    char lat_s[20];
    char long_s[20];
    int ok;
    char *Substring[7];  // Pointers to substrings parsed by split_string()
    char temp_string[MAX_MESSAGE_LENGTH+1];
    char temp_char;


    ok = 0; // Start out as invalid.  If we get enough info, we change this to a 1.
  
    if ( (data == NULL) || (strlen(data) < 28) )  // Not enough data to parse position from.
        return(ok);

    p_station->record_type = NORMAL_GPS_GLL;
    // Create a timestamp from the current time
    // get_time saves the time in temp_data
    xastir_snprintf(p_station->pos_time,
        sizeof(p_station->pos_time),
        "%s",
        get_time(temp_data));
    p_station->flag &= (~ST_MSGCAP);    // clear "message capable" flag

    /* check aprs type on call sign */
    p_station->aprs_symbol = *id_callsign(call_sign, path);

    // Make a copy of the incoming data.  The string passed to
    // split_string() gets destroyed.
    xastir_snprintf(temp_string,
        sizeof(temp_string),
        "%s",
        data);
    split_string(temp_string, Substring, 7);

    // The Substring[] array contains pointers to each substring in
    // the original data string.

    if (Substring[0] == NULL)  // No GPGGA string
        return(ok);

    if (Substring[1] == NULL)  // No latitude string
        return(ok);

    if (Substring[2] == NULL)   // No N/S string
        return(ok);

    if (Substring[3] == NULL)   // No longitude string
        return(ok);

    if (Substring[4] == NULL)   // No E/W string
        return(ok);

    temp_char = toupper((int)Substring[2][0]);
    if (temp_char != 'N' && temp_char != 'S')
        return(ok);

    xastir_snprintf(lat_s,
        sizeof(lat_s),
        "%s%c",
        Substring[1],
        temp_char);
// Need to check lat_s for validity here.  Note that some GPS's put out another digit of precision
// (4801.1234).  Next character after digits should be a ','

    // Count digits after the decimal point for latitude
    if (strchr(Substring[1],'.')) {
        *num_digits = strlen(Substring[1]) - (int)(strchr(Substring[1],'.') - Substring[1]) - 1;
    }
    else {
        *num_digits = 0;
    }

    temp_char = toupper((int)Substring[4][0]);
    if (temp_char != 'E' && temp_char != 'W')
        return(ok);

    xastir_snprintf(long_s,
        sizeof(long_s),
        "%s%c",
        Substring[3],
        temp_char);
// Need to check long_s for validity here.  Should be all digits.  Note that some GPS's put out another
// digit of precision.  (12201.1234).  Next character after digits should be a ','

    p_station->coord_lat = convert_lat_s2l(lat_s);
    p_station->coord_lon = convert_lon_s2l(long_s);
    ok = 1; // We have enough for a position now

    xastir_snprintf(p_station->course,
        sizeof(p_station->course),
        "000.0");  // Fill in with dummy values
    p_station->speed[0] = '\0';        // Fill in with dummy values

    // A is valid, V is a warning but we can get good data still?
    // We don't currently check the data valid flag.

    return(ok);
}



//
//  Extract data for $GPGGA
//
// GPGGA,UTC-Time,lat,N/S,long,E/W,GPS-Quality,nsat,HDOP,MSL-Meters,M,Geoidal-Meters,M,DGPS-Data-Age(seconds),DGPS-Ref-Station-ID[*CHK]
// GPGGA,hhmmss[.sss],ddmm.mm[mm],{N|S},dddmm.mm[mm],{E|W},{0-8},dd,[d]d.d,[-dddd]d.d,M,[-ddd]d.d,M,[dddd.d],[dddd][*CHK]
//
// GPS-Quality:
//  0: Invalid Fix
//  1: GPS Fix
//  2: DGPS Fix
//  3: PPS Fix
//  4: RTK Fix
//  5: Float RTK Fix
//  6: Estimated (dead-reckoning) Fix
//  7: Manual Input Mode
//  8: Simulation Mode
//
// $GPGGA,170834,4124.8963,N,08151.6838,W,1,05,1.5,280.2,M,-34.0,M,,,*75 
// $GPGGA,104438.833,4301.1439,N,08803.0338,W,1,05,1.8,185.8,M,-34.2,M,0.0,0000*40
//
// nsat=Number of Satellites being tracked
//
//
int extract_GGA(AprsDataRow *p_station,char *data,char *call_sign, char *path, int *num_digits) {
    char temp_data[40]; // short term string storage, MAX_CALLSIGN, ...  ???
    char lat_s[20];
    char long_s[20];
    int  ok;
    char *Substring[15];  // Pointers to substrings parsed by split_string()
    char temp_string[MAX_MESSAGE_LENGTH+1];
    char temp_char;
    int  temp_num;


    ok = 0; // Start out as invalid.  If we get enough info, we change this to a 1.
 
    if ( (data == NULL) || (strlen(data) < 32) )  // Not enough data to parse position from.
        return(ok);

    p_station->record_type = NORMAL_GPS_GGA;
    // Create a timestamp from the current time
    // get_time saves the time in temp_data
    xastir_snprintf(p_station->pos_time,
        sizeof(p_station->pos_time),
        "%s",
        get_time(temp_data));
    p_station->flag &= (~ST_MSGCAP);    // clear "message capable" flag

    /* check aprs type on call sign */
    p_station->aprs_symbol = *id_callsign(call_sign, path);

    // Make a copy of the incoming data.  The string passed to
    // split_string() gets destroyed.
    xastir_snprintf(temp_string,
        sizeof(temp_string),
        "%s",
        data);
    split_string(temp_string, Substring, 15);

    // The Substring[] array contains pointers to each substring in
    // the original data string.


// GPGGA,hhmmss[.sss],ddmm.mm[mm],{N|S},dddmm.mm[mm],{E|W},{0-8},dd,[d]d.d,[-dddd]d.d,M,[-ddd]d.d,M,[dddd.d],[dddd][*CHK]
//   0     1              2         3        4         5        6      7     8        9     1     1     1    1        1
//                                                                                          0     1     2    3        4

    if (Substring[0] == NULL)  // No GPGGA string
        return(ok);

    if (Substring[1] == NULL)  // No time string
        return(ok);

    if (Substring[2] == NULL)   // No latitude string
        return(ok);

    if (Substring[3] == NULL)   // No latitude N/S
        return(ok);

// Need to check lat_s for validity here.  Note that some GPS's put out another digit of precision
// (4801.1234).  Next character after digits should be a ','

    // Count digits after the decimal point for latitude
    if (strchr(Substring[2],'.')) {
        *num_digits = strlen(Substring[2]) - (int)(strchr(Substring[2],'.') - Substring[2]) - 1;
    }
    else {
        *num_digits = 0;
    }

    temp_char = toupper((int)Substring[3][0]);

    if (temp_char != 'N' && temp_char != 'S')   // Bad N/S
        return(ok);

    xastir_snprintf(lat_s,
        sizeof(lat_s),
        "%s%c",
        Substring[2],
        temp_char);

    if (Substring[4] == NULL)   // No longitude string
        return(ok);

    if (Substring[5] == NULL)   // No longitude E/W
        return(ok);

// Need to check long_s for validity here.  Should be all digits.  Note that some GPS's put out another
// digit of precision.  (12201.1234).  Next character after digits should be a ','

    temp_char = toupper((int)Substring[5][0]);

    if (temp_char != 'E' && temp_char != 'W')   // Bad E/W
        return(ok);

    xastir_snprintf(long_s,
        sizeof(long_s),
        "%s%c",
        Substring[4],
        temp_char);

    p_station->coord_lat = convert_lat_s2l(lat_s);
    p_station->coord_lon = convert_lon_s2l(long_s);

    // If we've made it this far, We have enough for a position now!
    ok = 1;


    // Now that we have a basic position, let's see what other data
    // can be parsed from the packet.  The rest of it can still be
    // corrupt, so we're proceeding carefully under yellow alert on
    // impulse engines only.

    // Check for valid fix {
    if (Substring[6] == NULL
            || Substring[6][0] == '0'      // Fix quality
            || Substring[7] == NULL        // Sat number
            || Substring[8] == NULL        // hdop
            || Substring[9] == NULL) {     // Altitude in meters
        p_station->sats_visible[0] = '\0'; // Store empty sats visible
        p_station->altitude[0] = '\0';;    // Store empty altitude
        return(ok); // A field between fix quality and altitude is missing
    }

// Need to check for validity of this number.  Should be 0-12?  Perhaps a few more with WAAS, GLONASS, etc?
    temp_num = atoi(Substring[7]);
    if (temp_num < 0 || temp_num > 30) {
        return(ok); // Number of satellites not valid
    }
    else {
        // Store 
        xastir_snprintf(p_station->sats_visible,
            sizeof(p_station->sats_visible),
            "%d",
            temp_num);
    }


// Check for valid number for HDOP instead of just throwing it away?


    xastir_snprintf(p_station->altitude,
        sizeof(p_station->altitude),
        "%s",
        Substring[9]); // Get altitude

// Need to check for valid altitude before conversion

    // unit is in meters, if not adjust value ???

    if (Substring[10] == NULL)  // No units for altitude
        return(ok);

    if (Substring[10][0] != 'M') {
        //fprintf(stderr,"ERROR: should adjust altitude for meters\n");
        //} else {  // Altitude units wrong.  Assume altitude bad
        p_station->altitude[0] = '\0';
    }

    return(ok);
}



static void extract_multipoints(AprsDataRow *p_station,
        char *data,
        int type,
        int remove_string) {
    // If they're in there, the multipoints start with the
    // sequence <space><rbrace><lower><digit> and end with a <lbrace>.
    // In addition, there must be no spaces in there, and there
    // must be an even number of characters (after the lead-in).

    char *p, *p2;
    int found = 0;
    char *end;
    int data_size;


    if (data == NULL) {
        return;
    }

//fprintf(stderr,"Data: %s\t\t", data);

    data_size = strlen(data);

    end = data + (strlen(data) - 7);  // 7 == 3 lead-in chars, plus 2 points
 
    p_station->num_multipoints = 0;

    /*
    for (p = data; !found && p <= end; ++p) {
        if (*p == ' ' && *(p+1) == RBRACE && islower((int)*(p+2)) && isdigit((int)*(p+3)) && 
                            (p2 = strchr(p+4, LBRACE)) != NULL && ((p2 - p) % 2) == 1) {
            found = 1;
        }
    }
    */

    // Start looking at the beginning of the data.

    p = data;

    // Look for the opening string.

    while (!found && p < end && (p = strstr(p, START_STR)) != NULL) {
        // The opening string was found. Check the following information.

        if (islower((int)*(p+2)) && g_ascii_isdigit(*(p+3)) && (p2 = strchr(p+4, LBRACE)) != NULL && ((p2 - p) % 2) == 1) {
            // It all looks good!

            found = 1;
        }
        else {
            // The following characters are not right. Advance and
            // look again.

            ++p;
        }
    }

    if (found) {
        long multiplier;
        double d;
        char *m_start = p;    // Start of multipoint string
        char ok = 1;
 
        // The second character (the lowercase) indicates additional style information,
        // such as color, line type, etc.

        p_station->style = *(p+2);

        // The third character (the digit) indicates the way the points should be
        // used. They may be used to draw a closed polygon, a series of line segments,
        // etc.

        p_station->type = *(p+3);

        // The fourth character indicates the scale of the coordinates that
        // follow. It may range from '!' to 'z'. The value represents the
        // unit of measure (1, 0.1, 0.001, etc., in degrees) used in the offsets.
        //
        // Use the following formula to convert the char to the value:
        // (10 ^ ((c - 33) / 20)) / 10000 degrees
        //
        // Finally we have to convert to Xastir units. Xastir stores coordinates
        // as hudredths of seconds. There are 360,000 of those per degree, so we
        // need to multiply by that factor so our numbers will be converted to
        // Xastir units.

        p = p + 4;

        if (*p < '!' || *p > 'z') {
            fprintf(stderr,"extract_multipoints: invalid scale character %d\n", *p);
            ok = 0; // Failure
        }
        else {

            d = (double)(*p);
            d = pow(10.0, ((d - 33) / 20)) / 10000.0 * 360000.0;
            multiplier = (long)d;
 
            ++p;

            // The remaining characters are in pairs. Each pair is the
            // offset lat and lon for one of the points. (The offset is
            // from the actual location of the object.) Convert each
            // character to its numeric value and save it.

            while (*p != LBRACE && p_station->num_multipoints < MAX_MULTIPOINTS) {
                // The characters are in the range '"' (34 decimal) to 'z' (122). They
                // encode values in the range -44 to +44. To convert to the correct
                // value 78 is subtracted from the character's value.

                int lat_val = *p - 78;
                int lon_val = *(p+1) - 78;

                // Check for correct values.

                if (lon_val < -44 || lon_val > 44 || lat_val < -44 || lat_val > 44) {
                    char temp[MAX_LINE_SIZE+1];
                    int i;

                    // Filter the string so we don't send strange
                    // chars to the xterm
                    for (i = 0; i < (int)strlen(data); i++) {
                        temp[i] = data[i] & 0x7f;
                        if ( (temp[i] < 0x20) || (temp[i] > 0x7e) )
                            temp[i] = ' ';
                    }
                    temp[strlen(data)] = '\0';
                    
                    fprintf(stderr,"extract_multipoints: invalid value in (filtered) \"%s\": %d,%d\n",
                        temp,
                        lat_val,
                        lon_val);

                    p_station->num_multipoints = 0;     // forget any points we already set
                    ok = 0; // Failure to decode
                    break;
                }

                // Malloc the storage area for this if we don't have
                // it yet.
                if (p_station->multipoint_data == NULL) {

                    p_station->multipoint_data = malloc(sizeof(AprsMultipointRow));
                    if (p_station->multipoint_data == NULL) {
                        p_station->num_multipoints = 0;
                        fprintf(stderr,"Couldn't malloc AprsMultipointRow'\n");
                        return;
                    }
                }
 
                // Add the offset to the object's position to obtain the position of the point.
                // Note that we're working in Xastir coordinates, and in North America they
                // are exactly opposite to lat/lon (larger numbers are farther east and south).
                // An offset with a positive value means that the point should be north and/or
                // west of the object, so we have to *subtract* the offset to get the correct
                // placement in Xastir coordinates.
                // TODO: Consider what we should do in the other geographic quadrants. Should we
                // check here for the correct sign of the offset? Or should the program that
                // creates the offsets take that into account?

                p_station->multipoint_data->multipoints[p_station->num_multipoints][0]
                    = p_station->coord_lon - (lon_val * multiplier);
                p_station->multipoint_data->multipoints[p_station->num_multipoints][1]
                    = p_station->coord_lat - (lat_val * multiplier);

                p += 2;
                ++p_station->num_multipoints;
            }   // End of while loop
        }

        if (ok && remove_string) {
            // We've successfully decoded a multipoint object?
            // Remove the multipoint strings (and the sequence
            // number at the end if present) from the data string.
            // m_start points to the first character (a space).  'p'
            // should be pointing at the LBRACE character.

            // Make 'p' point to just after the end of the chars
            while ( (p < data+strlen(data)) && (*p != ' ') ) {
               p++;
            }
            // The string that 'p' points to now may be empty

            // Truncate "data" at the starting brace - 1
            *m_start = '\0';

            // Now we have two strings inside "data".  Copy the 2nd
            // string directly onto the end of the first.
            strncat(data, p, data_size+1);

            // The multipoint string and sequence number should be
            // erased now from "data".
//fprintf(stderr,"New Data: %s\n", data);
        }

    }

}




// Returns time in seconds since the Unix epoch.
//
time_t sec_now(void) {
    time_t timenw;
    time_t ret;

    ret = time(&timenw);
    return(ret);
}


int is_my_station(AprsDataRow *p_station) {
    // if station is owned by me (including SSID)
    return(p_station->flag & ST_MYSTATION);
}

int is_my_object_item(AprsDataRow *p_station) {
    // If object/item is owned by me (including SSID)
    return(p_station->flag & ST_MYOBJITEM);
}

/*
 *  Display text in the status line, text is removed after timeout
 */
void statusline(char *status_text,int update) {

// Maybe useful
/*
    XmTextFieldSetString (text, status_text);
    last_statusline = sec_now();    // Used for auto-ID timeout
*/
}


/*
 *  Extract text inserted by TNC X-1J4 from start of info line
 */
void extract_TNC_text(char *info) {
    int i,j,len;

    if (strncasecmp(info,"thenet ",7) == 0) {   // 1st match
        len = strlen(info)-1;
        for (i=7;i<len;i++) {
            if (info[i] == ')')
                break;
        }
        len++;
        if (i>7 && info[i] == ')' && info[i+1] == ' ') {        // found
            i += 2;
            for (j=0;i<=len;i++,j++) {
                info[j] = info[i];
            }
        }
    }
}



/*
 *  Check for valid path and change it to TAPR format
 *  Add missing asterisk for stations that we must have heard via a digi
 *  Extract port for KAM TNCs
 *  Handle igate injection ID formats: "callsign-ssid,I" & "callsign-0ssid"
 *
 * TAPR-2 Format:
 * KC2ELS-1*>SX0PWT,RELAY,WIDE:`2`$l##>/>"4)}
 *
 * AEA Format:
 * KC2ELS-1*>RELAY>WIDE>SX0PWT:`2`$l##>/>"4)}
 */

int valid_path(char *path) {
    int i,len,hops,j;
    int type,ast,allast,ins;
    char ch;


    len  = (int)strlen(path);
    type = 0;       // 0: unknown, 1: AEA '>', 2: TAPR2 ',', 3: mixed
    hops = 1;
    ast  = 0;
    allast = 0;

    // There are some multi-port TNCs that deliver the port at the end
    // of the path. For now we discard this information. If there is
    // multi-port TNC support some day, we should write the port into
    // our database.
    // KAM:        /V /H
    // KPC-9612:   /0 /1 /2
    if (len > 2 && path[len-2] == '/') {
        ch = path[len-1];
        if (ch == 'V' || ch == 'H' || ch == '0' || ch == '1' || ch == '2') {
            path[len-2] = '\0';
            len  = (int)strlen(path);
        }
    }


    // One way of adding igate injection ID is to add "callsign-ssid,I".
    // We need to remove the ",I" portion so it doesn't count as another
    // digi here.  This should be at the end of the path.
    if (len > 2 && path[len-2] == ',' && path[len-1] == 'I') {  // Found ",I"
        //fprintf(stderr,"%s\n",path);
        //fprintf(stderr,"Found ',I'\n");
        path[len-2] = '\0';
        len  = (int)strlen(path);
        //fprintf(stderr,"%s\n\n",path);
    }
    // Now look for the same thing but with a '*' character at the end.
    // This should be at the end of the path.
    if (len > 3 && path[len-3] == ',' && path[len-2] == 'I' && path[len-1] == '*') {  // Found ",I*"
        //fprintf(stderr,"%s\n",path);
        //fprintf(stderr,"Found ',I*'\n");
        path[len-3] = '\0';
        len  = (int)strlen(path);
        //fprintf(stderr,"%s\n\n",path);
    }


    // Another method of adding igate injection ID is to add a '0' in front of
    // the SSID.  For WE7U it would change to WE7U-00, for WE7U-15 it would
    // change to WE7U-015.  Take out this zero so the rest of the decoding will
    // work.  This should be at the end of the path.
    // Also look for the same thing but with a '*' character at the end.
    if (len > 6) {
        for (i=len-1; i>len-6; i--) {
            if (path[i] == '-' && path[i+1] == '0') {
                //fprintf(stderr,"%s\n",path);
                for (j=i+1; j<len; j++) {
                    path[j] = path[j+1];    // Shift everything left by one
                }
                len = (int)strlen(path);
                //fprintf(stderr,"%s\n\n",path);
            }
            // Check whether we just chopped off the '0' from "-0".
            // If so, chop off the dash as well.
            if (path[i] == '-' && path[i+1] == '\0') {
                //fprintf(stderr,"%s\tChopping off dash\n",path);
                path[i] = '\0';
                len = (int)strlen(path);
                //fprintf(stderr,"%s\n",path);
            }
            // Check for "-*", change to '*' only
            if (path[i] == '-' && path[i+1] == '*') {
                //fprintf(stderr,"%s\tChopping off dash\n",path);
                path[i] = '*';
                path[i+1] = '\0';
                len = (int)strlen(path);
                //fprintf(stderr,"%s\n",path);
            }
            // Check for "-0" or "-0*".  Change to "" or "*".
            if ( path[i] == '-' && path[i+1] == '0' ) {
                //fprintf(stderr,"%s\tShifting left by two\n",path);
                for (j=i; j<len; j++) {
                    path[j] = path[j+2];    // Shift everything left by two
                }
                len = (int)strlen(path);
                //fprintf(stderr,"%s\n",path);
            }
        }
    }


    for (i=0,j=0; i<len; i++) {
        ch = path[i];

        if (ch == '>' || ch == ',') {   // found digi call separator
            // We're at the start of a callsign entry in the path

            if (ast > 1 || (ast == 1 && i-j > 10) || (ast == 0 && (i == j || i-j > 9))) {
                return(0);              // more than one asterisk in call or wrong call size
            }
            ast = 0;                    // reset local asterisk counter
            
            j = i+1;                    // set to start of next call
            if (ch == ',')
                type |= 0x02;           // set TAPR2 flag
            else
                type |= 0x01;           // set AEA flag (found '>')
            hops++;                     // count hops
        }

        else {                          // digi call character or asterisk
            // We're in the middle of a callsign entry

            if (ch == '*') {
                ast++;                  // count asterisks in call
                allast++;               // count asterisks in path
            }
            else if ((ch <'A' || ch > 'Z')      // Not A-Z
                    && (ch <'a' || ch > 'z')    // Not a-z
                    && (ch <'0' || ch > '9')    // Not 0-9
                    && ch != '-') {
                // Note that Q-construct and internet callsigns can
                // have a-z in them, AX.25 callsigns cannot unless
                // they are in a 3rd-party packet.

                return(0);          // wrong character in path
            }
        }
    }
    if (ast > 1 || (ast == 1 && i-j > 10) || (ast == 0 && (i == j || i-j > 9))) {
        return(0);                      // more than one asterisk or wrong call size
    }

    if (type == 0x03) {
        return(0);                      // wrong format, both '>' and ',' in path
    }

    if (hops > 9) {                     // [APRS Reference chapter 3]
        return(0);                      // too much hops, destination + 0-8 digipeater addresses
    }

    if (type == 0x01) {
        int delimiters[20];
        int k = 0;
        char dest[15];
        char rest[100];

        for (i=0; i<len; i++) {
            if (path[i] == '>') {
                path[i] = ',';          // Exchange separator character
                delimiters[k++] = i;    // Save the delimiter indexes
            }
        }

        // We also need to move the destination callsign to the end.
        // AEA has them in a different order than TAPR-2 format.
        // We'll move the destination address between delimiters[0]
        // and [1] to the end of the string.

        //fprintf(stderr,"Orig. Path:%s\n",path);
        // Save the destination
        xastir_snprintf(dest,sizeof(dest),"%s",&path[delimiters[--k]+1]);
        dest[strlen(path) - delimiters[k] - 1] = '\0'; // Terminate it
        dest[14] = '\0';    // Just to make sure
        path[delimiters[k]] = '\0'; // Delete it from the original path
        //fprintf(stderr,"Destination: %s\n",dest);

        // TAPR-2 Format:
        // KC2ELS-1*>SX0PWT,RELAY,WIDE:`2`$l##>/>"4)}
        //
        // AEA Format:
        // KC2ELS-1*>RELAY>WIDE>SX0PWT:`2`$l##>/>"4)}
        //          9     15   20

        // We now need to insert the destination into the middle of
        // the string.  Save part of it in another variable first.
        xastir_snprintf(rest,
            sizeof(rest),
            "%s",
            path);
        //fprintf(stderr,"Rest:%s\n",rest);
        xastir_snprintf(path,len+1,"%s,%s",dest,rest);
        //fprintf(stderr,"New Path:%s\n",path);
    }

    if (allast < 1) {                   // try to insert a missing asterisk
        ins  = 0;
        hops = 0;

        for (i=0; i<len; i++) {

            for (j=i; j<len; j++) {             // search for separator
                if (path[j] == ',')
                    break;
            }

            if (hops > 0 && (j - i) == 5) {     // WIDE3
                if (  path[ i ] == 'W' && path[i+1] == 'I' && path[i+2] == 'D' 
                   && path[i+3] == 'E' && path[i+4] >= '0' && path[i+4] <= '9') {
                    ins = j;
                }
            }

/*
Don't do this!  It can mess up relay/wide1-1 digipeating by adding
an asterisk later in the path than the first unused digi.
            if (hops > 0 && (j - i) == 7) {     // WIDE3-2
                if (  path[ i ] == 'W' && path[i+1] == 'I' && path[i+2] == 'D' 
                   && path[i+3] == 'E' && path[i+4] >= '0' && path[i+4] <= '9'
                   && path[i+5] == '-' && path[i+6] >= '0' && path[i+6] <= '9'
                   && (path[i+4] != path[i+6]) ) {
                    ins = j;
                }
            }
*/

            if (hops > 0 && (j - i) == 6) {     // TRACE3
                if (  path[ i ] == 'T' && path[i+1] == 'R' && path[i+2] == 'A' 
                   && path[i+3] == 'C' && path[i+4] == 'E'
                   && path[i+5] >= '0' && path[i+5] <= '9') {
                    if (hops == 1)
                        ins = j;
                    else
                        ins = i-1;
                }
            }

/*
Don't do this!  It can mess up relay/wide1-1 digipeating by adding
an asterisk later in the path than the first unused digi.
            if (hops > 0 && (j - i) == 8) {     // TRACE3-2
                if (  path[ i ] == 'T' && path[i+1] == 'R' && path[i+2] == 'A' 
                   && path[i+3] == 'C' && path[i+4] == 'E' && path[i+5] >= '0'
                   && path[i+5] <= '9' && path[i+6] == '-' && path[i+7] >= '0'
                   && path[i+7] <= '9' && (path[i+5] != path[i+7]) ) {
                    if (hops == 1)
                        ins = j;
                    else
                        ins = i-1;
                }
            }
*/

            hops++;
            i = j;                      // skip to start of next call
        }
        if (ins > 0) {
            for (i=len;i>=ins;i--) {
                path[i+1] = path[i];    // generate space for '*'
                // we work on a separate path copy which is long enough to do it
            }
            path[ins] = '*';            // and insert it
        }
    }
    return(1);  // Path is good
}




char *remove_leading_spaces(char *data) {
    int i,j;
    int count;

    if (data == NULL)
        return NULL;

    if (strlen(data) == 0)
        return NULL;

    count = 0;
    // Count the leading space characters
    for (i = 0; i < (int)strlen(data); i++) {
        if (data[i] == ' ') {
            count++;
        }
        else {  // Found a non-space
            break;
        }
    }

    // Check whether entire string was spaces
    if (count == (int)strlen(data)) {
        // Empty the string
        data[0] = '\0';
    }
    else if (count > 0) {  // Found some spaces
        i = 0;
        for( j = count; j < (int)strlen(data); j++ ) {
            data[i++] = data[j];    // Move string left
        }
        data[i] = '\0'; // Terminate the new string
    }

    return(data);
}

int is_num_chr(char ch) {
    return((int)isdigit(ch));
}

char *remove_trailing_spaces(char *data) {
    int i;

    if (data == NULL)
        return NULL;

    if (strlen(data) == 0)
        return NULL;

    for(i=strlen(data)-1;i>=0;i--)
        if(data[i] == ' ')
            data[i] = '\0';
        else
            break;

        return(data);
}



char *remove_trailing_asterisk(char *data) {
    int i;

    if (data == NULL)
        return NULL;

    if (strlen(data) == 0)
        return NULL;

// Should the test here be i>=0 ??
    for(i=strlen(data)-1;i>0;i--) {
        if(data[i] == '*')
            data[i] = '\0';
    }
    return(data);
}

//--------------------------------------------------------------------
//Removes all control codes ( <0x20 or >0x7e ) from a string, including
// CR's, LF's, tab's, etc.
//
void makePrintable(char *cp) {
    int i,j;
    int len = (int)strlen(cp);
    unsigned char *ucp = (unsigned char *)cp;

    for (i=0, j=0; i<=len; i++) {
        ucp[i] &= 0x7f;                 // Clear 8th bit
        if ( ((ucp[i] >= (unsigned char)0x20) && (ucp[i] <= (unsigned char)0x7e))
              || ((char)ucp[i] == '\0') )     // Check for printable or terminating 0
            ucp[j++] = ucp[i] ;        // Copy to (possibly) new location if printable
    }
}




/*
 *  Check for a valid AX.25 call
 *      Valid calls consist of up to 6 uppercase alphanumeric characters
 *      plus optional SSID (four-bit integer)       [APRS Reference, AX.25 Reference]
 */
int valid_call(char *call) {
    int len, ok;
    int i, del, has_num, has_chr;
    char c;

    has_num = 0;
    has_chr = 0;
    ok      = 1;
    len = (int)strlen(call);

    if (len == 0)
        return(0);                              // wrong size

    while (call[0]=='c' && call[1]=='m' && call[2]=='d' && call[3]==':') {
        // Erase TNC prompts from beginning of callsign.  This may
        // not be the right place to do this, but it came in handy
        // here, so that's where I put it. -- KB6MER

        for(i=0; call[i+4]; i++)
            call[i]=call[i+4];

        call[i++]=0;
        call[i++]=0;
        call[i++]=0;
        call[i++]=0;
        len=strlen(call);

    }

    if (len > 9)
        return(0);      // Too long for valid call (6-2 max e.g. KB6MER-12)

    del = 0;
    for (i=len-2;ok && i>0 && i>=len-3;i--) {   // search for optional SSID
        if (call[i] =='-')
            del = i;                            // found the delimiter
    }
    if (del) {                                  // we have a SSID, so check it
        if (len-del == 2) {                     // 2 char SSID
            if (call[del+1] < '1' || call[del+1] > '9')                         //  -1 ...  -9
                del = 0;
        }
        else {                                  // 3 char SSID
            if (call[del+1] != '1' || call[del+2] < '0' || call[del+2] > '5')   // -10 ... -15
                del = 0;
        }
    }

    if (del)
        len = del;                              // length of base call

    for (i=0;ok && i<len;i++) {                 // check for uppercase alphanumeric
        c = call[i];

        if (c >= 'A' && c <= 'Z')
            has_chr = 1;                        // we need at least one char
        else if (c >= '0' && c <= '9')
            has_num = 1;                        // we need at least one number
        else
            ok = 0;                             // wrong character in call
    }

//    if (!has_num || !has_chr)                 // with this we also discard NOCALL etc.
    if (!has_chr)                               
        ok = 0;

    ok = (ok && strcmp(call,"NOCALL") != 0);    // check for errors
    ok = (ok && strcmp(call,"ERROR!") != 0);
    ok = (ok && strcmp(call,"WIDE")   != 0);
    ok = (ok && strcmp(call,"RELAY")  != 0);
    ok = (ok && strcmp(call,"MAIL")   != 0);

    return(ok);
}


/*
 *  Check whether callsign is mine.  "exact == 1" checks the SSID
 *  for a match as well.  "exact == 0" checks only the base
 *  callsign.
 */
int is_my_call(char *call, int exact) {
    char *p_del;
    int ok;


    // U.S. special-event callsigns can be as short as three
    // characters, any less and we don't have a valid callsign.  We
    // don't check for that restriction here though.

    if (exact) {
        // We're looking for an exact match
        ok = (int)( !strcmp(call,_aprs_mycall ) );
        //fprintf(stderr,"My exact call found: %s\n",call);
    }
    else {
        // We're looking for a similar match.  Compare only up to
        // the '-' in each (if present).
        int len1,len2;

        p_del = index(call,'-');
        if (p_del == NULL)
            len1 = (int)strlen(call);
        else
            len1 = p_del - call;

        p_del = index(_aprs_mycall,'-');
        if (p_del == NULL)
            len2 = (int)strlen(_aprs_mycall);
        else
            len2 = p_del - _aprs_mycall;

        ok = (int)(len1 == len2 && !strncmp(call,_aprs_mycall,(size_t)len1));
        //fprintf(stderr,"My base call found: %s\n",call);
    }
 
    return(ok);
}




/*
 *  Check for a valid internet name.
 *  Accept darned-near anything here as long as it is the proper
 *  length and printable.
 */
int valid_inet_name(char *name, char *info, char *origin, int origin_size) {
    int len, i, ok;
    char *ptr;
    
    len = (int)strlen(name);

    if (len > 9 || len == 0)            // max 9 printable ASCII characters
        return(0);                      // wrong size

    for (i=0;i<len;i++)
        if (!isprint((int)name[i]))
            return(0);                  // not printable

    // Modifies "origin" if a match found
    //
    if (len >= 5 && strncmp(name,"aprsd",5) == 0) {
        snprintf(origin, origin_size, "INET");
        origin[4] = '\0';   // Terminate it
        return(1);                      // aprsdXXXX is ok
    }

    // Modifies "origin" if a match found
    //
    if (len == 6) {                     // check for NWS
        ok = 1;
        for (i=0;i<6;i++)
            if (name[i] <'A' || name[i] > 'Z')  // 6 uppercase characters
                ok = 0;
        ok = ok && (info != NULL);      // check if we can test info
        if (ok) {
            ptr = strstr(info,":NWS-"); // "NWS-" in info field (non-compressed alert)
            ok = (ptr != NULL);

            if (!ok) {
                ptr = strstr(info,":NWS_"); // "NWS_" in info field (compressed alert)
                ok = (ptr != NULL);
            }
        }
        if (ok) {
            snprintf(origin, origin_size, "INET-NWS");
            origin[8] = '\0';
            return(1);                      // weather alerts
        }
    }

    return(1);  // Accept anything else if we get to this point in
                // the code.  After all, the message came from the
                // internet, not from RF.
}



/*
 *  Extract third-party traffic from information field before processing
 */
int extract_third_party(char *call,
                        char *path,
                        int path_size,
                        char **info,
                        char *origin,
                        int origin_size) {
    int ok;
    char *p_call;
    char *p_path;

    p_call = NULL;                              // to make the compiler happy...
    p_path = NULL;                              // to make the compiler happy...
    ok = 0;
    if (!is_my_call(call,1)) { // Check SSID also
        // todo: add reporting station call to database ??
        //       but only if not identical to reported call
        (*info) = (*info) +1;                   // strip '}' character
        p_call = strtok((*info),">");           // extract call
        if (p_call != NULL) {
            p_path = strtok(NULL,":");          // extract path
            if (p_path != NULL) {
                (*info) = strtok(NULL,"");      // rest is information field
                if ((*info) != NULL)            // the above looks dangerous, but works on same string
                    if (strlen(p_path) < 100)
                        ok = 1;                 // we have found all three components
            }
        }
    }

    if (ok) {

        snprintf(path,
            path_size,
            "%s",
            p_path);

        ok = valid_path(path);                  // check the path and convert it to TAPR format
        // Note that valid_path() also removes igate injection identifiers

    }

    if (ok) {                                         // check callsign
        (void)remove_trailing_asterisk(p_call);       // is an asterisk valid here ???
        if (valid_inet_name(p_call,(*info),origin,origin_size)) { // accept some of the names used in internet
            // Treat it as object with special origin
            snprintf(call,
                MAX_CALLSIGN+1,
                "%s",
                p_call);
        }
        else if (valid_call(p_call)) {              // accept real AX.25 calls
            snprintf(call,
                MAX_CALLSIGN+1,
                "%s",
                p_call);
        }
        else {
            ok = 0;
        }
    }
    return(ok);
}



// DK7IN 99
/*
 *  Extract Compressed Position Report Data Formats from begin of line
 *    [APRS Reference, chapter 9]
 *
 * If a position is found, it is deleted from the data.  If a
 * compressed position is found, delete the three csT bytes as well,
 * even if all spaces.
 * Returns 0 if the packet is NOT a properly compressed position
 * packet, returns 1 if ok.
 */
int extract_comp_position(AprsDataRow *p_station, char **info, /*@unused@*/ int type) {
    int ok;
    int x1, x2, x3, x4, y1, y2, y3, y4;
    int c = 0;
    int s = 0;
    int T = 0;
    int len;
    char *my_data;
    float lon = 0;
    float lat = 0;
    float range;
    int skip = 0;
    char L;

    my_data = (*info);

    // Check leading char.  Must be one of these:
    // '/'
    // '\'
    // A-Z
    // a-j
    //
    L = my_data[0];
    if (   L == '/'
        || L == '\\'
        || ( L >= 'A' && L <= 'Z' )
        || ( L >= 'a' && L <= 'j' ) ) {
        // We're good so far
    }
    else {
        // Note one of the symbol table or overlay characters, so
        // there's something funky about this packet.  It's not a
        // properly formatted compressed position.
        return(0);
    }

    //fprintf(stderr,"my_data: %s\n",my_data);

    // If c = space, csT bytes are ignored.  Minimum length:  8
    // bytes for lat/lon, 2 for symbol, 3 for csT for a total of 13.
    len = strlen(my_data);
    ok = (int)(len >= 13);

    if (ok) {
        y1 = (int)my_data[1] - '!';
        y2 = (int)my_data[2] - '!';
        y3 = (int)my_data[3] - '!';
        y4 = (int)my_data[4] - '!';
        x1 = (int)my_data[5] - '!';
        x2 = (int)my_data[6] - '!';
        x3 = (int)my_data[7] - '!';
        x4 = (int)my_data[8] - '!';

        // csT bytes
        if (my_data[10] == ' ') // Space
            c = -1; // This causes us to ignore csT
        else {
            c = (int)my_data[10] - '!';
            s = (int)my_data[11] - '!';
            T = (int)my_data[12] - '!';
        }
        skip = 13;

        // Convert ' ' to '0'.  Not specified in APRS Reference!  Do
        // we need it?
        if (x1 == -1) x1 = '\0';
        if (x2 == -1) x2 = '\0';
        if (x3 == -1) x3 = '\0';
        if (x4 == -1) x4 = '\0';
        if (y1 == -1) y1 = '\0';
        if (y2 == -1) y2 = '\0';
        if (y3 == -1) y3 = '\0';
        if (y4 == -1) y4 = '\0';

        ok = (int)(ok && (x1 >= '\0' && x1 < 91));  //  /YYYYXXXX$csT
        ok = (int)(ok && (x2 >= '\0' && x2 < 91));  //  0123456789012
        ok = (int)(ok && (x3 >= '\0' && x3 < 91));
        ok = (int)(ok && (x4 >= '\0' && x4 < 91));
        ok = (int)(ok && (y1 >= '\0' && y1 < 91));
        ok = (int)(ok && (y2 >= '\0' && y2 < 91));
        ok = (int)(ok && (y3 >= '\0' && y3 < 91));
        ok = (int)(ok && (y4 >= '\0' && y4 < 91));

        T &= 0x3F;      // DK7IN: force Compression Byte to valid format
                        // mask off upper two unused bits, they should be zero!?

        ok = (int)(ok && (c == -1 || ((c >=0 && c < 91) && (s >= 0 && s < 91) && (T >= 0 && T < 64))));

        if (ok) {
            lat = (((y1 * 91 + y2) * 91 + y3) * 91 + y4 ) / 380926.0; // in deg, 0:  90°N
            lon = (((x1 * 91 + x2) * 91 + x3) * 91 + x4 ) / 190463.0; // in deg, 0: 180°W
            lat *= 60 * 60 * 100;                       // in 1/100 sec
            lon *= 60 * 60 * 100;                       // in 1/100 sec

            // The below check should _not_ be done.  Compressed
            // format can resolve down to about 1 foot worldwide
            // (0.3 meters).
            //if ((((long)(lat+4) % 60) > 8) || (((long)(lon+4) % 60) > 8))
            //    ok = 0;   // check max resolution 0.01 min to
                            // catch even more errors
        }
    }

    if (ok) {
        overlay_symbol(my_data[9], my_data[0], p_station);      // Symbol / Table

        // Callsign check here includes checking SSID for an exact
        // match
//        if (!is_my_call(p_station->call_sign,1)) {  // don't change my position, I know it better...
        if ( !(is_my_station(p_station)) ) {  // don't change my position, I know it better...
 
            // Record the uncompressed lat/long that we just
            // computed.
            p_station->coord_lat = (long)((lat));               // in 1/100 sec
            p_station->coord_lon = (long)((lon));               // in 1/100 sec
        }

        if (c >= 0) {                                   // ignore csT if c = ' '
            if (c < 90) {   // Found course/speed or altitude bytes
                if ((T & 0x18) == 0x10) {   // check for GGA (with altitude)
                    xastir_snprintf(p_station->altitude, sizeof(p_station->altitude), "%06.0f",pow(1.002,(double)(c*91+s))*0.3048);
                }
                else { // Found compressed course/speed bytes

                    // Convert 0 degrees to 360 degrees so that
                    // Xastir will see it as a valid course and do
                    // dead-reckoning properly on this station
                    if (c == 0) {
                        c = 90;
                    }

                    // Compute course in degrees
                    xastir_snprintf(p_station->course,
                        sizeof(p_station->course),
                        "%03d",
                        c*4);

                    // Compute speed in knots
                    xastir_snprintf(p_station->speed,
                        sizeof(p_station->speed),
                        "%03.0f",
                        pow( 1.08,(double)s ) - 1.0);

                    //fprintf(stderr,"Decoded speed:%s, course:%s\n",p_station->speed,p_station->course);

                }
            }
            else {    // Found pre-calculated radio range bytes
                if (c == 90) {
                    // pre-calculated radio range
                    range = 2 * pow(1.08,(double)s);    // miles

                    // DK7IN: dirty hack...  but better than nothing
                    if (s <= 5)                         // 2.9387 mi
                        xastir_snprintf(p_station->power_gain, sizeof(p_station->power_gain), "PHG%s0", "000");
                    else if (s <= 17)                   // 7.40 mi
                        xastir_snprintf(p_station->power_gain, sizeof(p_station->power_gain), "PHG%s0", "111");
                    else if (s <= 36)                   // 31.936 mi
                        xastir_snprintf(p_station->power_gain, sizeof(p_station->power_gain), "PHG%s0", "222");
                    else if (s <= 75)                   // 642.41 mi
                        xastir_snprintf(p_station->power_gain, sizeof(p_station->power_gain), "PHG%s0", "333");
                    else                       // max 90:  2037.8 mi
                        xastir_snprintf(p_station->power_gain, sizeof(p_station->power_gain), "PHG%s0", "444");
                }
            }
        }
        (*info) += skip;    // delete position from comment
    }


    //fprintf(stderr,"  extract_comp_position end: %s\n",*info);

    return(ok);
}


/*
 *  Extract bearing and number/range/quality from beginning of info field
 */
int extract_bearing_NRQ(char *info, char *bearing, char *nrq) {
    int i,found,len;

    len = (int)strlen(info);
    found = 0;
    if (len >= 8) {
        found = 1;
        for(i=1; found && i<8; i++)         // check data format
            if(!(isdigit((int)info[i]) || (i==4 && info[i]=='/')))
                found=0;
    }
    if (found) {
        substr(bearing,info+1,3);
        substr(nrq,info+5,3);

        for (i=0;i<=len-8;i++)        // delete bearing/nrq from info field
            info[i] = info[i+8];
    }

    if (!found) {
        bearing[0] ='\0';
        nrq[0]='\0';
    }
    return(found);
}




// APRS Data Extensions               [APRS Reference p.27]
//  .../...  Course & Speed, may be followed by others (see p.27)
//  .../...  Wind Dir and Speed
//  PHG....  Station Power and Effective Antenna Height/Gain
//  RNG....  Pre-Calculated Radio Range
//  DFS....  DF Signal Strength and Effective Antenna Height/Gain
//  T../C..  Area Object Descriptor

/* Extract one of several possible APRS Data Extensions */
void process_data_extension(AprsDataRow *p_station, char *data, /*@unused@*/ int type) {
    char temp1[7+1];
    char temp2[3+1];
    char temp3[10+1];
    char bearing[3+1];
    char nrq[3+1];

    if (p_station->aprs_symbol.aprs_type == '\\' && p_station->aprs_symbol.aprs_symbol == 'l') {
            /* This check needs to come first because the area object extension can look
               exactly like what extract_speed_course will attempt to decode. */
            extract_area(p_station, data);
    }
    else {
        clear_area(p_station); // we got a packet with a non area symbol, so clear the data

        if (extract_speed_course(data,temp1,temp2)) {  // ... from Mic-E, etc.
        //fprintf(stderr,"extracted speed/course\n");

            if (atof(temp2) > 0) {
            //fprintf(stderr,"course is non-zero\n");
            xastir_snprintf(p_station->speed,
                sizeof(p_station->speed),
                "%06.2f",
                atof(temp1));
            xastir_snprintf(p_station->course,  // in degrees
                sizeof(p_station->course),
                "%s",
                temp2);
            }

            if (extract_bearing_NRQ(data, bearing, nrq)) {  // Beam headings from DF'ing
                //fprintf(stderr,"extracted bearing and NRQ\n");
                xastir_snprintf(p_station->bearing,
                    sizeof(p_station->bearing),
                    "%s",
                    bearing);
                xastir_snprintf(p_station->NRQ,
                    sizeof(p_station->NRQ),
                    "%s",
                    nrq);
                p_station->signal_gain[0] = '\0';   // And blank out the shgd values
            }
        }
        // Don't try to extract speed & course if a compressed
        // object.  Test for beam headings for compressed packets
        // here
        else if (extract_bearing_NRQ(data, bearing, nrq)) {  // Beam headings from DF'ing

            //fprintf(stderr,"extracted bearing and NRQ\n");
            xastir_snprintf(p_station->bearing,
                    sizeof(p_station->bearing),
                    "%s",
                    bearing);
            xastir_snprintf(p_station->NRQ,
                    sizeof(p_station->NRQ),
                    "%s",
                    nrq);
            p_station->signal_gain[0] = '\0';   // And blank out the shgd values
        }
        else {
            if (extract_powergain_range(data,temp1)) {

//fprintf(stderr,"Found power_gain: %s\n", temp1);

                xastir_snprintf(p_station->power_gain,
                    sizeof(p_station->power_gain),
                    "%s",
                    temp1);

                if (extract_bearing_NRQ(data, bearing, nrq)) {  // Beam headings from DF'ing
                    //fprintf(stderr,"extracted bearing and NRQ\n");
                    xastir_snprintf(p_station->bearing,
                        sizeof(p_station->bearing),
                        "%s",
                        bearing);
                    xastir_snprintf(p_station->NRQ,
                        sizeof(p_station->NRQ),
                        "%s",
                        nrq);
                    p_station->signal_gain[0] = '\0';   // And blank out the shgd values
                }
            }
            else {
                if (extract_omnidf(data,temp1)) {
                    xastir_snprintf(p_station->signal_gain,
                        sizeof(p_station->signal_gain),
                        "%s",
                        temp1);   // Grab the SHGD values
                    p_station->bearing[0] = '\0';   // And blank out the bearing/NRQ values
                    p_station->NRQ[0] = '\0';

                    // The spec shows speed/course before DFS, but example packets that
                    // come with DOSaprs show DFSxxxx/speed/course.  We'll take care of
                    // that possibility by trying to decode speed/course again.
                    if (extract_speed_course(data,temp1,temp2)) {  // ... from Mic-E, etc.
                    //fprintf(stderr,"extracted speed/course\n");
                        if (atof(temp2) > 0) {
                            //fprintf(stderr,"course is non-zero\n");
                            xastir_snprintf(p_station->speed,
                                sizeof(p_station->speed),
                                "%06.2f",
                                atof(temp1));
                            xastir_snprintf(p_station->course,
                                sizeof(p_station->course),
                                "%s",
                                temp2);                    // in degrees
                        }
                    }

                    // The spec shows that omnidf and bearing/NRQ can be in the same
                    // packet, which makes no sense, but we'll try to decode it that
                    // way anyway.
                    if (extract_bearing_NRQ(data, bearing, nrq)) {  // Beam headings from DF'ing
                        //fprintf(stderr,"extracted bearing and NRQ\n");
                        xastir_snprintf(p_station->bearing,
                            sizeof(p_station->bearing),
                            "%s",
                            bearing);
                        xastir_snprintf(p_station->NRQ,
                            sizeof(p_station->NRQ),
                            "%s",
                            nrq);
                        //p_station->signal_gain[0] = '\0';   // And blank out the shgd values
                    }
                }
            }
        }

        if (extract_signpost(data, temp2)) {
            //fprintf(stderr,"extracted signpost data\n");
            xastir_snprintf(p_station->signpost,
                sizeof(p_station->signpost),
                "%s",
                temp2);
        }

        if (extract_probability_min(data, temp3, sizeof(temp3))) {
            //fprintf(stderr,"extracted probability_min data: %s\n",temp3);
            xastir_snprintf(p_station->probability_min,
                sizeof(p_station->probability_min),
                "%s",
                temp3);
        }
 
        if (extract_probability_max(data, temp3, sizeof(temp3))) {
            //fprintf(stderr,"extracted probability_max data: %s\n",temp3);
            xastir_snprintf(p_station->probability_max,
                sizeof(p_station->probability_max),
                "%s",
                temp3);
        }
    }
}


/*
 *  Extract altitude from APRS info field          "/A=012345"    in feet
 */
int extract_altitude(char *info, char *altitude) {
    int i,ofs,found,len;

    found=0;
    len = (int)strlen(info);
    for(ofs=0; !found && ofs<len-8; ofs++)      // search for start sequence
        if (strncmp(info+ofs,"/A=",3)==0) {
            found=1;
            // Are negative altitudes even defined?  Yes!  In Mic-E spec to -10,000 meters
            if(!isdigit((int)info[ofs+3]) && info[ofs+3]!='-')  // First char must be digit or '-'
                found=0;
            for(i=4; found && i<9; i++)         // check data format for next 5 chars
                if(!isdigit((int)info[ofs+i]))
                    found=0;
    }
    if (found) {
        ofs--;  // was one too much on exit from for loop
        substr(altitude,info+ofs+3,6);
        for (i=ofs;i<=len-9;i++)        // delete altitude from info field
            info[i] = info[i+9];
    }
    else
        altitude[0] = '\0';
    return(found);
}




/* extract all available information from info field */
void process_info_field(AprsDataRow *p_station, char *info, /*@unused@*/ int type) {
    char temp_data[6+1];	
//    char time_data[MAX_TIME];

    if (extract_altitude(info,temp_data)) {                         // get altitude
        xastir_snprintf(p_station->altitude, sizeof(p_station->altitude), "%.2f",atof(temp_data)*0.3048);
        //fprintf(stderr,"%.2f\n",atof(temp_data)*0.3048);
    }
    // do other things...
}






/*
 *  Extract Uncompressed Position Report from begin of line
 *
 * If a position is found, it is deleted from the data.
 */
int extract_position(AprsDataRow *p_station, char **info, int type) {
    int ok;
    char temp_lat[8+1];
    char temp_lon[9+1];
    char temp_grid[8+1];
    char *my_data;
    float gridlat;
    float gridlon;
    my_data = (*info);

    if (type != APRS_GRID){ // Not a grid
        ok = (int)(strlen(my_data) >= 19);
        ok = (int)(ok && my_data[4]=='.' && my_data[14]=='.'
            && (toupper(my_data[7]) =='N' || toupper(my_data[7]) =='S')
            && (toupper(my_data[17])=='E' || toupper(my_data[17])=='W'));
        // errors found:  [4]: X   [7]: n s   [17]: w e
        if (ok) {
            ok =             is_num_chr(my_data[0]);           // 5230.31N/01316.88E>
            ok = (int)(ok && is_num_chr(my_data[1]));          // 0123456789012345678
            ok = (int)(ok && is_num_or_sp(my_data[2]));
            ok = (int)(ok && is_num_or_sp(my_data[3]));
            ok = (int)(ok && is_num_or_sp(my_data[5]));
            ok = (int)(ok && is_num_or_sp(my_data[6]));
            ok = (int)(ok && is_num_chr(my_data[9]));
            ok = (int)(ok && is_num_chr(my_data[10]));
            ok = (int)(ok && is_num_chr(my_data[11]));
            ok = (int)(ok && is_num_or_sp(my_data[12]));
            ok = (int)(ok && is_num_or_sp(my_data[13]));
            ok = (int)(ok && is_num_or_sp(my_data[15]));
            ok = (int)(ok && is_num_or_sp(my_data[16]));
        }
                                            
        if (ok) {
            overlay_symbol(my_data[18], my_data[8], p_station);
            p_station->pos_amb = 0;
            // spaces in latitude set position ambiguity, spaces in longitude do not matter
            // we will adjust the lat/long to the center of the rectangle of ambiguity
            if (my_data[2] == ' ') {      // nearest degree
                p_station->pos_amb = 4;
                my_data[2]  = my_data[12] = '3';
                my_data[3]  = my_data[5]  = my_data[6]  = '0';
                my_data[13] = my_data[15] = my_data[16] = '0';
            }
            else if (my_data[3] == ' ') { // nearest 10 minutes
                p_station->pos_amb = 3;
                my_data[3]  = my_data[13] = '5';
                my_data[5]  = my_data[6]  = '0';
                my_data[15] = my_data[16] = '0';
            }
            else if (my_data[5] == ' ') { // nearest minute
                p_station->pos_amb = 2;
                my_data[5]  = my_data[15] = '5';
                my_data[6]  = '0';
                my_data[16] = '0';
            }
            else if (my_data[6] == ' ') { // nearest 1/10th minute
                p_station->pos_amb = 1;
                my_data[6]  = my_data[16] = '5';
            }

            xastir_snprintf(temp_lat,
                sizeof(temp_lat),
                "%s",
                my_data);
            temp_lat[7] = toupper(my_data[7]);
            temp_lat[8] = '\0';

            xastir_snprintf(temp_lon,
                sizeof(temp_lon),
                "%s",
                my_data+9);
            temp_lon[8] = toupper(my_data[17]);
            temp_lon[9] = '\0';

            // Callsign check here also checks SSID for an exact
            // match
//            if (!is_my_call(p_station->call_sign,1)) {      // don't change my position, I know it better...
            if ( !(is_my_station(p_station)) ) {      // don't change my position, I know it better...

                p_station->coord_lat = convert_lat_s2l(temp_lat);   // ...in case of position ambiguity
                p_station->coord_lon = convert_lon_s2l(temp_lon);
            }

            (*info) += 19;                  // delete position from comment
        }
    }
    else { // It is a grid 
        // first sanity checks, need more
        ok = (int)(is_num_chr(my_data[2]));
        ok = (int)(ok && is_num_chr(my_data[3]));
        ok = (int)(ok && ((my_data[0]>='A')&&(my_data[0]<='R')));
        ok = (int)(ok && ((my_data[1]>='A')&&(my_data[1]<='R')));
        if (ok) {
            xastir_snprintf(temp_grid,
                sizeof(temp_grid),
                "%s",
                my_data);
            // this test treats >6 digit grids as 4 digit grids; >6 are uncommon.
            // the spec mentioned 4 or 6, I'm not sure >6 is even allowed.
            if ( (temp_grid[6] != ']') || (temp_grid[4] == 0) || (temp_grid[5] == 0)){
                p_station->pos_amb = 6; // 1deg lat x 2deg lon 
                temp_grid[4] = 'L';
                temp_grid[5] = 'L';
            }
            else {
                p_station->pos_amb = 5; // 2.5min lat x 5min lon
                temp_grid[4] = toupper(temp_grid[4]); 
                temp_grid[5] = toupper(temp_grid[5]);
            }
            // These equations came from what I read in the qgrid source code and
            // various mailing list archives.
            gridlon= (20.*((float)temp_grid[0]-65.) + 2.*((float)temp_grid[2]-48.) + 5.*((float)temp_grid[4]-65.)/60.) - 180.;
            gridlat= (10.*((float)temp_grid[1]-65.) + ((float)temp_grid[3]-48.) + 5.*(temp_grid[5]-65.)/120.) - 90.;
            // could check for my callsign here, and avoid changing it...
            p_station->coord_lat = (unsigned long)(32400000l + (360000.0 * (-gridlat)));
            p_station->coord_lon = (unsigned long)(64800000l + (360000.0 * gridlon));
            p_station->aprs_symbol.aprs_type = '/';
            p_station->aprs_symbol.aprs_symbol = 'G';
        }        // is it valid grid or not - "ok"
        // could cut off the grid square from the comment here, but why bother?
    } // is it grid or not
    return(ok);
}





// Add a status line to the linked-list of status records
// associated with a station.  Note that a blank status line is
// allowed, but we don't store that unless we have seen a non-blank
// status line previously.
//
void add_status(AprsDataRow *p_station, char *status_string) {
    AprsCommentRow *ptr;
    int add_it = 0;
    int len;


    len = strlen(status_string);

    // Eliminate line-end chars
    if (len > 1) {
        if ( (status_string[len-1] == '\n')
                || (status_string[len-1] == '\r') ) {
            status_string[len-1] = '\0';
        }
    }

    // Shorten it
    (void)remove_trailing_spaces(status_string);
    (void)remove_leading_spaces(status_string);
 
    len = strlen(status_string);

    // Check for valid pointer
    if (p_station != NULL) {

// We should probably create a new station record for this station
// if there isn't one.  This allows us to collect as much info about
// a station as we can until a posit comes in for it.  Right now we
// don't do this.  If we decide to do this in the future, we also
// need a method to find out the info about that station without
// having to click on an icon, 'cuz the symbol won't be on our map
// until we have a posit.

        //fprintf(stderr,"Station:%s\tStatus:%s\n",p_station->call_sign,status_string);

        // Check whether we have any data stored for this station
        if (p_station->status_data == NULL) {
            if (len > 0) {
                // No status stored yet and new status is non-NULL,
                // so add it to the list.
                add_it++;
            }
        }
        else {  // We have status data stored already
                // Check for an identical string
            AprsCommentRow *ptr2;
            int ii = 0;
 
            ptr = p_station->status_data;
            ptr2 = ptr;
            while (ptr != NULL) {

                // Note that both text_ptr and comment_string can be
                // empty strings.

                if (strcasecmp(ptr->text_ptr, status_string) == 0) {
                    // Found a matching string
                    //fprintf(stderr,"Found match:
                    //%s:%s\n",p_station->call_sign,status_string);

// Instead of updating the timestamp, we'll delete the record from
// the list and add it to the top in the code below.  Make sure to
// tweak the "ii" pointer so that we don't end up shortening the
// list unnecessarily.
                    if (ptr == p_station->status_data) {

                        // Only update the timestamp: We're at the
                        // beginning of the list already.
                        ptr->sec_heard = sec_now();

                        return; // No need to add a new record
                    }
                    else {  // Delete the record
                        AprsCommentRow *ptr3;

                        // Keep a pointer to the record
                        ptr3 = ptr;

                        // Close the chain, skipping this record
                        ptr2->next = ptr3->next;

                        // Skip "ptr" over the record we wish to
                        // delete
                        ptr = ptr3->next;

                        // Free the record
                        free(ptr3->text_ptr);
                        free(ptr3);

                        // Muck with the counter 'cuz we just
                        // deleted one record
                        ii--;
                    }
                }
                ptr2 = ptr; // Back one record
                if (ptr != NULL) {
                    ptr = ptr->next;
                }
                ii++;
            }


            // No matching string found, or new timestamp found for
            // old record.  Add it to the top of the list.
            add_it++;
            //fprintf(stderr,"No match:
            //%s:%s\n",p_station->call_sign,status_string);

            // We counted the records.  If we have more than
            // MAX_STATUS_LINES records we'll delete/free the last
            // one to make room for the next.  This keeps us from
            // storing unique status records ad infinitum for active
            // stations, limiting the total space used.
            //
            if (ii >= MAX_STATUS_LINES) {
                // We know we didn't get a match, and that our list
                // is full (as full as we want it to be).  Traverse
                // the list again, looking for ptr2->next->next ==
                // NULL.  If found, free last record and set the
                // ptr2->next pointer to NULL.
                ptr2 = p_station->status_data;
                while (ptr2->next->next != NULL) {
                    ptr2 = ptr2->next;
                }
                // At this point, we have a pointer to the last
                // record in ptr2->next.  Free it and the text
                // string in it.
                free(ptr2->next->text_ptr);
                free(ptr2->next);
                ptr2->next = NULL;
            } 
        }

        if (add_it) {   // We add to the beginning so we don't have
                        // to traverse the linked list.  This also
                        // puts new records at the beginning of the
                        // list to keep them in sorted order.

            ptr = p_station->status_data;  // Save old pointer to records
            p_station->status_data = (AprsCommentRow *)malloc(sizeof(AprsCommentRow));
            CHECKMALLOC(p_station->status_data);

            p_station->status_data->next = ptr;    // Link in old records or NULL

            // Malloc the string space we'll need, attach it to our
            // new record
            p_station->status_data->text_ptr = (char *)malloc(sizeof(char) * (len+1));
            CHECKMALLOC(p_station->status_data->text_ptr);

            // Fill in the string
            xastir_snprintf(p_station->status_data->text_ptr,
                len+1,
                "%s",
                status_string);

            // Fill in the timestamp
            p_station->status_data->sec_heard = sec_now();

            //fprintf(stderr,"Station:%s\tStatus:%s\n\n",p_station->call_sign,p_station->status_data->text_ptr);
        }
    }
}


 
// Add a comment line to the linked-list of comment records
// associated with a station.  Note that a blank comment is allowed
// and necessary for the times when we wish to blank out the comment
// on an object/item, but we don't store that unless we have seen a
// non-blank comment line previously.
//
void add_comment(AprsDataRow *p_station, char *comment_string) {
    AprsCommentRow *ptr;
    int add_it = 0;
    int len;


    len = strlen(comment_string);

    // Eliminate line-end chars
    if (len > 1) {
        if ( (comment_string[len-1] == '\n')
                || (comment_string[len-1] == '\r') ) {
            comment_string[len-1] = '\0';
        }
    }

    // Shorten it
    (void)remove_trailing_spaces(comment_string);
    (void)remove_leading_spaces(comment_string);

    len = strlen(comment_string);

    // Check for valid pointer
    if (p_station != NULL) {

        // Check whether we have any data stored for this station
        if (p_station->comment_data == NULL) {
            if (len > 0) {
                // No comments stored yet and new comment is
                // non-NULL, so add it to the list.
                add_it++;
            }
        }
        else {  // We have comment data stored already
                // Check for an identical string
            AprsCommentRow *ptr2;
            int ii = 0;
 
            ptr = p_station->comment_data;
            ptr2 = ptr;
            while (ptr != NULL) {

                // Note that both text_ptr and comment_string can be
                // empty strings.

                if (strcasecmp(ptr->text_ptr, comment_string) == 0) {
                    // Found a matching string
//fprintf(stderr,"Found match: %s:%s\n",p_station->call_sign,comment_string);

// Instead of updating the timestamp, we'll delete the record from
// the list and add it to the top in the code below.  Make sure to
// tweak the "ii" pointer so that we don't end up shortening the
// list unnecessarily.
                    if (ptr == p_station->comment_data) {
                        // Only update the timestamp:  We're at the
                        // beginning of the list already.
                        ptr->sec_heard = sec_now();

                        return; // No need to add a new record
                    }
                    else {  // Delete the record
                        AprsCommentRow *ptr3;

                        // Keep a pointer to the record
                        ptr3 = ptr;

                        // Close the chain, skipping this record
                        ptr2->next = ptr3->next;

                        // Skip "ptr" over the record we with to
                        // delete
                        ptr = ptr3->next;

                        // Free the record
                        free(ptr3->text_ptr);
                        free(ptr3);

                        // Muck with the counter 'cuz we just
                        // deleted one record
                        ii--;
                    }
                }
                ptr2 = ptr; // Keep this pointer one record back as
                            // we progress.

                if (ptr != NULL) {
                    ptr = ptr->next;
                }

                ii++;
            }
            // No matching string found, or new timestamp found for
            // old record.  Add it to the top of the list.
            add_it++;
            //fprintf(stderr,"No match: %s:%s\n",p_station->call_sign,comment_string);

            // We counted the records.  If we have more than
            // MAX_COMMENT_LINES records we'll delete/free the last
            // one to make room for the next.  This keeps us from
            // storing unique comment records ad infinitum for
            // active stations, limiting the total space used.
            //
            if (ii >= MAX_COMMENT_LINES) {

                // We know we didn't get a match, and that our list
                // is full (as we want it to be).  Traverse the list
                // again, looking for ptr2->next->next == NULL.  If
                // found, free that last record and set the
                // ptr2->next pointer to NULL.
                ptr2 = p_station->comment_data;
                while (ptr2->next->next != NULL) {
                    ptr2 = ptr2->next;
                }
                // At this point, we have a pointer to the last
                // record in ptr2->next.  Free it and the text
                // string in it.
                free(ptr2->next->text_ptr);
                free(ptr2->next);
                ptr2->next = NULL;
            } 
        }

        if (add_it) {   // We add to the beginning so we don't have
                        // to traverse the linked list.  This also
                        // puts new records at the beginning of the
                        // list to keep them in sorted order.

            ptr = p_station->comment_data;  // Save old pointer to records
            p_station->comment_data = (AprsCommentRow *)malloc(sizeof(AprsCommentRow));
            CHECKMALLOC(p_station->comment_data);

            p_station->comment_data->next = ptr;    // Link in old records or NULL

            // Malloc the string space we'll need, attach it to our
            // new record
            p_station->comment_data->text_ptr = (char *)malloc(sizeof(char) * (len+1));
            CHECKMALLOC(p_station->comment_data->text_ptr);

            // Fill in the string
            xastir_snprintf(p_station->comment_data->text_ptr,
                len+1,
                "%s",
                comment_string);

            // Fill in the timestamp
            p_station->comment_data->sec_heard = sec_now();
        }
    }
}



// Extract single weather data item from "data".  Returns it in
// "temp".  Modifies "data" to remove the found data from the
// string.  Returns a 1 if found, 0 if not found.
//
// PE1DNN
// If the item is contained in the string but does not contain a
// value then regard the item as "not found" in the weather string.
//
int extract_weather_item(char *data, char type, int datalen, char *temp) {
    int i,ofs,found,len;


//fprintf(stderr,"%s\n",data);

    found=0;
    len = (int)strlen(data);
    for(ofs=0; !found && ofs<len-datalen; ofs++)      // search for start sequence
        if (data[ofs]==type) {
            found=1;
            if (!is_weather_data(data+ofs+1, datalen))
                found=0;
        }
    if (found) {   // ofs now points after type character
        substr(temp,data+ofs,datalen);
        for (i=ofs-1;i<len-datalen;i++)        // delete item from info field
            data[i] = data[i+datalen+1];
        if((temp[0] == ' ') || (temp[0] == '.')) {
            // found it, but it doesn't contain a value!
            // Clean up and report "not found" - PE1DNN
            temp[0] = '\0';
            found = 0;
        }
        else
        {
//                fprintf(stderr,"extract_weather_item: %s\n",temp);
        }
    }
    else
        temp[0] = '\0';
    return(found);
}





// test-extract single weather data item from information field.  In
// other words:  Does not change the input string, but does test
// whether the data is present.  Returns a 1 if found, 0 if not
// found.
//
// PE1DNN
// If the item is contained in the string but does not contain a
// value then regard the item as "not found" in the weather string.
//
int test_extract_weather_item(char *data, char type, int datalen) {
    int ofs,found,len;

    found=0;
    len = (int)strlen(data);
    for(ofs=0; !found && ofs<len-datalen; ofs++)      // search for start sequence
        if (data[ofs]==type) {
            found=1;
            if (!is_weather_data(data+ofs+1, datalen))
                found=0;
        }

    // We really should test for numbers here (with an optional
    // leading '-'), and test across the length of the substring.
    //
    if(found && ((data[ofs+1] == ' ') || (data[ofs+1] == '.'))) {
        // found it, but it doesn't contain a value!
        // report "not found" - PE1DNN
        found = 0;
    }

    //fprintf(stderr,"test_extract: %c %d\n",type,found);
    return(found);
}


int get_weather_record(AprsDataRow *fill) {    // get or create weather storage
    int ok=1;

    if (fill->weather_data == NULL) {      // new weather data, allocate storage and init
        fill->weather_data = malloc(sizeof(AprsWeatherRow));
        if (fill->weather_data == NULL) {
            fprintf(stderr,"Couldn't allocate memory in get_weather_record()\n");
            ok = 0;
        }
        else {
            init_weather(fill->weather_data);
        }
    }
    return(ok);
}




// DK7IN 77
// raw weather report            in information field
// positionless weather report   in information field
// complete weather report       with lat/lon
//  see APRS Reference page 62ff
//
// Added 'F' for Fuel Temp and 'f' for Fuel Moisture in order to
// decode these two new parameters used for RAWS weather station
// objects.
//
// By the time we call this function we've already extracted any
// time/position info at the beginning of the string.
//
int extract_weather(AprsDataRow *p_station, char *data, int compr) {
    char time_data[MAX_TIME];
    char temp[5];
    int  ok = 1;
    AprsWeatherRow *weather;
    char course[4];
    char speed[4];
    int in_knots = 0;

//WE7U
// Try copying the string to a temporary string, then do some
// extractions to see if a few weather items are present?  This
// would allow us to have the weather items in any order, and if
// enough of them were present, we consider it to be a weather
// packet?  We'd need to qualify all of the data to make sure we had
// the proper number of digits for each.  The trick is to make sure
// we don't decide it's a weather packet if it's not.  We don't know
// what people might send in packets in the future.

    if (compr) {        // compressed position report
        // Look for weather data in fixed locations first
        if (strlen(data) >= 8
                && data[0] =='g' && is_weather_data(&data[1],3)
                && data[4] =='t' && is_weather_data(&data[5],3)) {

            // Snag WX course/speed from compressed position data.
            // This speed is in knots.  This assumes that we've
            // already extracted speed/course from the compressed
            // packet.  extract_comp_position() extracts
            // course/speed as well.
            xastir_snprintf(speed,
                sizeof(speed),
                "%s",
                p_station->speed);
            xastir_snprintf(course,
                sizeof(course),
                "%s",
                p_station->course);
            in_knots = 1;

            //fprintf(stderr,"Found compressed wx\n");
        }
        // Look for weather data in non-fixed locations (RAWS WX
        // Stations?)
        else if ( strlen(data) >= 8
                && test_extract_weather_item(data,'g',3)
                && test_extract_weather_item(data,'t',3) ) {

            // Snag WX course/speed from compressed position data.
            // This speed is in knots.  This assumes that we've
            // already extracted speed/course from the compressed
            // packet.  extract_comp_position() extracts
            // course/speed as well.
            xastir_snprintf(speed,
                sizeof(speed),
                "%s",
                p_station->speed);
            xastir_snprintf(course,
                sizeof(course),
                "%s",
                p_station->course);
            in_knots = 1;

            //fprintf(stderr,"Found compressed WX in non-fixed locations! %s:%s\n",
            //    p_station->call_sign,data);

        }
        else {  // No weather data found
            ok = 0;

            //fprintf(stderr,"No compressed wx\n");
        }
    }
    else {    // Look for non-compressed weather data
        // Look for weather data in defined locations first
        if (strlen(data)>=15 && data[3]=='/'
                && is_weather_data(data,3) && is_weather_data(&data[4],3)
                && data[7] =='g' && is_weather_data(&data[8], 3)
                && data[11]=='t' && is_weather_data(&data[12],3)) {    // Complete Weather Report

            // Get speed/course.  Speed is in knots.
            (void)extract_speed_course(data,speed,course);
            in_knots = 1;

            // Either one not found?  Try again.
            if ( (speed[0] == '\0') || (course[0] == '\0') ) {

                // Try to get speed/course from 's' and 'c' fields
                // (another wx format).  Speed is in mph.
                (void)extract_weather_item(data,'c',3,course); // wind direction (in degrees)
                (void)extract_weather_item(data,'s',3,speed);  // sustained one-minute wind speed (in mph)
                in_knots = 0;
            }

            //fprintf(stderr,"Found Complete Weather Report\n");
        }
        // Look for date/time and weather in fixed locations first
        else if (strlen(data)>=16
                && data[0] =='c' && is_weather_data(&data[1], 3)
                && data[4] =='s' && is_weather_data(&data[5], 3)
                && data[8] =='g' && is_weather_data(&data[9], 3)
                && data[12]=='t' && is_weather_data(&data[13],3)) { // Positionless Weather Data
//fprintf(stderr,"Found positionless wx data\n");
            // Try to snag speed/course out of first 7 bytes.  Speed
            // is in knots.
            (void)extract_speed_course(data,speed,course);
            in_knots = 1;

            // Either one not found?  Try again.
            if ( (speed[0] == '\0') || (course[0] == '\0') ) {
//fprintf(stderr,"Trying again for course/speed\n");
                // Also try to get speed/course from 's' and 'c' fields
                // (another wx format)
                (void)extract_weather_item(data,'c',3,course); // wind direction (in degrees)
                (void)extract_weather_item(data,'s',3,speed);  // sustained one-minute wind speed (in mph)
                in_knots = 0;
            }

            //fprintf(stderr,"Found weather\n");
        }
        // Look for weather data in non-fixed locations (RAWS WX
        // Stations?)
        else if (strlen (data) >= 16
                && test_extract_weather_item(data,'h',2)
                && test_extract_weather_item(data,'g',3)
                && test_extract_weather_item(data,'t',3) ) {

            // Try to snag speed/course out of first 7 bytes.  Speed
            // is in knots.
            (void)extract_speed_course(data,speed,course);
            in_knots = 1;

            // Either one not found?  Try again.
            if ( (speed[0] == '\0') || (course[0] == '\0') ) {

                // Also try to get speed/course from 's' and 'c' fields
                // (another wx format)
                (void)extract_weather_item(data,'c',3,course); // wind direction (in degrees)
                (void)extract_weather_item(data,'s',3,speed);  // sustained one-minute wind speed (in mph)
                in_knots = 0;
            }
 
            //fprintf(stderr,"Found WX in non-fixed locations!  %s:%s\n",
            //    p_station->call_sign,data);
        }
        else {  // No weather data found
            ok = 0;

            //fprintf(stderr,"No wx found\n");
        }
    }

    if (ok) {
        ok = get_weather_record(p_station);     // get existing or create new weather record
    }

    if (ok) {
        weather = p_station->weather_data;

        // Copy into weather speed variable.  Convert knots to mph
        // if necessary.
        if (in_knots) {
            xastir_snprintf(weather->wx_speed,
                sizeof(weather->wx_speed),
                "%03.0f",
                atoi(speed) * 1.1508);  // Convert knots to mph
        }
        else {
            // Already in mph.  Copy w/no conversion.
            xastir_snprintf(weather->wx_speed,
                sizeof(weather->wx_speed),
                "%s",
                speed);
        }

        xastir_snprintf(weather->wx_course,
            sizeof(weather->wx_course),
            "%s",
            course);

        if (compr) {        // course/speed was taken from normal data, delete that
            // fix me: we delete a potential real speed/course now
            // we should differentiate between normal and weather data in compressed position decoding...
//            p_station->speed_time[0]     = '\0';
            p_station->speed[0]          = '\0';
            p_station->course[0]         = '\0';
        }

        (void)extract_weather_item(data,'g',3,weather->wx_gust);      // gust (peak wind speed in mph in the last 5 minutes)

        (void)extract_weather_item(data,'t',3,weather->wx_temp);      // temperature (in deg Fahrenheit), could be negative

        (void)extract_weather_item(data,'r',3,weather->wx_rain);      // rainfall (1/100 inch) in the last hour

        (void)extract_weather_item(data,'p',3,weather->wx_prec_24);   // rainfall (1/100 inch) in the last 24 hours

        (void)extract_weather_item(data,'P',3,weather->wx_prec_00);   // rainfall (1/100 inch) since midnight

        if (extract_weather_item(data,'h',2,weather->wx_hum))         // humidity (in %, 00 = 100%)
                xastir_snprintf(weather->wx_hum, sizeof(weather->wx_hum), "%03d",(atoi(weather->wx_hum)+99)%100+1);

        if (extract_weather_item(data,'b',5,weather->wx_baro))  // barometric pressure (1/10 mbar / 1/10 hPascal)
            xastir_snprintf(weather->wx_baro,
                sizeof(weather->wx_baro),
                "%0.1f",
                (float)(atoi(weather->wx_baro)/10.0));

        // If we parsed a speed/course, a second 's' parameter means
        // snowfall.  Try to parse it, but only in the case where
        // we've parsed speed out of this packet already.
        if ( (speed[0] != '\0') && (course[0] != '\0') ) {
            (void)extract_weather_item(data,'s',3,weather->wx_snow);      // snowfall, inches in the last 24 hours
        }

        (void)extract_weather_item(data,'L',3,temp);                  // luminosity (in watts per square meter) 999 and below

        (void)extract_weather_item(data,'l',3,temp);                  // luminosity (in watts per square meter) 1000 and above

        (void)extract_weather_item(data,'#',3,temp);                  // raw rain counter

        (void)extract_weather_item(data,'F',3,weather->wx_fuel_temp); // Fuel Temperature in °F (RAWS)

        if (extract_weather_item(data,'f',2,weather->wx_fuel_moisture))// Fuel Moisture (RAWS) (in %, 00 = 100%)
            xastir_snprintf(weather->wx_fuel_moisture,
                sizeof(weather->wx_fuel_moisture),
                "%03d",
                (atoi(weather->wx_fuel_moisture)+99)%100+1);

//    extract_weather_item(data,'w',3,temp);                          // ?? text wUII

    // now there should be the name of the weather station...

        // Create a timestamp from the current time
        xastir_snprintf(weather->wx_time,
            sizeof(weather->wx_time),
            "%s",
            get_time(time_data));

        // Set the timestamp in the weather record so that we can
        // decide whether or not to "ghost" the weather data later.
        weather->wx_sec_time=sec_now();
//        weather->wx_data=1;  // we don't need this

//        case ('.'):/* skip */
//            wx_strpos+=4;
//            break;

//        default:
//            wx_done=1;
//            weather->wx_type=data[wx_strpos];
//            if(strlen(data)>wx_strpos+1)
//                xastir_snprintf(weather->wx_station,    
//                    sizeof(weather->wx_station),
//                    "%s",
//                    data+wx_strpos+1);
//            break;
    }
    return(ok);
}



// Initial attempt at decoding tropical storm, tropical depression,
// and hurricane data.
//
// This data can be in an Object report, but can also be in an Item
// or position report.
// "/TS" = Tropical Storm
// "/HC" = Hurricane
// "/TD" = Tropical Depression
// "/TY" = Typhoon
// "/ST" = Super Typhoon
// "/SC" = Severe Cyclone

// The symbol will be either "\@" for current position, or "/@" for
// predicted position.
//
int extract_storm(AprsDataRow *p_station, char *data, int compr) {
    char time_data[MAX_TIME];
    int  ok = 1;
    AprsWeatherRow *weather;
    char course[4];
    char speed[4];  // Speed in knots
    char *p, *p2;


// Should probably encode the storm type in the weather object and
// print it out in plain text in the Station Info dialog.

    if ((p = strstr(data, "/TS")) != NULL) {
        // We have a Tropical Storm
//fprintf(stderr,"Tropical Storm! %s\n",data);
    }
    else if ((p = strstr(data, "/TD")) != NULL) {
        // We have a Tropical Depression
//fprintf(stderr,"Tropical Depression! %s\n",data);
    }
    else if ((p = strstr(data, "/HC")) != NULL) {
        // We have a Hurricane
//fprintf(stderr,"Hurricane! %s\n",data);
    }
    else if ((p = strstr(data, "/TY")) != NULL) {
        // We have a Typhoon
//fprintf(stderr,"Hurricane! %s\n",data);
    }
    else if ((p = strstr(data, "/ST")) != NULL) {
        // We have a Super Typhoon
//fprintf(stderr,"Hurricane! %s\n",data);
    }
    else if ((p = strstr(data, "/SC")) != NULL) {
        // We have a Severe Cyclone
//fprintf(stderr,"Hurricane! %s\n",data);
    }
    else {  // Not one of the three we're trying to decode
        ok = 0;
        return(ok);
    }

//fprintf(stderr,"\n%s\n",data);

    // Back up 7 spots to try to extract the next items
    p2 = p - 7;
    if (p2 >= data) {
        // Attempt to extract course/speed.  Speed in knots.
        if (!extract_speed_course(p2,speed,course)) {
            // No speed/course to extract
//fprintf(stderr,"No speed/course found\n");
            ok = 0;
            return(ok);
        }
    }
    else {  // Not enough characters for speed/course.  Must have
            // guessed wrong on what type of data it is.
//fprintf(stderr,"No speed/course found 2\n");
        ok = 0;
        return(ok);
    }


//fprintf(stderr,"%s\n",data);

    if (ok) {

        // If we got this far, we have speed/course and know what type
        // of storm it is.
//fprintf(stderr,"Speed: %s, Course: %s\n",speed,course);

        ok = get_weather_record(p_station);     // get existing or create new weather record
    }

    if (ok) {
//        p_station->speed_time[0]     = '\0';

        p_station->weather_data->wx_storm = 1;  // We found a storm

        // Note that speed is in knots.  If we were stuffing it into
        // "wx_speed" we'd have to convert it to MPH.
        if (strcmp(speed,"   ") != 0 && strcmp(speed,"...") != 0) {
            xastir_snprintf(p_station->speed,
                sizeof(p_station->speed),
                "%s",
                speed);
        }
        else {
            p_station->speed[0] = '\0';
        }

        if (strcmp(course,"   ") != 0 && strcmp(course,"...") != 0)
            xastir_snprintf(p_station->course,
                sizeof(p_station->course),
                "%s",
                course);
        else
            p_station->course[0] = '\0';
 
        weather = p_station->weather_data;
 
        p2++;   // Skip the description text, "/TS", "/HC", "/TD", "/TY", "/ST", or "/SC"

        // Extract the sustained wind speed in knots
        if(extract_weather_item(p2,'/',3,weather->wx_speed))
            // Convert from knots to MPH
            xastir_snprintf(weather->wx_speed,
                sizeof(weather->wx_speed),
                "%0.1f",
                (float)(atoi(weather->wx_speed)) * 1.1508);

//fprintf(stderr,"%s\n",data);

        // Extract gust speed in knots
        if (extract_weather_item(p2,'^',3,weather->wx_gust)) // gust (peak wind speed in knots)
            // Convert from knots to MPH
            xastir_snprintf(weather->wx_gust,
                sizeof(weather->wx_gust),
                "%0.1f",
                (float)(atoi(weather->wx_gust)) * 1.1508);

//fprintf(stderr,"%s\n",data);

        // Pressure is already in millibars/hPa.  No conversion
        // needed.
        if (extract_weather_item(p2,'/',4,weather->wx_baro))  // barometric pressure (1/10 mbar / 1/10 hPascal)
            xastir_snprintf(weather->wx_baro,
                sizeof(weather->wx_baro),
                "%0.1f",
                (float)(atoi(weather->wx_baro)));

//fprintf(stderr,"%s\n",data);

        (void)extract_weather_item(p2,'>',3,weather->wx_hurricane_radius); // Nautical miles

//fprintf(stderr,"%s\n",data);

        (void)extract_weather_item(p2,'&',3,weather->wx_trop_storm_radius); // Nautical miles

//fprintf(stderr,"%s\n",data);

        (void)extract_weather_item(p2,'%',3,weather->wx_whole_gale_radius); // Nautical miles

//fprintf(stderr,"%s\n",data);

        // Create a timestamp from the current time
        xastir_snprintf(weather->wx_time,
            sizeof(weather->wx_time),
            "%s",
            get_time(time_data));

        // Set the timestamp in the weather record so that we can
        // decide whether or not to "ghost" the weather data later.
        weather->wx_sec_time=sec_now();
    }
    return(ok);
}



/*
 *  Free station memory for one entry
 */
void delete_station_memory(AprsDataRow *p_del) {
    if (p_del == NULL)
        return;
    
    remove_name(p_del);
    remove_time(p_del);
    free(p_del);
    station_count--;
}

/*
 *  Create new uninitialized element in station list
 *  and insert it before p_name after p_time entries.
 *
 *  Returns NULL if malloc error.
 */
/*@null@*/ AprsDataRow *insert_new_station(AprsDataRow *p_name, AprsDataRow *p_time) {
    AprsDataRow *p_new;


    p_new = (AprsDataRow *)malloc(sizeof(AprsDataRow));

    if (p_new != NULL) {                // we really got the memory
        p_new->call_sign[0] = '\0';     // just to be sure
        p_new->n_next = NULL;
        p_new->n_prev = NULL;
        p_new->t_newer = NULL;
        p_new->t_older = NULL;
        insert_name(p_new,p_name);      // insert element into name ordered list
        insert_time(p_new,p_time);      // insert element into time ordered list
    }
    else {  // p_new == NULL
        fprintf(stderr,"ERROR: we got no memory for station storage\n");
    }


    return(p_new);                      // return pointer to new element
}


// Station storage is done in a double-linked list. In fact there are two such
// pointer structures, one for sorting by name and one for sorting by time.
// We store both the pointers to the next and to the previous elements.  DK7IN

/*
 *  Setup station storage structure
 */
void init_station_data(void) {

    station_count = 0;                  // empty station list
    n_first = NULL;                     // pointer to next element in name sorted list
    n_last  = NULL;                     // pointer to previous element in name sorted list
    t_oldest = NULL;                     // pointer to oldest element in time sorted list
    t_newest  = NULL;                     // pointer to newest element in time sorted list
    last_sec = sec_now();               // check value for detecting changed seconds in time
    next_time_sn = 0;                   // serial number for unique time index
//    current_trail_color = 0x00;         // first trail color used will be 0x01
    last_station_remove = sec_now();    // last time we checked for stations to remove
    
}


/*
 *  Initialize station data
 */        
void init_station(AprsDataRow *p_station) {
    // the list pointers should already be set
	
    p_station->oldest_trackpoint  = NULL;         // no trail
    p_station->newest_trackpoint  = NULL;         // no trail
    p_station->trail_color        = 0;
    p_station->weather_data       = NULL;         // no weather
    p_station->coord_lat          = 0l;           //  90°N  \ undefined
    p_station->coord_lon          = 0l;           // 180°W  / position
    p_station->pos_amb            = 0;            // No ambiguity
    p_station->error_ellipse_radius = 600;        // In cm, default 6 meters
    p_station->lat_precision      = 60;           // In 100ths of seconds latitude (60 = 0.01 minutes)
    p_station->lon_precision      = 60;           // In 100ths of seconds longitude (60 = 0.01 minutes)
    p_station->call_sign[0]       = '\0';         // ?????
    p_station->tactical_call_sign = NULL;
    p_station->sec_heard          = 0;
    p_station->time_sn            = 0;
    p_station->flag               = 0;            // set all flags to inactive
    p_station->object_retransmit  = -1;           // transmit forever
    p_station->last_transmit_time = sec_now();    // Used for object/item decaying algorithm
    p_station->transmit_time_increment = 0;       // Used in data_add()
//    p_station->last_modified_time = 0;            // Used for object/item dead-reckoning
    p_station->record_type        = '\0';
    p_station->heard_via_tnc_port = 0;
    p_station->heard_via_tnc_last_time = 0;
    p_station->last_port_heard    = 0;
    p_station->num_packets        = 0;
    p_station->aprs_symbol.aprs_type = '\0';
    p_station->aprs_symbol.aprs_symbol = '\0';
    p_station->aprs_symbol.special_overlay = '\0';
    p_station->aprs_symbol.area_object.type           = AREA_NONE;
    p_station->aprs_symbol.area_object.color          = AREA_GRAY_LO;
    p_station->aprs_symbol.area_object.sqrt_lat_off   = 0;
    p_station->aprs_symbol.area_object.sqrt_lon_off   = 0;
    p_station->aprs_symbol.area_object.corridor_width = 0;
//    p_station->station_time_type  = '\0';
    p_station->origin[0]          = '\0';        // no object
    p_station->packet_time[0]     = '\0';
    p_station->node_path_ptr      = NULL;
    p_station->pos_time[0]        = '\0';
//    p_station->altitude_time[0]   = '\0';
    p_station->altitude[0]        = '\0';
//    p_station->speed_time[0]      = '\0';
    p_station->speed[0]           = '\0';
    p_station->course[0]          = '\0';
    p_station->bearing[0]         = '\0';
    p_station->NRQ[0]             = '\0';
    p_station->power_gain[0]      = '\0';
    p_station->signal_gain[0]     = '\0';
    p_station->signpost[0]        = '\0';
    p_station->probability_min[0] = '\0';
    p_station->probability_max[0] = '\0';
//    p_station->station_time[0]    = '\0';
    p_station->sats_visible[0]    = '\0';
    p_station->status_data        = NULL;
    p_station->comment_data       = NULL;
    p_station->df_color           = -1;
    
    // Show that there are no other points associated with this
    // station. We could also zero all the entries of the 
    // multipoints[][] array, but nobody should be looking there
    // unless this is non-zero.
    // KG4NBB
    
    p_station->num_multipoints = 0;
    p_station->multipoint_data = NULL;
}



void init_tactical_hash(int clobber) {

    // make sure we don't leak
    if (tactical_hash) {
        if (clobber) {
            hashtable_destroy(tactical_hash, 1);
            tactical_hash=create_hashtable(TACTICAL_HASH_SIZE,
                tactical_hash_from_key,
                tactical_keys_equal);
        }
    }
    else {
        tactical_hash=create_hashtable(TACTICAL_HASH_SIZE,
            tactical_hash_from_key,
            tactical_keys_equal);
    }
}




char *get_tactical_from_hash(char *callsign) {
    char *result;

    if (callsign == NULL || *callsign == '\0') {
        fprintf(stderr,"Empty callsign passed to get_tactical_from_hash()\n");
        return(NULL);
    }

    if (!tactical_hash) {  // no table to search
//fprintf(stderr,"Creating hash table\n");
        init_tactical_hash(1); // so create one
        return NULL;
    }

//    fprintf(stderr,"   searching for %s...",callsign);

    result=hashtable_search(tactical_hash,callsign);

        if (result) {
//            fprintf(stderr,"\t\tFound it, %s, len=%d, %s\n",
//                callsign,
//                strlen(callsign),
//                result);
        } else {
//            fprintf(stderr,"\t\tNot found, %s, len=%d\n",
//                callsign,
//                strlen(callsign));
        }

    return (result);
}


// Distance calculation (Great Circle) using the Haversine formula
// (2-parameter arctan version), which gives better accuracy than
// the "Law of Cosines" for short distances.  It should be
// equivalent to the "Law of Cosines for Spherical Trigonometry" for
// longer distances.  Haversine is a great-circle calculation.
//
//
// Inputs:  lat1/long1/lat2/long2 in radians (double)
//
// Outputs: Distance in meters between them (double)
//
double calc_distance_haversine_radian(double lat1, double lon1, double lat2, double lon2) {
    double dlon, dlat;
    double a, c, d;
    double R = EARTH_RADIUS_METERS;
    #define square(x) (x)*(x)


    dlon = lon2 - lon1;
    dlat = lat2 - lat1;
    a = square((sin(dlat/2.0))) + cos(lat1) * cos(lat2) * square((sin(dlon/2.0)));
    c = 2.0 * atan2(sqrt(a), sqrt(1.0-a));
    d = R * c;

    return(d);
}






/*
 *  Insert existing element into name ordered list before p_name.
 *  If p_name is NULL then we add it to the end instead.
 */
void insert_name(AprsDataRow *p_new, AprsDataRow *p_name) {

    // Set up pointer to next record (or NULL), sorted by name
    p_new->n_next = p_name;

    if (p_name == NULL) {       // Add to end of list

        p_new->n_prev = n_last;

        if (n_last == NULL)     // If we have an empty list
            n_first = p_new;    // Add it to the head of the list

        else    // List wasn't empty, add to the end of the list.
            n_last->n_next = p_new;

        n_last = p_new;
    }

    else {  // Insert new record ahead of p_name record

        p_new->n_prev = p_name->n_prev;

        if (p_name->n_prev == NULL)     // add to begin of list
            n_first = p_new;
        else
            p_name->n_prev->n_next = p_new;

        p_name->n_prev = p_new;
    }
}



// Update all of the pointers so that they accurately reflect the
// current state of the station database.
//
// NOTE:  This part of the code could be made smarter so that the
// pointers are updated whenever they are found to be out of whack,
// instead of zeroing all of them and starting from scratch each
// time.  Alternate:  Follow the current pointer if non-NULL then go
// up/down the list to find the current switchover point between
// letters.
//
// Better:  Tie into the station insert function.  If a new letter
// is inserted, or a new station at the beginning of a letter group,
// run this function to keep things up to date.  That way we won't
// have to traverse in both directions to find a callsign in the
// search_station_name() function.
//
// If hash_key_in is -1, we need to redo all of the hash keys.  If
// it is between 0 and 16383, then we need to redo just that one
// hash key.  The 2nd parameter is either NULL for a removed record,
// or a pointer to a new station record in the case of an addition.
//
void station_shortcuts_update_function(int hash_key_in, AprsDataRow *p_rem) {
    int ii;
    AprsDataRow *ptr;
    int prev_hash_key = 0x0000;
    int hash_key;


// I just changed the function so that we can pass in the hash_key
// that we wish to update:  We should be able to speed things up by
// updating one hash key instead of all 16384 pointers.

    if ( (hash_key_in != -1)
            && (hash_key_in >= 0)
            && (hash_key_in < 16384) ) {

        // We're adding/changing a hash key entry
        station_shortcuts[hash_key_in] = p_rem;
//fprintf(stderr,"%i ",hash_key_in);
    }
    else {  // We're removing a hash key entry.

        // Clear and rebuild the entire hash table.

//??????????????????????????????????????????????????
    // Clear all of the pointers before we begin????
//??????????????????????????????????????????????????
        for (ii = 0; ii < 16384; ii++) {
            station_shortcuts[ii] = NULL;
        }

        ptr = n_first;  // Start of list


        // Loop through entire list, writing the pointer into the
        // station_shortcuts array whenever a new character is
        // encountered.  Do this until the end of the array or the end
        // of the list.
        //
        while ( (ptr != NULL) && (prev_hash_key < 16384) ) {

            // We create the hash key out of the lower 7 bits of the
            // first two characters, creating a 14-bit key (1 of 16384)
            //
            hash_key = (int)((ptr->call_sign[0] & 0x7f) << 7);
            hash_key = hash_key | (int)(ptr->call_sign[1] & 0x7f);

            if (hash_key > prev_hash_key) {

                // We found the next hash_key.  Store the pointer at the
                // correct location.
                if (hash_key < 16384) {
                    station_shortcuts[hash_key] = ptr;

                }
                prev_hash_key = hash_key;
            }
            ptr = ptr->n_next;
        }

    }

}




/*
 *  Create new initialized element for call in station list
 *  and insert it before p_name after p_time entries.
 *
 *  Returns NULL if mallc error.
 */
/*@null@*/ AprsDataRow *add_new_station(AprsDataRow *p_name, AprsDataRow *p_time, char *call) {
    AprsDataRow *p_new;
    int hash_key;   // We use a 14-bit hash key
    char *tactical_call;



    if (call[0] == '\0') {
        // Do nothing.  No update needed.  Callsign is empty.
        return(NULL);
    }

	 
    if(_aprs_show_new_station_alert)
    {
	    const gchar *msg = g_strdup_printf("New station: %s", call); 
	    hildon_banner_show_information(_window, NULL, msg);
    }

    p_new = insert_new_station(p_name,p_time);  // allocate memory

    if (p_new == NULL) {

        // Couldn't allocate space for the station
        return(NULL);
    }

    init_station(p_new);                    // initialize new station record
    
    
    xastir_snprintf(p_new->call_sign,
        sizeof(p_new->call_sign),
        "%s",
        call);
    station_count++;



    // Do some quick checks to see if we just inserted a new hash
    // key or inserted at the beginning of a hash key (making the
    // old pointer incorrect).  If so, update our pointers to match.

    // We create the hash key out of the lower 7 bits of the first
    // two characters, creating a 14-bit key (1 of 16384)
    //
    hash_key = (int)((call[0] & 0x7f) << 7);
    hash_key = hash_key | (int)(call[1] & 0x7f);

    if (station_shortcuts[hash_key] == NULL) {
        // New hash key entry point found.  Fill in the pointer.
        station_shortcuts_update_function(hash_key, p_new);
    }
    else if (p_new->n_prev == NULL) {
        // We just inserted at the beginning of the list.  Assume
        // that we inserted at the beginning of our hash_key
        // segment.
        station_shortcuts_update_function(hash_key, p_new);
    }
    else {
        // Check whether either of the first two chars of the new
        // callsign and the previous callsign are different.  If so,
        // we need to update the hash table entry for our new record
        // 'cuz we're at the start of a new hash table entry.
        if (p_new->n_prev->call_sign[0] != call[0]
                || p_new->n_prev->call_sign[1] != call[1]) {

            station_shortcuts_update_function(hash_key, p_new);
        }
    }

    // Check whether we have a tactical call to assign to this
    // station in our tactical hash table.

    tactical_call = get_tactical_from_hash(call);


    // If tactical call found and not blank
    if (tactical_call && tactical_call[0] != '\0') {

        // Malloc some memory to hold it in the station record.
        p_new->tactical_call_sign = (char *)malloc(MAX_TACTICAL_CALL+1);
        CHECKMALLOC(p_new->tactical_call_sign);

        snprintf(p_new->tactical_call_sign,
            MAX_TACTICAL_CALL+1,
            "%s",
            tactical_call);

        //if (tactical_call[0] == '\0')
        //    fprintf(stderr,"Blank tactical call\n");
    }

    return(p_new);                      // return pointer to new element
}



/*
 *  Add data from APRS information field to station database
 *  Returns a 1 if successful
 */
int data_add(gint type,
             gchar *call_sign,
             gchar *path,
             gchar *data,
             TAprsPort port,
             gchar *origin,
             gint third_party,
             gint station_is_mine,
             gint object_is_mine) {


    AprsDataRow *p_station;
    AprsDataRow *p_time;
    char call[MAX_CALLSIGN+1];
    char new_station;
    long last_lat, last_lon;
    char last_alt[MAX_ALTITUDE];
    char last_speed[MAX_SPEED+1];
    char last_course[MAX_COURSE+1];
    time_t last_stn_sec;
    short last_flag;
    char temp_data[40]; // short term string storage, MAX_CALLSIGN, ...
//    long l_lat, l_lon;
    double distance;
//    char station_id[600];
    int found_pos;
    float value;
    AprsWeatherRow *weather;
    int moving;
    int changed_pos;
    int screen_update;
    int ok, store;
    int ok_to_display;
    int compr_pos;
    char *p = NULL; // KC2ELS - used for WIDEn-N
    int direct = 0;
    int object_is_mine_previous = 0;
    int new_origin_is_mine = 0;
    int num_digits = 0; // Number of digits after decimal point in NMEA string
    
// TODO update based on time
//    static time_t lastScreenUpdate;


    // call and path had been validated before
    // Check "data" against the max APRS length, and dump the packet if too long.
    if ( (data != NULL) && (strlen(data) > MAX_INFO_FIELD_SIZE) ) {   
    	// Overly long packet.  Throw it away.

        return(0);  // Not an ok packet
    }

    // Check for some reasonable string in call_sign parameter
    if (call_sign == NULL || strlen(call_sign) == 0) {

        return(0);
    }
 

    if (origin && is_my_call(origin, 1)) {
        new_origin_is_mine++;   // The new object/item is owned by me
    }

    weather = NULL; // only to make the compiler happy...
    found_pos = 1;
    snprintf(call,
        sizeof(call),
        "%s",
        call_sign);
    p_station = NULL;
    new_station = (char)FALSE;                          // to make the compiler happy...
    last_lat = 0L;
    last_lon = 0L;
    last_stn_sec = sec_now();
    last_alt[0]    = '\0';
    last_speed[0]  = '\0';
    last_course[0] = '\0';
    last_flag      = 0;
    ok = 0;
    store = 0;
    p_time = NULL;                                      // add to end of time sorted list (newest)
    compr_pos = 0;


    if (search_station_name(&p_station,call,1)) 
    {       // If we found the station in our list
 

//    	fprintf(stderr, "DEBUG: Station:,Found,:%s:\n", call);
    	
        // Check whether it's already a locally-owned object/item
        if (is_my_object_item(p_station)) {

            // We don't want to re-order it in the time-ordered list
            // so that it'll expire from the queue normally.  Don't
            // call "move_station_time()" here.

            // We need an exception later in this function for the
            // case where we've moved an object/item (by how much?).
            // We need to update the time in this case so that it'll
            // expire later (in fact it could already be expired
            // when we move it).  We should be able to move expired
            // objects/items to make them active again.  Perhaps
            // some other method as well?

            new_station = (char)FALSE;
            object_is_mine_previous++;
        }
        else {
            move_station_time(p_station,p_time);        // update time, change position in time sorted list
            new_station = (char)FALSE;                  // we have seen this one before
        }

        if (is_my_station(p_station)) {
            station_is_mine++; // Station/object/item is owned/controlled by me
        }
    }
    else 
    {
//    	fprintf(stderr, "DEBUG: Station:,New,:%s:\n", call);

        p_station = add_new_station(p_station,p_time,call);     // create storage
        new_station = (char)TRUE;                       // for new station

    }
    

    if (p_station != NULL) 
    {

        last_lat = p_station->coord_lat;                // remember last position
        last_lon = p_station->coord_lon;
        last_stn_sec = p_station->sec_heard;
        snprintf(last_alt,
            sizeof(last_alt),
            "%s",
            p_station->altitude);
        snprintf(last_speed,
            sizeof(last_speed),
            "%s",
            p_station->speed);
        snprintf(last_course,
            sizeof(last_course),    
            "%s",
            p_station->course);
        last_flag = p_station->flag;

        // Wipe out old data so that it doesn't hang around forever
        p_station->altitude[0] = '\0';
        p_station->speed[0] = '\0';
        p_station->course[0] = '\0';

        ok = 1;                         // succeed as default


        switch (type) {

            case (APRS_MICE):           // Mic-E format
            case (APRS_FIXED):          // '!'
            case (APRS_MSGCAP):         // '='

                if (!extract_position(p_station,&data,type)) {          // uncompressed lat/lon
                    compr_pos = 1;
                    if (!extract_comp_position(p_station,&data,type))   // compressed lat/lon
                        ok = 0;
                    else
                        p_station->pos_amb = 0; // No ambiguity in compressed posits
                }

                if (ok) {

                    // Create a timestamp from the current time
                    snprintf(p_station->pos_time,
                        sizeof(p_station->pos_time),
                        "%s", get_time(temp_data));
                    (void)extract_storm(p_station,data,compr_pos);
                    (void)extract_weather(p_station,data,compr_pos);    // look for weather data first
                    process_data_extension(p_station,data,type);        // PHG, speed, etc.
                    process_info_field(p_station,data,type);            // altitude

                    if ( (p_station->coord_lat > 0) && (p_station->coord_lon > 0) )
                        extract_multipoints(p_station, data, type, 1);
 
                    add_comment(p_station,data);

                    p_station->record_type = NORMAL_APRS;
                    if (type == APRS_MSGCAP)
                        p_station->flag |= ST_MSGCAP;           // set "message capable" flag
                    else
                        p_station->flag &= (~ST_MSGCAP);        // clear "message capable" flag

                    // Assign a non-default value for the error
                    // ellipse?
                    if (type == APRS_MICE || !compr_pos) {
                        p_station->error_ellipse_radius = 2550; // 25.5m, or about 60ft resolution
                        p_station->lat_precision = 60;
                        p_station->lon_precision = 60;
                    }
                    else {
                        p_station->error_ellipse_radius = 600; // Default of 6m
                        p_station->lat_precision = 6;
                        p_station->lon_precision = 6;
                    }

                }
                break;

/*
            case (APRS_DOWN):           // '/'
                ok = extract_time(p_station, data, type);               // we need a time
                if (ok) {
                    if (!extract_position(p_station,&data,type)) {      // uncompressed lat/lon
                        compr_pos = 1;
                        if (!extract_comp_position(p_station,&data,type)) // compressed lat/lon
                            ok = 0;
                        else
                            p_station->pos_amb = 0; // No ambiguity in compressed posits
                    }
                }

                if (ok) {

                    // Create a timestamp from the current time
                    xastir_snprintf(p_station->pos_time,
                        sizeof(p_station->pos_time),
                        "%s",
                        get_time(temp_data));
                    process_data_extension(p_station,data,type);        // PHG, speed, etc.
                    process_info_field(p_station,data,type);            // altitude

                    if ( (p_station->coord_lat > 0) && (p_station->coord_lon > 0) )
                        extract_multipoints(p_station, data, type, 1);
 
                    add_comment(p_station,data);

                    p_station->record_type = DOWN_APRS;
                    p_station->flag &= (~ST_MSGCAP);            // clear "message capable" flag

                    // Assign a non-default value for the error
                    // ellipse?
                    if (!compr_pos) {
                        p_station->error_ellipse_radius = 2550; // 25.5m, or about 60ft resolution
                        p_station->lat_precision = 60;
                        p_station->lon_precision = 60;
                    }
                    else {
                        p_station->error_ellipse_radius = 600; // Default of 6m
                        p_station->lat_precision = 6;
                        p_station->lon_precision = 6;
                    }
                }
                break;
*/

            case (APRS_DF):             // '@'
            case (APRS_MOBILE):         // '@'

                ok = extract_time(p_station, data, type);               // we need a time
                if (ok) {
                    if (!extract_position(p_station,&data,type)) {      // uncompressed lat/lon
                        compr_pos = 1;
                        if (!extract_comp_position(p_station,&data,type)) // compressed lat/lon
                            ok = 0;
                        else
                            p_station->pos_amb = 0; // No ambiguity in compressed posits
                    }
                }
                if (ok) {

                    process_data_extension(p_station,data,type);        // PHG, speed, etc.
                    process_info_field(p_station,data,type);            // altitude

                    if ( (p_station->coord_lat > 0) && (p_station->coord_lon > 0) )
                        extract_multipoints(p_station, data, type, 1);
 
                    add_comment(p_station,data);

                    if(type == APRS_MOBILE)
                        p_station->record_type = MOBILE_APRS;
                    else
                        p_station->record_type = DF_APRS;
                    //@ stations have messaging per spec
                    p_station->flag |= (ST_MSGCAP);            // set "message capable" flag

                    // Assign a non-default value for the error
                    // ellipse?
                    if (!compr_pos) {
                        p_station->error_ellipse_radius = 2550; // 25.5m, or about 60ft resolution
                        p_station->lat_precision = 60;
                        p_station->lon_precision = 60;
                    }
                    else {
                        p_station->error_ellipse_radius = 600; // Default of 6m
                        p_station->lat_precision = 6;
                        p_station->lon_precision = 6;
                    }
                }
                break;

            case (APRS_GRID):

                ok = extract_position(p_station, &data, type);
                if (ok) { 


                    process_info_field(p_station,data,type);            // altitude

                    if ( (p_station->coord_lat > 0) && (p_station->coord_lon > 0) )
                        extract_multipoints(p_station, data, type, 1);
 
                    add_comment(p_station,data);

                    // Assign a non-default value for the error
                    // ellipse?
//                    p_station->error_ellipse_radius = 2550; // 25.5m, or about 60ft resolution

// WE7U
// This needs to change based on the number of grid letters/digits specified
//                    p_station->lat_precision = 60;
//                    p_station->lon_precision = 60;
                }
                else {
                }
                break;

            case (STATION_CALL_DATA):

                p_station->record_type = NORMAL_APRS;
                found_pos = 0;
                break;

            case (APRS_STATUS):         // '>' Status Reports     [APRS Reference, chapter 16]

                (void)extract_time(p_station, data, type);              // we need a time
                // todo: could contain Maidenhead or beam heading+power

                if ( (p_station->coord_lat > 0) && (p_station->coord_lon > 0) )
                    extract_multipoints(p_station, data, type, 1);
 
                add_status(p_station,data);

                p_station->flag |= (ST_STATUS);                         // set "Status" flag
                p_station->record_type = NORMAL_APRS;                   // ???
                found_pos = 0;
                break;

            case (OTHER_DATA):          // Other Packets          [APRS Reference, chapter 19]

                // non-APRS beacons, treated as status reports until we get a real one

                if ( (p_station->coord_lat > 0) && (p_station->coord_lon > 0) )
                    extract_multipoints(p_station, data, type, 1);
 
                if ((p_station->flag & (~ST_STATUS)) == 0) {            // only store if no status yet

                    add_status(p_station,data);

                    p_station->record_type = NORMAL_APRS;               // ???
                }
                found_pos = 0;
                break;

            case (APRS_OBJECT):

                // If old match is a killed Object (owner doesn't
                // matter), new one is an active Object and owned by
                // us, remove the old record and create a new one
                // for storing this Object.  Do the same for Items
                // in the next section below.
                //
                // The easiest implementation might be to remove the
                // old record and then call this routine again with
                // the same parameters, which will cause a brand-new
                // record to be created.
                //
                // The new record we're processing is an active
                // object, as data_add() won't be called on a killed
                // object.
                //
//                if ( is_my_call(origin,1)  // If new Object is owned by me (including SSID)
                if (new_origin_is_mine
                        && !(p_station->flag & ST_ACTIVE)
                        && (p_station->flag & ST_OBJECT) ) {  // Old record was a killed Object
                    remove_name(p_station);  // Remove old killed Object
//                    redo_list = (int)TRUE;
                    return( data_add(type, call_sign, path, data, port, origin, third_party, 1, 1) );
                }
 
                ok = extract_time(p_station, data, type);               // we need a time
                if (ok) {
                    if (!extract_position(p_station,&data,type)) {      // uncompressed lat/lon
                        compr_pos = 1;
                        if (!extract_comp_position(p_station,&data,type)) // compressed lat/lon
                            ok = 0;
                        else
                            p_station->pos_amb = 0; // No ambiguity in compressed posits
                    }
                }
                p_station->flag |= ST_OBJECT;                           // Set "Object" flag
                if (ok) {

                    // If object was owned by me but another station
                    // is transmitting it now, write entries into
                    // the object.log file showing that we don't own
                    // this object anymore.

//                    if ( (is_my_call(p_station->origin,1))  // If station was owned by me (including SSID)
//                            && (!is_my_call(origin,1)) ) {  // But isn't now
  // TODO                  
/*
                	if (is_my_object_item(p_station)    // If station was owned by me (include SSID)
                            && !new_origin_is_mine) {   // But isn't now

                        disown_object_item(call_sign, origin);

                    }
*/
                    // If station is owned by me (including SSID)
                    // but it's a new object/item
//                    if ( (is_my_call(p_station->origin,1))
                    if (new_origin_is_mine
                            && (p_station->transmit_time_increment == 0) ) {
                        // This will get us transmitting this object
                        // on the decaying algorithm schedule.
                        // We've transmitted it once if we've just
                        // gotten to this code.
                        p_station->transmit_time_increment = OBJECT_CHECK_RATE;
//fprintf(stderr,"data_add(): Setting transmit_time_increment to %d\n", OBJECT_CHECK_RATE);
                    }
 
                    // Create a timestamp from the current time
                    xastir_snprintf(p_station->pos_time,
                        sizeof(p_station->pos_time),
                        "%s",
                        get_time(temp_data));

                    xastir_snprintf(p_station->origin,
                        sizeof(p_station->origin),
                        "%s",
                        origin);                   // define it as object
                    (void)extract_storm(p_station,data,compr_pos);
                    (void)extract_weather(p_station,data,compr_pos);    // look for wx info
                    process_data_extension(p_station,data,type);        // PHG, speed, etc.
                    process_info_field(p_station,data,type);            // altitude

                    if ( (p_station->coord_lat > 0) && (p_station->coord_lon > 0) )
                        extract_multipoints(p_station, data, type, 0);
 
                    add_comment(p_station,data);

                    // the last char always was missing...
                    //p_station->comments[ strlen(p_station->comments) - 1 ] = '\0';  // Wipe out '\n'
                    // moved that to decode_ax25_line
                    // and don't added a '\n' in interface.c
                    p_station->record_type = NORMAL_APRS;
                    p_station->flag &= (~ST_MSGCAP);            // clear "message capable" flag

                    // Assign a non-default value for the error
                    // ellipse?
                    if (!compr_pos) {
                        p_station->error_ellipse_radius = 2550; // 25.5m, or about 60ft resolution
                        p_station->lat_precision = 60;
                        p_station->lon_precision = 60;
                    }
                    else {
                        p_station->error_ellipse_radius = 600; // Default of 6m
                        p_station->lat_precision = 6;
                        p_station->lon_precision = 6;
                    }
                }
                break;

            case (APRS_ITEM):

                // If old match is a killed Item (owner doesn't
                // matter), new one is an active Item and owned by
                // us, remove the old record and create a new one
                // for storing this Item.  Do the same for Objects
                // in the previous section above.
                //
                // The easiest implementation might be to remove the
                // old record and then call this routine again with
                // the same parameters, which will cause a brand-new
                // record to be created.
                //
                // The new record we're processing is an active
                // Item, as data_add() won't be called on a killed
                // Item.
                //
//                if ( is_my_call(origin,1)  // If new Item is owned by me (including SSID)
                if (new_origin_is_mine
                        && !(p_station->flag & ST_ACTIVE)
                        && (p_station->flag & ST_ITEM) ) {  // Old record was a killed Item
 
                    remove_name(p_station);  // Remove old killed Item
//                    redo_list = (int)TRUE;
                    return( data_add(type, call_sign, path, data, port, origin, third_party, 1, 1) );
                }
 
                if (!extract_position(p_station,&data,type)) {          // uncompressed lat/lon
                    compr_pos = 1;
                    if (!extract_comp_position(p_station,&data,type))   // compressed lat/lon
                        ok = 0;
                    else
                        p_station->pos_amb = 0; // No ambiguity in compressed posits
                }
                p_station->flag |= ST_ITEM;                             // Set "Item" flag
                if (ok) {

                    // If item was owned by me but another station
                    // is transmitting it now, write entries into
                    // the object.log file showing that we don't own
                    // this item anymore.
// TODO
/*
//                    if ( (is_my_call(p_station->origin,1))  // If station was owned by me (including SSID)
//                            && (!is_my_call(origin,1)) ) {  // But isn't now
                    if (is_my_object_item(p_station)
                            && !new_origin_is_mine) {  // But isn't now

                        disown_object_item(call_sign,origin);
                    }
 */

                    // If station is owned by me (including SSID)
                    // but it's a new object/item
//                    if ( (is_my_call(p_station->origin,1))
                    if (is_my_object_item(p_station)
                            && (p_station->transmit_time_increment == 0) ) {
                        // This will get us transmitting this object
                        // on the decaying algorithm schedule.
                        // We've transmitted it once if we've just
                        // gotten to this code.
                        p_station->transmit_time_increment = OBJECT_CHECK_RATE;
//fprintf(stderr,"data_add(): Setting transmit_time_increment to %d\n", OBJECT_CHECK_RATE);
                    }
 
                    // Create a timestamp from the current time
                    xastir_snprintf(p_station->pos_time,
                        sizeof(p_station->pos_time),
                        "%s",
                        get_time(temp_data));
                    xastir_snprintf(p_station->origin,
                        sizeof(p_station->origin),
                        "%s",
                        origin);                   // define it as item
                    (void)extract_storm(p_station,data,compr_pos);
                    (void)extract_weather(p_station,data,compr_pos);    // look for wx info
                    process_data_extension(p_station,data,type);        // PHG, speed, etc.
                    process_info_field(p_station,data,type);            // altitude

                    if ( (p_station->coord_lat > 0) && (p_station->coord_lon > 0) )
                        extract_multipoints(p_station, data, type, 0);
 
                    add_comment(p_station,data);

                    // the last char always was missing...
                    //p_station->comments[ strlen(p_station->comments) - 1 ] = '\0';  // Wipe out '\n'
                    // moved that to decode_ax25_line
                    // and don't added a '\n' in interface.c
                    p_station->record_type = NORMAL_APRS;
                    p_station->flag &= (~ST_MSGCAP);            // clear "message capable" flag

                    // Assign a non-default value for the error
                    // ellipse?
                    if (!compr_pos) {
                        p_station->error_ellipse_radius = 2550; // 25.5m, or about 60ft resolution
                        p_station->lat_precision = 60;
                        p_station->lon_precision = 60;
                    }
                    else {
                        p_station->error_ellipse_radius = 600; // Default of 6m
                        p_station->lat_precision = 6;
                        p_station->lon_precision = 6;
                    }
                }
                break;

            case (APRS_WX1):    // weather in '@' or '/' packet

                ok = extract_time(p_station, data, type);               // we need a time
                if (ok) {
                    if (!extract_position(p_station,&data,type)) {      // uncompressed lat/lon
                        compr_pos = 1;
                        if (!extract_comp_position(p_station,&data,type)) // compressed lat/lon
                            ok = 0;
                        else
                            p_station->pos_amb = 0; // No ambiguity in compressed posits
                    }
                }
                if (ok) {

                    (void)extract_storm(p_station,data,compr_pos);
                    (void)extract_weather(p_station,data,compr_pos);
                    p_station->record_type = (char)APRS_WX1;

                    process_info_field(p_station,data,type);            // altitude

                    if ( (p_station->coord_lat > 0) && (p_station->coord_lon > 0) )
                        extract_multipoints(p_station, data, type, 1);
 
                    add_comment(p_station,data);

                    // Assign a non-default value for the error
                    // ellipse?
                    if (!compr_pos) {
                        p_station->error_ellipse_radius = 2550; // 25.5m, or about 60ft resolution
                        p_station->lat_precision = 60;
                        p_station->lon_precision = 60;
                    }
                    else {
                        p_station->error_ellipse_radius = 600; // Default of 6m
                        p_station->lat_precision = 6;
                        p_station->lon_precision = 6;
                    }
                }
                break;

            case (APRS_WX2):            // '_'

                ok = extract_time(p_station, data, type);               // we need a time
                if (ok) {
                    (void)extract_storm(p_station,data,compr_pos);
                    (void)extract_weather(p_station,data,0);            // look for weather data first
                    p_station->record_type = (char)APRS_WX2;
                    found_pos = 0;

                    process_info_field(p_station,data,type);            // altitude

                    if ( (p_station->coord_lat > 0) && (p_station->coord_lon > 0) )
                        extract_multipoints(p_station, data, type, 1);
                }
                break;

            case (APRS_WX4):            // '#'          Peet Bros U-II (km/h)
            case (APRS_WX6):            // '*'          Peet Bros U-II (mph)
            case (APRS_WX3):            // '!'          Peet Bros Ultimeter 2000 (data logging mode)
            case (APRS_WX5):            // '$ULTW'      Peet Bros Ultimeter 2000 (packet mode)

            	
                if (get_weather_record(p_station)) {    // get existing or create new weather record
                    weather = p_station->weather_data;
                    if (type == APRS_WX3)   // Peet Bros Ultimeter 2000 data logging mode
                    {
                        decode_U2000_L (1,(unsigned char *)data,weather);
                    }
                    else if (type == APRS_WX5) // Peet Bros Ultimeter 2000 packet mode
                        decode_U2000_P(1,(unsigned char *)data,weather);
                    else    // Peet Bros Ultimeter-II
                        decode_Peet_Bros(1,(unsigned char *)data,weather,type);
                    p_station->record_type = (char)type;
                    // Create a timestamp from the current time
                    xastir_snprintf(weather->wx_time,
                        sizeof(weather->wx_time),
                        "%s",
                        get_time(temp_data));
                    weather->wx_sec_time = sec_now();
                    found_pos = 0;
                }

                break;


// GPRMC, digits after decimal point
// ---------------------------------
// 2  = 25.5 meter error ellipse
// 3  =  6.0 meter error ellipse
// 4+ =  6.0 meter error ellipse


            case (GPS_RMC):             // $GPRMC

// WE7U
// Change this function to return HDOP and the number of characters
// after the decimal point.
                ok = extract_RMC(p_station,data,call_sign,path,&num_digits);

                if (ok) {
                    // Assign a non-default value for the error
                    // ellipse?
//
// WE7U
// Degrade based on the precision provided in the sentence.  If only
// 2 digits after decimal point, give it 2550 as a radius (25.5m).
// Best (smallest) circle should be 600 as we have no augmentation
// flag to check here for anything better.
//
                    switch (num_digits) {

                        case 0:
                            p_station->error_ellipse_radius = 2550; // 25.5m, or about 60ft resolution
                            p_station->lat_precision = 6000;
                            p_station->lon_precision = 6000;
                            break;

                        case 1:
                            p_station->error_ellipse_radius = 2550; // 25.5m, or about 60ft resolution
                            p_station->lat_precision = 600;
                            p_station->lon_precision = 600;
                            break;

                        case 2:
                            p_station->error_ellipse_radius = 600; // Default of 6m
                            p_station->lat_precision = 60;
                            p_station->lon_precision = 60;
                            break;

                        case 3:
                            p_station->error_ellipse_radius = 600; // Default of 6m
                            p_station->lat_precision = 6;
                            p_station->lon_precision = 6;
                            break;

                        case 4:
                        case 5:
                        case 6:
                        case 7:
                            p_station->error_ellipse_radius = 600; // Default of 6m
                            p_station->lat_precision = 0;
                            p_station->lon_precision = 0;
                            break;

                        default:
                            p_station->error_ellipse_radius = 600; // Default of 6m
                            p_station->lat_precision = 60;
                            p_station->lon_precision = 60;
                            break;
                    }
                }
                break;


// GPGGA, digits after decimal point, w/o augmentation
// ---------------------------------------------------
// 2   = 25.5 meter error ellipse
// 3   =  6.0 meter error ellipse unless HDOP>4, then 10.0 meters
// 4+  =  6.0 meter error ellipse unless HDOP>4, then 10.0 meters
// 
// 
// GPGGA, digits after decimal point, w/augmentation
// --------------------------------------------------
// 2   = 25.5 meter error ellipse
// 3   =  2.5 meter error ellipse unless HDOP>4, then 10.0 meters
// 4+  =  0.6 meter error ellipse unless HDOP>4, then 10.0 meters


            case (GPS_GGA):             // $GPGGA

// WE7U
// Change this function to return HDOP and the number of characters
// after the decimal point.
                ok = extract_GGA(p_station,data,call_sign,path,&num_digits);

                if (ok) {
                    // Assign a non-default value for the error
                    // ellipse?
//
// WE7U
// Degrade based on the precision provided in the sentence.  If only
// 2 digits after decimal point, give it 2550 as a radius (25.5m).
// 3 digits: 6m w/o augmentation unless HDOP >4 = 10m, 2.5m w/augmentation.
// 4+ digits: 6m w/o augmentation unless HDOP >4 = 10m, 0.6m w/augmentation.
//
                     switch (num_digits) {

                        case 0:
                            p_station->error_ellipse_radius = 2550; // 25.5m, or about 60ft resolution
                            p_station->lat_precision = 6000;
                            p_station->lon_precision = 6000;
                            break;

                        case 1:
                            p_station->error_ellipse_radius = 2550; // 25.5m, or about 60ft resolution
                            p_station->lat_precision = 600;
                            p_station->lon_precision = 600;
                            break;

                        case 2:
                            p_station->error_ellipse_radius = 600; // Default of 6m
                            p_station->lat_precision = 60;
                            p_station->lon_precision = 60;
                            break;

                        case 3:
                            p_station->error_ellipse_radius = 600; // Default of 6m
                            p_station->lat_precision = 6;
                            p_station->lon_precision = 6;
                            break;

                        case 4:
                        case 5:
                        case 6:
                        case 7:
                            p_station->error_ellipse_radius = 600; // Default of 6m
                            p_station->lat_precision = 0;
                            p_station->lon_precision = 0;
                            break;

                        default:
                            p_station->error_ellipse_radius = 600; // Default of 6m
                            p_station->lat_precision = 60;
                            p_station->lon_precision = 60;
                            break;
                    }
                }
                break;


// GPGLL, digits after decimal point
// ---------------------------------
// 2  = 25.5 meter error ellipse
// 3  =  6.0 meter error ellipse
// 4+ =  6.0 meter error ellipse


            case (GPS_GLL):             // $GPGLL
                ok = extract_GLL(p_station,data,call_sign,path,&num_digits);

                if (ok) {
                    // Assign a non-default value for the error
                    // ellipse?
//
// WE7U
// Degrade based on the precision provided in the sentence.  If only
// 2 digits after decimal point, give it 2550 as a radius, otherwise
// give it 600.
//
                     switch (num_digits) {

                        case 0:
                            p_station->error_ellipse_radius = 2550; // 25.5m, or about 60ft resolution
                            p_station->lat_precision = 6000;
                            p_station->lon_precision = 6000;
                            break;
 
                        case 1:
                            p_station->error_ellipse_radius = 2550; // 25.5m, or about 60ft resolution
                            p_station->lat_precision = 600;
                            p_station->lon_precision = 600;
                            break;

                        case 2:
                            p_station->error_ellipse_radius = 600; // Default of 6m
                            p_station->lat_precision = 60;
                            p_station->lon_precision = 60;
                            break;

                        case 3:
                            p_station->error_ellipse_radius = 600; // Default of 6m
                            p_station->lat_precision = 6;
                            p_station->lon_precision = 6;
                            break;

                        case 4:
                        case 5:
                        case 6:
                        case 7:
                            p_station->error_ellipse_radius = 600; // Default of 6m
                            p_station->lat_precision = 0;
                            p_station->lon_precision = 0;
                            break;

                        default:
                            p_station->error_ellipse_radius = 600; // Default of 6m
                            p_station->lat_precision = 60;
                            p_station->lon_precision = 60;
                            break;
                    }
 
                }
                break;

            default:

                fprintf(stderr,"ERROR: UNKNOWN TYPE in data_add\n");
                ok = 0;
                break;
        }

        // Left this one in, just in case.  Perhaps somebody might
        // attach a multipoint string onto the end of a packet we
        // might not expect.  For this case we need to check whether
        // we have multipoints first, as we don't want to erase the
        // work we might have done with a previous call to
        // extract_multipoints().
        if (ok && (p_station->coord_lat > 0)
                && (p_station->coord_lon > 0)
                && (p_station->num_multipoints == 0) ) {  // No multipoints found yet

            extract_multipoints(p_station, data, type, 0);
        }
    }

    if (!ok) {  // non-APRS beacon, treat it as Other Packet   [APRS Reference, chapter 19]

        // GPRMC etc. without a position is here too, but it should not be stored as status!

        // store it as status report until we get a real one
        if ((p_station->flag & (~ST_STATUS)) == 0) {         // only store it if no status yet
 
            add_status(p_station,data-1);

            p_station->record_type = NORMAL_APRS;               // ???
 
        }
 
        ok = 1;            
        found_pos = 0;
    }
    
    curr_sec = sec_now();
    if (ok) {

        // data packet is valid
        // announce own echo, we soon discard that packet...
//        if (!new_station && is_my_call(p_station->call_sign,1) // Check SSID as well
        if (!new_station
                && is_my_station(p_station) // Check SSID as well
                && strchr(path,'*') != NULL) {
 
            upd_echo(path);   // store digi that echoes my signal...
            statusline("Echo from digipeater",0);   // Echo from digipeater

        }
        // check if data is just a secondary echo from another digi
        if ((last_flag & ST_VIATNC) == 0
                || (curr_sec - last_stn_sec) > 15
                || p_station->coord_lon != last_lon 
                || p_station->coord_lat != last_lat)
 
            store = 1;                     // don't store secondary echos
    }
    
    if (!ok && new_station)
        delete_station_memory(p_station);       // remove unused record
 
    if (store) {
 
        // we now have valid data to store into database
        // make time index unique by adding a serial number

        if (station_is_mine) {
            // This station/object/item is owned by me.  Set the
            // flag which shows that we own/control this
            // station/object/item.  We use this flag later in lieu
            // of the is_my_call() function in order to speed things
            // up.
            //
            p_station->flag |= ST_MYSTATION;
        }
        
        // Check whether it's a locally-owned object/item
        if ( object_is_mine
                || (   new_origin_is_mine
                    && (p_station->flag & ST_ACTIVE)
                    && (p_station->flag & ST_OBJECT) ) ) {

            p_station->flag |= ST_MYOBJITEM;

            // Do nothing else.  We don't want to update the
            // last-heard time so that it'll expire from the queue
            // normally, unless it is a new object/item.
            //
            if (new_station) {
                p_station->sec_heard = curr_sec;
            }

            // We need an exception later in this function for the
            // case where we've moved an object/item (by how much?).
            // We need to update the time in this case so that it'll
            // expire later (in fact it could already be expired
            // when we move it).  We should be able to move expired
            // objects/items to make them active again.  Perhaps
            // some other method as well?.
        }
        else {
            // Reset the "my object" flag
            p_station->flag &= ~ST_MYOBJITEM;

            p_station->sec_heard = curr_sec;    // Give it a new timestamp
        }
        
        if (curr_sec != last_sec) {     // todo: if old time calculate time_sn from database
            last_sec = curr_sec;
            next_time_sn = 0;           // restart time serial number
        }

        p_station->time_sn = next_time_sn++;            // todo: warning if serial number too high
        if (port == APRS_PORT_TTY) {                     // heard via TNC
            if (!third_party) { // Not a third-party packet
                p_station->flag |= ST_VIATNC;               // set "via TNC" flag
                p_station->heard_via_tnc_last_time = curr_sec;
                p_station->heard_via_tnc_port = port;
            }
            else {  // Third-party packet
                // Leave the previous setting of "flag" alone.
                // Specifically do NOT set the ST_VIATNC flag if it
                // was a third-party packet.
            }
        }
        else {  // heard other than TNC
            if (new_station) {  // new station
                p_station->flag &= (~ST_VIATNC);  // clear "via TNC" flag
//fprintf(stderr,"New_station: Cleared ST_VIATNC flag: %s\n", p_station->call_sign);
                p_station->heard_via_tnc_last_time = 0l;
            }
        }
        p_station->last_port_heard = port;
        
        // Create a timestamp from the current time
        snprintf(p_station->packet_time, sizeof(p_station->packet_time),
            "%s", get_time(temp_data)); // get_time returns value in temp_data
    	 
        p_station->flag |= ST_ACTIVE;
        if (third_party)
            p_station->flag |= ST_3RD_PT;               // set "third party" flag
        else
            p_station->flag &= (~ST_3RD_PT);            // clear "third party" flag
        if (origin != NULL && strcmp(origin,"INET") == 0)  // special treatment for inet names
            snprintf(p_station->origin,
                sizeof(p_station->origin),
                "%s",
                origin);           // to keep them separated from calls
        if (origin != NULL && strcmp(origin,"INET-NWS") == 0)  // special treatment for NWS
            snprintf(p_station->origin,
                sizeof(p_station->origin),
                "%s",
                origin);           // to keep them separated from calls
        if (origin == NULL || origin[0] == '\0')        // normal call
            p_station->origin[0] = '\0';                // undefine possible former object with same name


        //--------------------------------------------------------------------

        // KC2ELS
        // Okay, here are the standards for ST_DIRECT:
        // 1.  The packet must have been received via TNC.
        // 2.  The packet must not have any * flags.
        // 3.  If present, the first WIDEn-N (or TRACEn-N) must have n=N.
        // A station retains the ST_DIRECT setting.  If
        // "st_direct_timeout" seconds have passed since we set
        // that bit then APRSD queries and displays based on the
        // ST_DIRECT bit will skip that station.

// In order to make this scheme work for stations that straddle both
// RF and INET, we need to make sure that node_path_ptr doesn't get
// overwritten with an INET path if there's an RF path already in
// there and it has been less than st_direct_timeout seconds since
// the station was last heard on RF.

        if ((port == APRS_PORT_TTY)             // Heard via TNC
                && !third_party                // Not a 3RD-Party packet
                && path != NULL                // Path is not NULL
                && strchr(path,'*') == NULL) { // No asterisk found

            // Look for WIDE or TRACE
            if ((((p = strstr(path,"WIDE")) != NULL) 
                    && (p+=4)) || 
                    (((p = strstr(path,"TRACE")) != NULL) 
                    && (p+=5))) {

                // Look for n=N on WIDEn-N/TRACEn-N digi field
                if ((*p != '\0') && isdigit((int)*p)) {
                    if ((*(p+1) != '\0') && (*(p+1) == '-')) {
                        if ((*(p+2) != '\0') && g_ascii_isdigit(*(p+2))) {
                            if (*(p) == *(p+2)) {
                                direct = 1;
                            }
                            else {
                                direct = 0;
                            }
                        }
                        else {
                            direct = 0;
                        }
                    }
                    else {
                        direct = 0;
                    }
                }
                else {
                    direct = 1;
                }
            }
            else {
                direct = 1;
            }
        }
        else {
            direct = 0;
        }
        
        
        if (direct == 1) {
            // This packet was heard direct.  Set the ST_DIRECT bit
            // and save the timestamp away.
            p_station->direct_heard = curr_sec;
            p_station->flag |= (ST_DIRECT);
        }
        else {
            // This packet was NOT heard direct.  Check whether we
            // need to expire the ST_DIRECT bit.  A lot of fixed
            // stations transmit every 30 minutes.  One hour gives
            // us time to receive a direct packet from them among
            // all the digipeated packets.

            if ((p_station->flag & ST_DIRECT) != 0 &&
                    curr_sec > (p_station->direct_heard + st_direct_timeout)) {
                p_station->flag &= (~ST_DIRECT);
            }
        }

        // If heard on TNC, overwrite node_path_ptr if any of these
        // conditions are met:
        //     *) direct == 1 (packet was heard direct)
        //     *) ST_DIRECT flag == 0 (packet hasn't been heard
        //     direct recently)
        //     *) ST_DIRECT is set, st_direct_timeout has expired
        //     (packet hasn't been heard direct recently)
        //
        // These rules will allow us to keep directly heard paths
        // saved for at least an hour (st_direct_timeout), and not
        // get overwritten with digipeated paths during that time.
        //
        if ((port == APRS_PORT_TTY)  // Heard via TNC
                && !third_party     // Not a 3RD-Party packet
                && path != NULL) {  // Path is not NULL

            // Heard on TNC interface and not third party.  Check
            // the other conditions listed in the comments above to
            // decide whether we should overwrite the node_path_ptr
            // variable.
            //
            if ( direct   // This packet was heard direct
                 || (p_station->flag & ST_DIRECT) == 0  // Not heard direct lately
                 || ( (p_station->flag & ST_DIRECT) != 0 // Not heard direct lately
                      && (curr_sec > (p_station->direct_heard+st_direct_timeout) ) ) ) {

                // Free any old path we might have
                if (p_station->node_path_ptr != NULL)
                    free(p_station->node_path_ptr);
                // Malloc and store the new path
                p_station->node_path_ptr = (char *)malloc(strlen(path) + 1);
                CHECKMALLOC(p_station->node_path_ptr);

                substr(p_station->node_path_ptr,path,strlen(path));
            }
        }
        // If a 3rd-party packet heard on TNC, overwrite
        // node_path_ptr only if heard_via_tnc_last_time is older
        // than one hour (zero counts as well!), plus clear the
        // ST_DIRECT and ST_VIATNC bits in this case.  This makes us
        // keep the RF path around for at least one hour after the
        // station is heard.
        //
        else if ((port == APRS_PORT_TTY)  // Heard via TNC
                && third_party      // It's a 3RD-Party packet
                && path != NULL) {  // Path is not NULL

            // 3rd-party packet heard on TNC interface.  Check if
            // heard_via_tnc_last_time is older than an hour.  If
            // so, overwrite the path and clear a few bits to show
            // that it has timed out on RF and we're now receiving
            // that station from an igate.
            //
            if (curr_sec > (p_station->heard_via_tnc_last_time + 60*60)) {

                // Yep, more than one hour old or is a zero,
                // overwrite the node_path_ptr variable with the new
                // one.  We're only hearing this station on INET
                // now.

                // Free any old path we might have
                if (p_station->node_path_ptr != NULL)
                    free(p_station->node_path_ptr);
                // Malloc and store the new path
                p_station->node_path_ptr = (char *)malloc(strlen(path) + 1);
                CHECKMALLOC(p_station->node_path_ptr);

                substr(p_station->node_path_ptr,path,strlen(path));

                // Clear the ST_VIATNC bit
                p_station->flag &= ~ST_VIATNC;
            }

            // If direct_heard is over an hour old, clear the
            // ST_DIRECT flag.  We're only hearing this station on
            // INET now.
            // 
            if (curr_sec > (p_station->direct_heard + st_direct_timeout)) {

                // Yep, more than one hour old or is a zero, clear
                // the ST_DIRECT flag.
                p_station->flag &= ~ST_DIRECT;
            }
        }
 
        // If heard on INET then overwrite node_path_ptr only if
        // heard_via_tnc_last_time is older than one hour (zero
        // counts as well!), plus clear the ST_DIRECT and ST_VIATNC
        // bits in this case.  This makes us keep the RF path around
        // for at least one hour after the station is heard.
        //
        else if (port != APRS_PORT_TTY  // From an INET interface
                && !third_party        // Not a 3RD-Party packet
                && path != NULL) {     // Path is not NULL
        	
            // Heard on INET interface.  Check if
            // heard_via_tnc_last_time is older than an hour.  If
            // so, overwrite the path and clear a few bits to show
            // that it has timed out on RF and we're now receiving
            // that station from the INET feeds.
            //
            if (curr_sec > (p_station->heard_via_tnc_last_time + 60*60)) {

                // Yep, more than one hour old or is a zero,
                // overwrite the node_path_ptr variable with the new
                // one.  We're only hearing this station on INET
                // now.

                // Free any old path we might have
                if (p_station->node_path_ptr != NULL)
                    free(p_station->node_path_ptr);
                // Malloc and store the new path
                p_station->node_path_ptr = (char *)malloc(strlen(path) + 1);
                CHECKMALLOC(p_station->node_path_ptr);

                substr(p_station->node_path_ptr,path,strlen(path));

                // Clear the ST_VIATNC bit
                p_station->flag &= ~ST_VIATNC;

            }
            
            // If direct_heard is over an hour old, clear the
            // ST_DIRECT flag.  We're only hearing this station on
            // INET now.
            // 
            if (curr_sec > (p_station->direct_heard + st_direct_timeout)) {

                // Yep, more than one hour old or is a zero, clear
                // the ST_DIRECT flag.
                p_station->flag &= ~ST_DIRECT;
            }
        }
 
 
        //---------------------------------------------------------------------

        p_station->num_packets += 1;


        screen_update = 0;
        if (new_station) {
            if (strlen(p_station->speed) > 0 && atof(p_station->speed) > 0) {
                p_station->flag |= (ST_MOVING); // it has a speed, so it's moving
                moving = 1;
            }

            if (p_station->coord_lat != 0 && p_station->coord_lon != 0) {   // discard undef positions from screen
                if (!altnet || is_altnet(p_station) ) {

                    screen_update = 1;
                    // TODO - check needed
                }
            
            }
        }
        else {        // we had seen this station before...
        	
        	if (found_pos && position_defined(p_station->coord_lat,p_station->coord_lon,1)) { // ignore undefined and 0N/0E
                if (p_station->newest_trackpoint != NULL) {
                    moving = 1;                         // it's moving if it has a trail
                }
                else {
                    if (strlen(p_station->speed) > 0 && atof(p_station->speed) > 0) {
                        moving = 1;                     // declare it moving, if it has a speed
                    }
                    else {
                        // Here's where we detect movement
                        if (position_defined(last_lat,last_lon,1)
                                && (p_station->coord_lat != last_lat || p_station->coord_lon != last_lon)) {
                            moving = 1;                 // it's moving if it has changed the position
                        }
                        else {
                            moving = 0;
                        }
                    }
                }
                changed_pos = 0;
                if (moving == 1) {                      
                    p_station->flag |= (ST_MOVING);
                    // we have a moving station, process trails
                    if (atoi(p_station->speed) < TRAIL_MAX_SPEED) {     // reject high speed data (undef gives 0)
                        // we now may already have the 2nd position, so store the old one first
                        if (p_station->newest_trackpoint == NULL) {
                            if (position_defined(last_lat,last_lon,1)) {  // ignore undefined and 0N/0E
                                (void)store_trail_point(p_station,
                                                        last_lon,
                                                        last_lat,
                                                        last_stn_sec,
                                                        last_alt,
                                                        last_speed,
                                                        last_course,
                                                        last_flag);
                            }
                        }
                        //if (   p_station->coord_lon != last_lon
                        //    || p_station->coord_lat != last_lat ) {
                        // we don't store redundant points (may change this
                                                // later ?)
                                                //
                        // There are often echoes delayed 15 minutes
                        // or so it looks ugly on the trail, so I
                        // want to discard them This also discards
                        // immediate echoes. Duplicates back in time
                        // up to TRAIL_ECHO_TIME minutes are
                        // discarded.
                        //
                        if (!is_trailpoint_echo(p_station)) {
                            (void)store_trail_point(p_station,
                                                    p_station->coord_lon,
                                                    p_station->coord_lat,
                                                    p_station->sec_heard,
                                                    p_station->altitude,
                                                    p_station->speed,
                                                    p_station->course,
                                                    p_station->flag);
                            changed_pos = 1;

                            // Check whether it's a locally-owned object/item
                            if (object_is_mine) {

                                // Update time, change position in
                                // time-sorted list to change
                                // expiration time.
                                move_station_time(p_station,p_time);

                                // Give it a new timestamp
                                p_station->sec_heard = curr_sec;
                                //fprintf(stderr,"Updating last heard time\n");
                            }
                        }
                    }

//                    if (track_station_on == 1)          // maybe we are tracking a station
//                        track_station(da,tracking_station_call,p_station);
                } // moving...

                // now do the drawing to the screen
                ok_to_display = !altnet || is_altnet(p_station); // Optimization step, needed twice below.
// TODO
//                screen_update = 0;
//                if (changed_pos == 1 && Display_.trail && ((p_station->flag & ST_INVIEW) != 0)) {
//                    if (ok_to_display) {
//                        draw_trail(da,p_station,1);         // update trail
//                        screen_update = 1;
//                    }

                }

                if (changed_pos == 1 || !position_defined(last_lat,last_lon,0)) {
                    if (ok_to_display) {

                        screen_update = 1; // TODO -check needed
                    }

                }
            } // defined position
        }
    
        if (screen_update) {
            if (p_station->last_port_heard == APRS_PORT_TTY) {   // Data from local TNC

                redraw_on_new_data = 2; // Update all symbols NOW!
            }
          else {
                redraw_on_new_data = 0; // Update each 60 secs
          }
     }


    if(ok)
    {

        plot_aprs_station( p_station, TRUE );

    }

    return(ok);
}   // End of data_add() function


int is_xnum_or_dash(char *data, int max){
    int i;
    int ok;

    ok=1;
    for(i=0; i<max;i++)
        if(!(isxdigit((int)data[i]) || data[i]=='-')) {
            ok=0;
            break;
        }

    return(ok);
}



/*
 *  Decode Mic-E encoded data
 */
int decode_Mic_E(char *call_sign,char *path,char *info,int port,int third_party) {
    int  ii;
    int  offset;
    unsigned char s_b1;
    unsigned char s_b2;
    unsigned char s_b3;
    unsigned char s_b4;
    unsigned char s_b5;
    unsigned char s_b6;
    unsigned char s_b7;
    int  north,west,long_offset;
    int  d,m,h;
    char temp[MAX_LINE_SIZE+1];     // Note: Must be big in case we get long concatenated packets
    char new_info[MAX_LINE_SIZE+1]; // Note: Must be big in case we get long concatenated packets
    int  course;
    int  speed;
    int  msg1,msg2,msg3,msg;
    int  info_size;
    long alt;
    int  msgtyp;
    char rig_type[10];
    int ok;
        
    // MIC-E Data Format   [APRS Reference, chapter 10]

    // todo:  error check
    //        drop wrong positions from receive errors...
    //        drop 0N/0E position (p.25)
    
    /* First 7 bytes of info[] contains the APRS data type ID,    */
    /* longitude, speed, course.                    */
    /* The 6-byte destination field of path[] contains latitude,    */
    /* N/S bit, E/W bit, longitude offset, message code.        */
    /*

    MIC-E Destination Field Format:
    -------------------------------
    Ar1DDDD0 Br1DDDD0 Cr1MMMM0 Nr1MMMM0 Lr1HHHH0 Wr1HHHH0 CrrSSID0
    D = Latitude Degrees.
    M = Latitude Minutes.
    H = Latitude Hundredths of Minutes.
    ABC = Message bits, complemented.
    N = N/S latitude bit (N=1).
    W = E/W longitude bit (W=1).
    L = 100's of longitude degrees (L=1 means add 100 degrees to longitude
    in the Info field).
    C = Command/Response flag (see AX.25 specification).
    r = reserved for future use (currently 0).
    
    */
    /****************************************************************************
    * I still don't handle:                                                     *
    *    Custom message bits                                                    *
    *    SSID special routing                                                   *
    *    Beta versions of the MIC-E (which use a slightly different format).    *
    *                                                                           *
    * DK7IN : lat/long with custom msg works, altitude/course/speed works       *
    *****************************************************************************/


    // Note that the first MIC-E character was not passed to us, so we're
    // starting just past it.
    // Check for valid symbol table character.  Should be '/' or '\'
    // or 0-9, A-Z.
    //
    if (        info[7] == '/'                          // Primary table
            ||  info[7] == '\\'                         // Alternate table
            || (info[7] >= '0' && info[7] <= '9')       // Overlay char
            || (info[7] >= 'A' && info[7] <= 'Z') ) {   // Overlay char

        // We're good, keep going

    }
    else { // Symbol table or overlay char incorrect

        if (info[6] == '/' || info[6] == '\\') {    // Found it back one char in string
            // Don't print out the full info string here because it
            // can contain unprintable characters.  In fact, we
            // should check the chars we do print out to make sure
            // they're printable, else print a space char.
        }

        return(1);  // No good, not MIC-E format or corrupted packet.  Return 1
                    // so that it won't get added to the database at all.
    }

    // Check for valid symbol.  Should be between '!' and '~' only.
    if (info[6] < '!' || info[6] > '~') {

        return(1);  // No good, not MIC-E format or corrupted packet.  Return 1
                    // so that it won't get added to the database at all.
    }

    // Check for minimum MIC-E size.
    if (strlen(info) < 8) {

        return(1);  // No good, not MIC-E format or corrupted packet.  Return 1
                    // so that it won't get added to the database at all.
    }

    // Check for 8-bit characters in the first eight slots.  Not
    // allowed per Mic-E chapter of the spec.
    for (ii = 0; ii < 8; ii++) {
        if ((unsigned char)info[ii] > 0x7f) {
            // 8-bit data was found in the lat/long/course/speed
            // portion.  Bad packet.  Drop it.
//fprintf(stderr, "%s: 8-bits found in Mic-E packet initial portion. Dropping it.\n", call_sign);
            return(1);
        }
    }

    // Check whether we have more data.  If flag character is 0x1d
    // (8-bit telemetry flag) then don't do the 8-bit check below.
    if (strlen(info) > 8) {

        // Check for the 8-bit telemetry flag
        if ((unsigned char)info[8] == 0x1d) {
            // 8-bit telemetry found, skip the check loop below
        }
        else {  // 8-bit telemetry flag was not found.  Check that
                // we only have 7-bit characters through the rest of
                // the packet.

            for (ii = 8; ii < (int)strlen(info); ii++) {

                if ((unsigned char)info[ii] > 0x7f) {
                    // 8-bit data was found.  Bad packet.  Drop it.
//fprintf(stderr, "%s: 8-bits found in Mic-E packet final portion (not 8-bit telemetry). Dropping it.\n", call_sign);
                    return(1);
                }
            }
        }
    }

    //fprintf(stderr,"Path1:%s\n",path);

    msg1 = (int)( ((unsigned char)path[0] & 0x40) >>4 );
    msg2 = (int)( ((unsigned char)path[1] & 0x40) >>5 );
    msg3 = (int)( ((unsigned char)path[2] & 0x40) >>6 );
    msg = msg1 | msg2 | msg3;   // We now have the complemented message number in one variable
    msg = msg ^ 0x07;           // And this is now the normal message number
    msgtyp = 0;                 // DK7IN: Std message, I have to add custom msg decoding

    //fprintf(stderr,"Msg: %d\n",msg);

    /* Snag the latitude from the destination field, Assume TAPR-2 */
    /* DK7IN: latitude now works with custom message */
    s_b1 = (unsigned char)( (path[0] & 0x0f) + (char)0x2f );
    //fprintf(stderr,"path0:%c\ts_b1:%c\n",path[0],s_b1);
    if (path[0] & 0x10)     // A-J
        s_b1 += (unsigned char)1;

    if (s_b1 > (unsigned char)0x39)        // K,L,Z
        s_b1 = (unsigned char)0x20;
    //fprintf(stderr,"s_b1:%c\n",s_b1);
 
    s_b2 = (unsigned char)( (path[1] & 0x0f) + (char)0x2f );
    //fprintf(stderr,"path1:%c\ts_b2:%c\n",path[1],s_b2);
    if (path[1] & 0x10)     // A-J
        s_b2 += (unsigned char)1;

    if (s_b2 > (unsigned char)0x39)        // K,L,Z
        s_b2 = (unsigned char)0x20;
    //fprintf(stderr,"s_b2:%c\n",s_b2);
 
    s_b3 = (unsigned char)( (path[2] & (char)0x0f) + (char)0x2f );
    if (path[2] & 0x10)     // A-J
        s_b3 += (unsigned char)1;

    if (s_b3 > (unsigned char)0x39)        // K,L,Z
        s_b3 = (unsigned char)0x20;
 
    s_b4 = (unsigned char)( (path[3] & 0x0f) + (char)0x30 );
    if (s_b4 > (unsigned char)0x39)        // L,Z
        s_b4 = (unsigned char)0x20;

 
    s_b5 = (unsigned char)( (path[4] & 0x0f) + (char)0x30 );
    if (s_b5 > (unsigned char)0x39)        // L,Z
        s_b5 = (unsigned char)0x20;

 
    s_b6 = (unsigned char)( (path[5] & 0x0f) + (char)0x30 );
    //fprintf(stderr,"path5:%c\ts_b6:%c\n",path[5],s_b6);
    if (s_b6 > (unsigned char)0x39)        // L,Z
        s_b6 = (unsigned char)0x20;
    //fprintf(stderr,"s_b6:%c\n",s_b6);
 
    s_b7 =  (unsigned char)path[6];        // SSID, not used here
    //fprintf(stderr,"path6:%c\ts_b7:%c\n",path[6],s_b7);
 
    //fprintf(stderr,"\n");

    // Special tests for 'L' due to position ambiguity deviances in
    // the APRS spec table.  'L' has the 0x40 bit set, but they
    // chose in the spec to have that represent position ambiguity
    // _without_ the North/West/Long Offset bit being set.  Yuk!
    // Please also note that the tapr.org Mic-E document (not the
    // APRS spec) has the state of the bit wrong in columns 2 and 3
    // of their table.  Reverse them.
    if (path[3] == 'L')
        north = 0;
    else 
        north = (int)((path[3] & 0x40) == (char)0x40);  // N/S Lat Indicator

    if (path[4] == 'L')
        long_offset = 0;
    else
        long_offset = (int)((path[4] & 0x40) == (char)0x40);  // Longitude Offset

    if (path[5] == 'L')
        west = 0;
    else
        west = (int)((path[5] & 0x40) == (char)0x40);  // W/E Long Indicator

    //fprintf(stderr,"north:%c->%d\tlat:%c->%d\twest:%c->%d\n",path[3],north,path[4],long_offset,path[5],west);

    /* Put the latitude string into the temp variable */
    snprintf(temp, sizeof(temp), "%c%c%c%c.%c%c%c%c",s_b1,s_b2,s_b3,s_b4,s_b5,s_b6,
            (north ? 'N': 'S'), info[7]);   // info[7] = symbol table

    /* Compute degrees longitude */
    snprintf(new_info, sizeof(new_info), "%s", temp);
    d = (int) info[0]-28;

    if (long_offset)
        d += 100;

    if ((180<=d)&&(d<=189))  // ??
        d -= 80;

    if ((190<=d)&&(d<=199))  // ??
        d -= 190;

    /* Compute minutes longitude */
    m = (int) info[1]-28;
    if (m>=60)
        m -= 60;

    /* Compute hundredths of minutes longitude */
    h = (int) info[2]-28;
    /* Add the longitude string into the temp variable */
    xastir_snprintf(temp, sizeof(temp), "%03d%02d.%02d%c%c",d,m,h,(west ? 'W': 'E'), info[6]);
    strncat(new_info,
        temp,
        sizeof(new_info) - strlen(new_info));

    /* Compute speed in knots */
    speed = (int)( ( info[3] - (char)28 ) * (char)10 );
    speed += ( (int)( (info[4] - (char)28) / (char)10) );
    if (speed >= 800)
        speed -= 800;       // in knots

    /* Compute course */
    course = (int)( ( ( (info[4] - (char)28) % 10) * (char)100) + (info[5] - (char)28) );
    if (course >= 400)
        course -= 400;

    /*  ???
        fprintf(stderr,"info[4]-28 mod 10 - 4 = %d\n",( ( (int)info[4]) - 28) % 10 - 4);
        fprintf(stderr,"info[5]-28 = %d\n", ( (int)info[5]) - 28 );
    */
    xastir_snprintf(temp, sizeof(temp), "%03d/%03d",course,speed);
    strncat(new_info,
        temp,
        sizeof(new_info) - strlen(new_info));
    offset = 8;   // start of rest of info

    /* search for rig type in Mic-E data */
    rig_type[0] = '\0';
    if (info[offset] != '\0' && (info[offset] == '>' || info[offset] == ']')) {
        /* detected type code:     > TH-D7    ] TM-D700 */
        if (info[offset] == '>')
            xastir_snprintf(rig_type,
                sizeof(rig_type),
                " TH-D7");
        else
            xastir_snprintf(rig_type,
                sizeof(rig_type),
                " TM-D700");

        offset++;
    }

    info_size = (int)strlen(info);
    /* search for compressed altitude in Mic-E data */  // {
    if (info_size >= offset+4 && info[offset+3] == '}') {  // {
        /* detected altitude  ___} */
        alt = ((((long)info[offset] - (long)33) * (long)91 +(long)info[offset+1] - (long)33) * (long)91
                    + (long)info[offset+2] - (long)33) - 10000;  // altitude in meters
        alt /= 0.3048;                                // altitude in feet, as in normal APRS

        //32808 is -10000 meters, or 10 km (deepest ocean), which is as low as a MIC-E
        //packet may go.  Upper limit is mostly a guess.
        if ( (alt > 500000) || (alt < -32809) ) {  // Altitude is whacko.  Skip it.
            offset += 4;
        }
        else {  // Altitude is ok
            xastir_snprintf(temp, sizeof(temp), " /A=%06ld",alt);
            offset += 4;
            strncat(new_info,
                temp,
                sizeof(new_info) - strlen(new_info));
        }
    }

    /* start of comment */
    if (strlen(rig_type) > 0) {
        xastir_snprintf(temp, sizeof(temp), "%s",rig_type);
        strncat(new_info,
            temp,
            sizeof(new_info) - strlen(new_info));
    }

    strncat(new_info,
        " Mic-E ",
        sizeof(new_info) - strlen(new_info));
    if (msgtyp == 0) {
        switch (msg) {
            case 1:
                strncat(new_info,
                    "Enroute",
                    sizeof(new_info) - strlen(new_info));
                break;

            case 2:
                strncat(new_info,
                    "In Service",
                    sizeof(new_info) - strlen(new_info));
                break;

            case 3:
                strncat(new_info,
                    "Returning",
                    sizeof(new_info) - strlen(new_info));
                break;

            case 4:
                strncat(new_info,
                    "Committed",
                    sizeof(new_info) - strlen(new_info));
                break;

            case 5:
                strncat(new_info,
                    "Special",
                    sizeof(new_info) - strlen(new_info));
                break;

            case 6:
                strncat(new_info,
                    "Priority",
                    sizeof(new_info) - strlen(new_info));
                break;

            case 7:
                strncat(new_info,
                    "Emergency",
                    sizeof(new_info) - strlen(new_info));

                // Functionality removed

                break;

            default:
                strncat(new_info,
                    "Off Duty",
                    sizeof(new_info) - strlen(new_info));
        }
    }
    else {
        xastir_snprintf(temp, sizeof(temp), "Custom%d",msg);
        strncat(new_info,
            temp,
            sizeof(new_info) - strlen(new_info));
    }

    if (info[offset] != '\0') {
        /* Append the rest of the message to the expanded MIC-E message */
        for (ii=offset; ii<info_size; ii++)
            temp[ii-offset] = info[ii];

        temp[info_size-offset] = '\0';
        strncat(new_info,
            " ",
            sizeof(new_info) - strlen(new_info));
        strncat(new_info,
            temp,
            sizeof(new_info) - strlen(new_info));
    }


    // We don't transmit Mic-E protocol from Xastir, so we know it's
    // not our station's packets or our object/item packets,
    // therefore the last two parameters here are both zero.
    //
    ok = data_add(APRS_MICE,call_sign,path,new_info,port,NULL,third_party, 0, 0);


    return(ok);
}   // End of decode_Mic_E()




void delete_object(char *name) {
	
	// TODO 
/*
	
	DataRow *p_station;

//fprintf(stderr,"delete_object\n");

    p_station = NULL;
    if (search_station_name(&p_station,name,1)) {       // find object name
        p_station->flag &= (~ST_ACTIVE);                // clear flag
        p_station->flag &= (~ST_INVIEW);                // clear "In View" flag
        if (position_on_screen(p_station->coord_lat,p_station->coord_lon))
            redraw_on_new_data = 2;                     // redraw now
            // there is some problem...  it is not redrawn immediately! ????
            // but deleted from list immediatetly
        redo_list = (int)TRUE;                          // and update lists
    }
    */
}


/*
 *  Decode APRS Information Field and dispatch it depending on the Data Type ID
 *
 *         call = Callsign or object/item name string
 *         path = Path string
 *      message = Info field (corrupted already if object/item packet)
 *       origin = Originating callsign if object/item, otherwise NULL
 *         from = DATA_VIA_LOCAL/DATA_VIA_TNC/DATA_VIA_NET/DATA_VIA_FILE
 *         port = Port number
 *  third_party = Set to one if third-party packet
 * orig_message = Unmodified info field
 *
 */
void decode_info_field(gchar *call,
                       gchar *path,
                       gchar *message,
                       gchar *origin, 
                       TAprsPort port,
                       gint third_party,
                       gchar *orig_message) 
{
//    char line[MAX_LINE_SIZE+1];
    int  ok_igate_net;
    int  ok_igate_rf;
    int  done, ignore;
    char data_id;
    int station_is_mine = 0;
    int object_is_mine = 0;

    /* remember fixed format starts with ! and can be up to 24 chars in the message */ // ???

    done         = 0;       // if 1, packet was decoded
    ignore       = 0;       // if 1, don't treat undecoded packets as status text
    ok_igate_net = 0;       // if 1, send packet to internet
    ok_igate_rf  = 0;       // if 1, igate packet to RF if "from" is in nws-stations.txt

    if ( is_my_call(call, 1) ) {
        station_is_mine++; // Station is controlled by me
    }
 
    if ( (message != NULL) && (strlen(message) > MAX_LINE_SIZE) ) { 
    	// Overly long message, throw it away.
        done = 1;
    }
    else if (message == NULL || strlen(message) == 0) {      
    	// we could have an empty message
        (void)data_add(STATION_CALL_DATA,call,path,NULL,port,origin,third_party, station_is_mine, 0);
        done = 1;                                       
        // don't report it to internet
    }

    // special treatment for objects/items.
    if (!done && origin[0] != '\0') {

        // If station/object/item is owned by me (including SSID)
        if ( is_my_call(origin, 1) ) {
            object_is_mine++;
        }
         
        if (message[0] == '*') {    // set object
            (void)data_add(APRS_OBJECT,call,path,message+1,port,origin,third_party, station_is_mine, object_is_mine);
            if (strlen(origin) > 0 && strncmp(origin,"INET",4)!=0) {
                ok_igate_net = 1;   // report it to internet
            }
            ok_igate_rf = 1;
            done = 1;
        }

        else if (message[0] == '!') {   
        	// set item
            (void)data_add(APRS_ITEM,call,path,message+1,port,origin,third_party, station_is_mine, object_is_mine);
            if (strlen(origin) > 0 && strncmp(origin,"INET",4)!=0) {
                ok_igate_net = 1;   // report it to internet
            }
            ok_igate_rf = 1;
            done = 1;
        }

        else if (message[0] == '_') {   // delete object/item
// TODO
/*
            AprsDataRow *p_station;

            delete_object(call);    // ?? does not vanish from map immediately !!???

            // If object was owned by me but another station is
            // transmitting it now, write entries into the
            // object.log file showing that we don't own this object
            // anymore.
            p_station = NULL;

            if (search_station_name(&p_station,call,1)) {
                if (is_my_object_item(p_station)    // If station was owned by me (including SSID)
                        && (!object_is_mine) ) {  // But isn't now
                    disown_object_item(call,origin);
                }
            }

            if (strlen(origin) > 0 && strncmp(origin,"INET",4)!=0) {
                ok_igate_net = 1;   // report it to internet
            }

            ok_igate_rf = 1;
*/
            done = 1;
            
        }
    }
    
    if (!done) {
        int rdf_type;

        data_id = message[0];           // look at the APRS Data Type ID (first char in information field)
        message += 1;                   // extract data ID from information field
        ok_igate_net = 1;               // as default report packet to internet


        switch (data_id) {
            case '=':   // Position without timestamp (with APRS messaging)

                //WE7U
                // Need to check for weather info in this packet type as well?

                done = data_add(APRS_MSGCAP,call,path,message,port,origin,third_party, station_is_mine, 0);
                ok_igate_rf = done;
                break;

            case '!':   // Position without timestamp (no APRS messaging) or Ultimeter 2000 WX
                if (message[0] == '!' && is_xnum_or_dash(message+1,40))   // Ultimeter 2000 WX
                    done = data_add(APRS_WX3,call,path,message+1,port,origin,third_party, station_is_mine, 0);
                else
                    done = data_add(APRS_FIXED,call,path,message,port,origin,third_party, station_is_mine, 0);
                ok_igate_rf = done;
                break;

            case '/':   // Position with timestamp (no APRS messaging)

//WE7U
// Need weather decode in this section similar to the '@' section
// below.

                if ((toupper(message[14]) == 'N' || toupper(message[14]) == 'S') &&
                    (toupper(message[24]) == 'W' || toupper(message[24]) == 'E')) { // uncompressed format
                    if (message[29] == '/') {
                        if (message[33] == 'g' && message[37] == 't')
                            done = data_add(APRS_WX1,call,path,message,port,origin,third_party, station_is_mine, 0);
                        else
                            done = data_add(APRS_MOBILE,call,path,message,port,origin,third_party, station_is_mine, 0);
                    }
                    else
                        done = data_add(APRS_DF,call,path,message,port,origin,third_party, station_is_mine, 0);
                }
                else {                                                // compressed format
                    if (message[16] >= '!' && message[16] <= 'z') {     // csT is speed/course
                        if (message[20] == 'g' && message[24] == 't')   // Wx data
                            done = data_add(APRS_WX1,call,path,message,port,origin,third_party, station_is_mine, 0);
                        else
                            done = data_add(APRS_MOBILE,call,path,message,port,origin,third_party, station_is_mine, 0);
                    }
                    else
                        done = data_add(APRS_DF,call,path,message,port,origin,third_party, station_is_mine, 0);
                }
//                done = data_add(APRS_DOWN,call,path,message,from,port,origin,third_party, station_is_mine, 0);
                ok_igate_rf = done;
                break;

            case '@':   // Position with timestamp (with APRS messaging)
                // DK7IN: could we need to test the message length first?
                if ((toupper(message[14]) == 'N' || toupper(message[14]) == 'S') &&
                    (toupper(message[24]) == 'W' || toupper(message[24]) == 'E')) {       // uncompressed format
                    if (message[29] == '/') {
                        if (message[33] == 'g' && message[37] == 't')
                            done = data_add(APRS_WX1,call,path,message,port,origin,third_party, station_is_mine, 0);
                        else
                            done = data_add(APRS_MOBILE,call,path,message,port,origin,third_party, station_is_mine, 0);
                    }
                    else
                        done = data_add(APRS_DF,call,path,message,port,origin,third_party, station_is_mine, 0);
                }
                else {                                                // compressed format
                    if (message[16] >= '!' && message[16] <= 'z') {     // csT is speed/course
                        if (message[20] == 'g' && message[24] == 't')   // Wx data
                            done = data_add(APRS_WX1,call,path,message,port,origin,third_party, station_is_mine, 0);
                        else
                            done = data_add(APRS_MOBILE,call,path,message,port,origin,third_party, station_is_mine, 0);
                    }
                    else
                        done = data_add(APRS_DF,call,path,message,port,origin,third_party, station_is_mine, 0);
                }
                ok_igate_rf = done;
                break;

            case '[':   // Maidenhead grid locator beacon (obsolete- but used for meteor scatter)
                done = data_add(APRS_GRID,call,path,message,port,origin,third_party, station_is_mine, 0);
                ok_igate_rf = done;
                break;
            case 0x27:  // Mic-E  Old GPS data (or current GPS data in Kenwood TM-D700)
            case 0x60:  // Mic-E  Current GPS data (but not used in Kennwood TM-D700)
            //case 0x1c:// Mic-E  Current GPS data (Rev. 0 beta units only)
            //case 0x1d:// Mic-E  Old GPS data (Rev. 0 beta units only)
                done = decode_Mic_E(call,path,message,port,third_party);
                ok_igate_rf = done;
                break;

            case '_':   // Positionless weather data                [APRS Reference, chapter 12]
                done = data_add(APRS_WX2,call,path,message,port,origin,third_party, station_is_mine, 0);
                ok_igate_rf = done;
                break;

            case '#':   // Peet Bros U-II Weather Station (km/h)    [APRS Reference, chapter 12]
                if (is_xnum_or_dash(message,13))
                    done = data_add(APRS_WX4,call,path,message,port,origin,third_party, station_is_mine, 0);
                ok_igate_rf = done;
                break;

            case '*':   // Peet Bros U-II Weather Station (mph)
                if (is_xnum_or_dash(message,13))
                    done = data_add(APRS_WX6,call,path,message,port,origin,third_party, station_is_mine, 0);
                ok_igate_rf = done;
                break;

            case '$':   // Raw GPS data or Ultimeter 2000
                if (strncmp("ULTW",message,4) == 0 && is_xnum_or_dash(message+4,44))
                    done = data_add(APRS_WX5,call,path,message+4,port,origin,third_party, station_is_mine, 0);
                else if (strncmp("GPGGA",message,5) == 0)
                    done = data_add(GPS_GGA,call,path,message,port,origin,third_party, station_is_mine, 0);
                else if (strncmp("GPRMC",message,5) == 0)
                    done = data_add(GPS_RMC,call,path,message,port,origin,third_party, station_is_mine, 0);
                else if (strncmp("GPGLL",message,5) == 0)
                    done = data_add(GPS_GLL,call,path,message,port,origin,third_party, station_is_mine, 0);
                else {
                        // handle VTG and WPT too  (APRS Ref p.25)
                }
                ok_igate_rf = done;
                break;

            case ':':   // Message

                // Do message logging if that feature is enabled.
                done = decode_message(call,path,message,port,third_party);

                // there could be messages I should not retransmit to internet... ??? Queries to me...
                break;

            case '>':   // Status                                   [APRS Reference, chapter 16]
                done = data_add(APRS_STATUS,call,path,message,port,origin,third_party, station_is_mine, 0);
                ok_igate_rf = done;
                break;

            case '?':   // Query
                done = process_query(call,path,message,port,third_party);
                ignore = 1;     // don't treat undecoded packets as status text
                break;

            case 'T':   // Telemetry data                           [APRS Reference, chapter 13]
                // We treat these as status packets currently.
                ok_igate_rf = 1;
                done = data_add(APRS_STATUS,call,path,message,port,origin,third_party, station_is_mine, 0);
                break;
 
            case '{':   // User-defined APRS packet format     //}
                // We treat these as status packets currently.
                ok_igate_rf = 1;
                break;
 
            case '<':   // Station capabilities                     [APRS Reference, chapter 15]
                //
                // We could tweak the Incoming Data dialog to add
                // filter togglebuttons.  One such toggle could be
                // "Station Capabilities".  We'd then have a usable
                // dialog for displaying things like ?IGATE?
                // responses.  In this case we wouldn't have to do
                // anything special with the packet for decoding,
                // just let it hit the default block below for
                // putting them into the status field of the record.
                // One downside is that we'd only be able to catch
                // new station capability records in that dialog.
                // The only way to look at past capability records
                // would be the Station Info dialog for each
                // station.
                //
                //fprintf(stderr,"%10s:  %s\n", call, message);

                // Don't set "done" as we want these to appear in
                // the status text for the record.
                break;

            case '%':   // Agrelo DFJr / MicroFinder Radio Direction Finding

                // Here is where we'd add a call to an RDF decode
                // function so that we could display vectors on the
                // map for each RDF position.

//
// Agrelo format:  "%XXX/Q<cr>"
//
// "XXX" is relative bearing to the signal (000-359).  Careful here:
// At least one unit reports in magnetic instead of relative
// degrees.  "000" means no direction info available, 360 means true
// north.
//
// "Q" is bearing quality (0-9).  0 = unsuitable.  9 = manually
// entered.  1-8 = varying quality with 8 being the best.
//
// I've also seen these formats, which may not be Agrelo compatible:
//
//      "%136.0/9"
//      "%136.0/8/158.0" (That last number is magnetic bearing)
//
// These sentences may be sent MULTIPLE times per second, like 20 or
// more!  If we decide to average readings, we'll need to dump our
// averages and start over if our course changes.
//

                // Check for Agrelo format:
                if (    strlen(message) >= 5
                        && is_num_chr(message[0])   // "%136/9"
                        && is_num_chr(message[1])
                        && is_num_chr(message[2])
                        && message[3] == '/'
                        && is_num_chr(message[4]) ) {

                    rdf_type = 1;

                    fprintf(stderr,
                        "Type 1 RDF packet from call: %s\tBearing: %c%c%c\tQuality: %c\n",
                        call,
                        message[0],
                        message[1],
                        message[2],
                        message[4]);
 
                }

                // Check for extended formats (not
                // Agrelo-compatible):
                else if (strlen(message) >= 13
                        && is_num_chr(message[0])   // "%136.0/8/158.0"
                        && is_num_chr(message[1])
                        && is_num_chr(message[2])
                        && message[3] == '.'
                        && is_num_chr(message[4])
                        && message[5] == '/'
                        && is_num_chr(message[6])
                        && message[7] == '/'
                        && is_num_chr(message[8])
                        && is_num_chr(message[9])
                        && is_num_chr(message[10])
                        && message[11] == '.'
                        && is_num_chr(message[12]) ) {

                    rdf_type = 3;

                    fprintf(stderr,
                        "Type 3 RDF packet from call: %s\tBearing: %c%c%c%c%c\tQuality: %c\tMag Bearing: %c%c%c%c%c\n",
                        call,
                        message[0],
                        message[1],
                        message[2],
                        message[3],
                        message[4],
                        message[6],
                        message[8],
                        message[9],
                        message[10],
                        message[11],
                        message[12]);
                }

                // Check for extended formats (not
                // Agrelo-compatible):
                else if (strlen(message) >= 7
                        && is_num_chr(message[0])   // "%136.0/9"
                        && is_num_chr(message[1])
                        && is_num_chr(message[2])
                        && message[3] == '.'
                        && is_num_chr(message[4])
                        && message[5] == '/'
                        && is_num_chr(message[6]) ) {

                    rdf_type = 2;

                    fprintf(stderr,
                        "Type 2 RDF packet from call: %s\tBearing: %c%c%c%c%c\tQuality: %c\n",
                        call,
                        message[0],
                        message[1],
                        message[2],
                        message[3],
                        message[4],
                        message[6]);
                }

                // Don't set "done" as we want these to appear in
                // the status text for the record until we get the
                // full decoding for this type of packet coded up.
                break;
 
            case '~':   // UI-format messages, not relevant for APRS ("Do not use" in Reference)
            case ',':   // Invalid data or test packets             [APRS Reference, chapter 19]
            case '&':   // Reserved -- Map Feature
                ignore = 1;     // Don't treat undecoded packets as status text
                break;
        }

        
        // Add most remaining data to the station record as status
        // info
        //
        if (!done && !ignore) {         // Other Packets        [APRS Reference, chapter 19]
            done = data_add(OTHER_DATA,call,path,message-1,port,origin,third_party, station_is_mine, 0);
            ok_igate_net = 0;           // don't put data on internet       ????
        }

        if (!done) {                    // data that we do ignore...
            //fprintf(stderr,"decode_info_field: not decoding info: Call:%s ID:%c Msg:|%s|\n",call,data_id,message);
            ok_igate_net = 0;           // don't put data on internet
        }
    }
    
    if (third_party)
        ok_igate_net = 0;   // don't put third party traffic on internet


    if (station_is_mine)
        ok_igate_net = 0;   // don't put my data on internet     ???
    
    
    // TODO - Add TX to IGate support
/*
    if (ok_igate_net) {

        if ( (from == DATA_VIA_TNC) // Came in via a TNC
                && (strlen(orig_message) > 0) ) { // Not empty

            // Here's where we inject our own callsign like this:
            // "WE7U-15,I" in order to provide injection ID for our
            // igate.
            snprintf(line,
                sizeof(line),
                "%s>%s,%s,I:%s",
                (strlen(origin)) ? origin : call,
                path,
                _aprs_mycall,
                orig_message);

            output_igate_net(line, port, third_party);
        }
    }
*/
}



/*
 *  Check for a valid object name
 */
int valid_object(char *name) {
    int len, i;

    // max 9 printable ASCII characters, case sensitive   [APRS
    // Reference]
    len = (int)strlen(name);
    if (len > 9 || len == 0)
        return(0);                      // wrong size

    for (i=0;i<len;i++)
        if (!isprint((int)name[i]))
            return(0);                  // not printable

    return(1);
}






/*
 *  Extract object or item data from information field before processing
 *
 *  Returns 1 if valid object found, else returns 0.
 *
 */
int extract_object(char *call, char **info, char *origin) 
{
    int ok, i;

    // Object and Item Reports     [APRS Reference, chapter 11]
    ok = 0;
    // todo: add station originator to database
    if ((*info)[0] == ';') {                    // object
        // fixed 9 character object name with any printable ASCII character
        if (strlen((*info)) > 1+9) {
            substr(call,(*info)+1,9);           // extract object name
            (*info) = (*info) + 10;
            // Remove leading spaces ? They look bad, but are allowed by the APRS Reference ???
            (void)remove_trailing_spaces(call);
            if (valid_object(call)) {
                // info length is at least 1
                ok = 1;
            }
        }
    }
    else if ((*info)[0] == ')') {             // item
        // 3 - 9 character item name with any printable ASCII character
        if (strlen((*info)) > 1+3) {
            for (i = 1; i <= 9; i++) {
                if ((*info)[i] == '!' || (*info)[i] == '_') {
                    call[i-1] = '\0';
                    break;
                }
                call[i-1] = (*info)[i];
            }
            call[9] = '\0';  // In case we never saw '!' || '_'
            (*info) = &(*info)[i];
            // Remove leading spaces ? They look bad, but are allowed by the APRS Reference ???
            //(void)remove_trailing_spaces(call);   // This statement messed up our searching!!! Don't use it!
            if (valid_object(call)) {
                // info length is at least 1
                ok = 1;
            }
        }
    }
    else 
    {
        fprintf(stderr,"Not an object, nor an item!!! call=%s, info=%s, origin=%s.\n",
               call, *info, origin);
    }
    return(ok);
}


/*
 *  Decode AX.25 line
 *  \r and \n should already be stripped from end of line
 *  line should not be NULL
 *
 * If dbadd is set, add to database.  Otherwise, just return true/false
 * to indicate whether input is valid AX25 line.
 */
//
// Note that the length of "line" can be up to MAX_DEVICE_BUFFER,
// which is currently set to 4096.
//
gint decode_ax25_line(gchar *line, TAprsPort port) {
    gchar *call_sign;
    gchar *path0;
    gchar path[100+1];           // new one, we may add an '*'
    gchar *info;
    gchar info_copy[MAX_LINE_SIZE+1];
    gchar call[MAX_CALLSIGN+1];
    gchar origin[MAX_CALLSIGN+1];
    gint ok;
    gint third_party;
//    gchar backup[MAX_LINE_SIZE+1];
//    gchar tmp_line[MAX_LINE_SIZE+1];
//    gchar tmp_path[100+1];
//    gchar *ViaCalls[10];

    if (line == NULL) 
    {
        return(FALSE);
    }

    if ( strlen(line) > MAX_LINE_SIZE ) 
    { 
        // Overly long message, throw it away.  We're done.
        return(FALSE);
    }

    if (line[strlen(line)-1] == '\n')           // better: look at other places,
                                                // so that we don't get it here...
        line[strlen(line)-1] = '\0';            // Wipe out '\n', to be sure
    if (line[strlen(line)-1] == '\r')
        line[strlen(line)-1] = '\0';            // Wipe out '\r'

    call_sign   = NULL;
    path0       = NULL;
    info        = NULL;
    origin[0]   = '\0';
    call[0]     = '\0';
    path[0]     = '\0';
    third_party = 0;

    // CALL>PATH:APRS-INFO-FIELD                // split line into components
    //     ^    ^
    ok = 0;
    call_sign = (gchar*)strtok(line,">");               // extract call from AX.25 line
    if (call_sign != NULL) {
        path0 = (gchar*)strtok(NULL,":");               // extract path from AX.25 line
        if (path0 != NULL) {
            info = (gchar*)strtok(NULL,"");             // rest is info_field
            if (info != NULL) {
                if ((info - path0) < 100) {     // check if path could be copied
                    ok = 1;                     // we have found all three components
                }
            }
        }
    }

    if (ok) 
    {
        snprintf(path, sizeof(path), "%s", path0);

        snprintf(info_copy, sizeof(info_copy), "%s", info);

        ok = valid_path(path);                  // check the path and convert it to TAPR format
        // Note that valid_path() also removes igate injection identifiers
    }

    if (ok) 
    {
        extract_TNC_text(info);                 // extract leading text from TNC X-1J4
        if (strlen(info) > 256)                 // first check if information field conforms to APRS specs
            ok = 0;                             // drop packets too long
    }

    if (ok) 
    {                                                   // check callsign
        (void)remove_trailing_asterisk(call_sign);              // is an asterisk valid here ???

        if (valid_inet_name(call_sign,info,origin,sizeof(origin))) 
        { // accept some of the names used in internet
            snprintf(call, sizeof(call), "%s", call_sign);
        }
        else if (valid_call(call_sign)) 
        {                     // accept real AX.25 calls
            snprintf(call, sizeof(call), "%s", call_sign);
        }
        else {
            ok = 0;
        }
    }

    if (ok && info[0] == '}') 
    {
    	// look for third-party traffic
        ok = extract_third_party(call,path,sizeof(path),&info,origin,sizeof(origin)); // extract third-party data
        third_party = 1;
    }

    if (ok && (info[0] == ';' || info[0] == ')')) 
    {             
    	// look for objects or items
        snprintf(origin, sizeof(origin), "%s", call);

        ok = extract_object(call,&info,origin);                 // extract object data
    }

    if (ok) 
    {
        // decode APRS information field, always called with valid call and path
        // info is a string with 0 - 256 bytes
        // fprintf(stderr,"dec: %s (%s) %s\n",call,origin,info);
  
       decode_info_field(call,
            path,
            info,
            origin,
            port,
            third_party,
            info_copy);
    }


    return(ok);
}





///////////////////
double convert_lon_l2d(long lon) 
{
    double dResult = 0.00;
    
    int ewpn;
    float deg, min; //, sec;
    int ideg; //, imin;
    long temp;

    deg = (float)(lon - 64800000l) / 360000.0;

    // Switch to integer arithmetic to avoid floating-point rounding
    // errors.
    temp = (long)(deg * 100000);

    
    ewpn = 1;
    if (temp <= 0) 
    {
        ewpn = -1;
        temp = labs(temp);
    }

    ideg = (int)temp / 100000;
    min = (temp % 100000) * 60.0 / 100000.0;


    dResult = ewpn*(ideg+min/60.0);

    return dResult;
}




// convert latitude from long to double 
// Input is in Xastir coordinate system

double convert_lat_l2d(long lat) 
{
    double dResult = 0.00;

    int nspn;
    float deg, min;
    int ideg;
    long temp;

    deg = (float)(lat - 32400000l) / 360000.0;
 
    // Switch to integer arithmetic to avoid floating-point
    // rounding errors.
    temp = (long)(deg * 100000);

    nspn = -1;
    if (temp <= 0) {
        nspn = 1;
        temp = labs(temp);
    }   

    ideg = (int)temp / 100000;
    min = (temp % 100000) * 60.0 / 100000.0;

    dResult = nspn*(ideg+min/60.0);

    return dResult;
}



/***********************************************************/
/* returns the hour (00..23), localtime                    */
/***********************************************************/
int get_hours(void) {
    struct tm *time_now;
    time_t secs_now;
    char shour[5];

    secs_now=sec_now();
    time_now = localtime(&secs_now);
    (void)strftime(shour,4,"%H",time_now);
    return(atoi(shour));
}




/***********************************************************/
/* returns the minute (00..59), localtime                  */
/***********************************************************/
int get_minutes(void) {
    struct tm *time_now;
    time_t secs_now;
    char sminute[5];

    secs_now=sec_now();
    time_now = localtime(&secs_now);
    (void)strftime(sminute,4,"%M",time_now);
    return(atoi(sminute));
}




/**************************************************************/
/* compute_rain_hour - rolling average for the last 59.x      */
/* minutes of rain.  I/O numbers are in hundredths of an inch.*/
/* Output is in "rain_minute_total", a global variable.       */
/**************************************************************/
void compute_rain_hour(float rain_total) {
    int current, j;
    float lowest;


    // Deposit the _starting_ rain_total for each minute into a separate bucket.
    // Subtract lowest rain_total in queue from current rain_total to get total
    // for the last hour.


    current = get_minutes(); // Fetch the current minute value.  Use this as an index
                        // into our minute "buckets" where we store the data.


    rain_minute[ (current + 1) % 60 ] = 0.0;   // Zero out the next bucket (probably have data in
                                                // there from the previous hour).


    if (rain_minute[current] == 0.0)           // If no rain_total stored yet in this minute's bucket
        rain_minute[current] = rain_total;      // Write into current bucket


    // Find the lowest non-zero value for rain_total.  The max value is "rain_total".
    lowest = rain_total;                    // Set to maximum to get things going
    for (j = 0; j < 60; j++) {
        if ( (rain_minute[j] > 0.0) && (rain_minute[j] < lowest) ) { // Found a lower non-zero value?
            lowest = rain_minute[j];        // Snag it
        }
    }
    // Found it, subtract the two to get total for the last hour
    rain_minute_total = rain_total - lowest;
}




/***********************************************************/
/* compute_rain - compute rain totals from the total rain  */
/* so far.  rain_total (in hundredths of an inch) keeps on */
/* incrementing.                                           */
/***********************************************************/
void compute_rain(float rain_total) {
    int current, i;
    float lower;


    // Skip the routine if input is outlandish (Negative value, zero, or 512 inches!).
    // We seem to get occasional 0.0 packets from wx200d.  This makes them go away.
    if ( (rain_total <= 0.0) || (rain_total > 51200.0) )
        return;

    compute_rain_hour(rain_total);

    current = get_hours();

    // Set rain_base:  The first rain_total for each hour.
    if (rain_base[current] == 0.0) {       // If we don't have a start value yet for this hour,
        rain_base[current] = rain_total;    // save it away.
        rain_check = 0;                     // Set up rain_check so we'll do the following
                                            // "else" clause a few times at start of each hour.
    }
    else {  // rain_base has been set, is it wrong?  We recheck three times at start of hour.
        if (rain_check < 3) {
            rain_check++;
            // Is rain less than base?  It shouldn't be.
            if (rain_total < rain_base[current])
                rain_base[current] = rain_total;

            // Difference greater than 10 inches since last reading?  It shouldn't be.
            if (fabs(rain_total - rain_base[current]) > 1000.0) // Remember:  Hundredths of an inch
                rain_base[current] = rain_total;
        }
    }
    rain_base[ (current + 1) % 24 ] = 0.0;    // Clear next hour's index.


    // Compute total rain in last 24 hours:  Really we'll compute the total rain
    // in the last 23 hours plus the portion of an hour we've completed (Sum up
    // all 24 of the hour totals).  This isn't the perfect way to do it, but to
    // really do it right we'd need finer increments of time (to get closer to
    // the goal of 24 hours of rain).
    lower = rain_total;
    for ( i = 0; i < 24; i++ ) {    // Find the lowest non-zero rain_base value in last 24 hours
        if ( (rain_base[i] > 0.0) && (rain_base[i] < lower) ) {
            lower = rain_base[i];
        }
    }
    rain_24 = rain_total - lower;    // Hundredths of an inch


    // Compute rain since midnight.  Note that this uses whatever local time was set
    // on the machine.  It might not be local midnight if your box is set to GMT.
    lower = rain_total;
    for ( i = 0; i <= current; i++ ) {  // Find the lowest non-zero rain_base value since midnight
        if ( (rain_base[i] > 0.0) && (rain_base[i] < lower) ) {
            lower = rain_base[i];
        }
    }
    rain_00 = rain_total - lower;    // Hundredths of an inch

    // It is the responsibility of the calling program to save
    // the new totals in the data structure for our station.
    // We don't return anything except in global variables.

}




/**************************************************************/
/* compute_gust - compute max wind gust during last 5 minutes */
/*                                                            */
/**************************************************************/
float compute_gust(float wx_speed, float last_speed, time_t *last_speed_time) {
    float computed_gust;
    int current, j;


    // Deposit max gust for each minute into a different bucket.
    // Check all buckets for max gust within the last five minutes
    // (Really 4 minutes plus whatever portion of a minute we've completed).

    current = get_minutes(); // Fetch the current minute value.  We use this as an index
                        // into our minute "buckets" where we store the data.

    // If we haven't started collecting yet, set up to do so
    if (gust_read_ptr == gust_write_ptr) {  // We haven't started yet
        gust_write_ptr = current;           // Set to write into current bucket
        gust_last_write = current;

        gust_read_ptr = current - 1;        // Set read pointer back one, modulus 60
        if (gust_read_ptr < 0)
            gust_read_ptr = 59;

        gust[gust_write_ptr] = 0.0;         // Zero the max gust
        gust[gust_read_ptr] = 0.0;          // for both buckets.

//WE7U: Debug
//gust[gust_write_ptr] = 45.9;
    }

    // Check whether we've advanced at least one minute yet
    if (current != gust_write_ptr) {        // We've advanced to a different minute
        gust_write_ptr = current;           // Start writing into a new bucket.
        gust[gust_write_ptr] = 0.0;         // Zero the new bucket

        // Check how many bins of real data we have currently.  Note that this '5' is
        // correct, as we just advanced "current" to the next minute.  We're just pulling
        // along the read_ptr behind us if we have 5 bins worth of data by now.
        if ( ((gust_read_ptr + 5) % 60) == gust_write_ptr)  // We have 5 bins of real data
            gust_read_ptr = (gust_read_ptr + 1) % 60;       // So advance the read pointer,

        // Check for really bad pointers, perhaps the weather station got
        // unplugged for a while or it's just REALLY slow at sending us data?
        // We're checking to see if gust_last_write happened in the previous
        // minute.  If not, we skipped a minute or more somewhere.
        if ( ((gust_last_write + 1) % 60) != current ) {
            // We lost some time somewhere: Reset the pointers, older gust data is
            // lost.  Start over collecting new gust data.

            gust_read_ptr = current - 1;    // Set read pointer back one, modulus 60
            if (gust_read_ptr < 0)
                gust_read_ptr = 59;

            gust[gust_read_ptr] = 0.0;
        }
        gust_last_write = current;
    }

    // Is current wind speed higher than the current minute bucket?
    if (wx_speed > gust[gust_write_ptr])
        gust[gust_write_ptr] = wx_speed;    // Save it in the bucket

    // Read the last (up to) five buckets and find the max gust
    computed_gust=gust[gust_write_ptr];
    j = gust_read_ptr;
    while (j != ((gust_write_ptr + 1) % 60) ) {
        if ( computed_gust < gust[j] )
            computed_gust = gust[j];
        j = (j + 1) % 60;
    }

    *last_speed_time = sec_now();
    return(computed_gust);
}





//*********************************************************************
// Decode Peet Brothers Ultimeter 2000 weather data (Data logging mode)
//
// This function is called from db.c:data_add() only.  Used for
// decoding incoming packets, not for our own weather station data.
//
// The Ultimeter 2000 can be in any of three modes, Data Logging Mode,
// Packet Mode, or Complete Record Mode.  This routine handles only
// the Data Logging Mode.
//*********************************************************************

void decode_U2000_L (int from, unsigned char *data, AprsWeatherRow *weather) {
    time_t last_speed_time;
    float last_speed;
    float computed_gust;
    char temp_data1[10];
    char *temp_conv;
    char format;

    last_speed = 0.0;
    last_speed_time = 0;
    computed_gust = 0.0;
    format = 0;

    weather->wx_type = WX_TYPE;
    xastir_snprintf(weather->wx_station,
        sizeof(weather->wx_station),
        "U2k");

    /* get last gust speed */
    if (strlen(weather->wx_gust) > 0 && !from) {
        /* get last speed */
        last_speed = (float)atof(weather->wx_gust);
        last_speed_time = weather->wx_speed_sec_time;
    }

    // 006B 00 58
    // 00A4 00 46 01FF 380E 2755 02C1 03E8 ---- 0052 04D7    0001 007BM
    // ^       ^  ^    ^    ^         ^                      ^
    // 0       6  8    12   16        24                     40
    /* wind speed */
    if (data[0] != '-') { // '-' signifies invalid data
        substr(temp_data1,(char *)data,4);
        xastir_snprintf(weather->wx_speed,
            sizeof(weather->wx_speed),
            "%03d",
            (int)(0.5 + ((float)strtol(temp_data1,&temp_conv,16)/10.0)*0.62137));
        if (from) {
            weather->wx_speed_sec_time = sec_now();
        } else {
            /* local station */
            computed_gust = compute_gust((float)atof(weather->wx_speed),
                                        last_speed,
                                        &last_speed_time);
            weather->wx_speed_sec_time = sec_now();
            xastir_snprintf(weather->wx_gust,
                sizeof(weather->wx_gust),
                "%03d",
                (int)(0.5 + computed_gust)); // Cheater's way of rounding
        }
    } else {
        if (!from)
            weather->wx_speed[0] = 0;
    }

    /* wind direction */
    //
    // Note that the first two digits here may be 00, or may be FF
    // if a direction calibration has been entered.  We should zero
    // them.
    //
    if (data[6] != '-') { // '-' signifies invalid data
        substr(temp_data1,(char *)(data+6),2);
        temp_data1[0] = '0';
        temp_data1[1] = '0';
        xastir_snprintf(weather->wx_course,
            sizeof(weather->wx_course),
            "%03d",
            (int)(((float)strtol(temp_data1,&temp_conv,16)/256.0)*360.0));
    } else {
        xastir_snprintf(weather->wx_course,
            sizeof(weather->wx_course),
            "000");
        if (!from)
            weather->wx_course[0]=0;
    }

    /* outdoor temp */
    if (data[8] != '-') { // '-' signifies invalid data
        int temp4;

        substr(temp_data1,(char *)(data+8),4);
        temp4 = (int)strtol(temp_data1,&temp_conv,16);
 
        if (temp_data1[0] > '7') {  // Negative value, convert
            temp4 = (temp4 & (temp4-0x7FFF)) - 0x8000;
        }

        xastir_snprintf(weather->wx_temp,
            sizeof(weather->wx_temp),
            "%03d",
            (int)((float)((temp4<<16)/65536)/10.0));

    }
    else {
        if (!from)
            weather->wx_temp[0]=0;
    }

    /* rain total long term */
    if (data[12] != '-') { // '-' signifies invalid data
        substr(temp_data1,(char *)(data+12),4);
        xastir_snprintf(weather->wx_rain_total,
            sizeof(weather->wx_rain_total),
            "%0.2f",
            (float)strtol(temp_data1,&temp_conv,16)/100.0);
        if (!from) {
            /* local station */
            compute_rain((float)atof(weather->wx_rain_total));
            /*last hour rain */
            xastir_snprintf(weather->wx_rain,
                sizeof(weather->wx_rain),
                "%0.2f",
                rain_minute_total);
            /*last 24 hour rain */
            xastir_snprintf(weather->wx_prec_24,
                sizeof(weather->wx_prec_24),
                "%0.2f",
                rain_24);
            /* rain since midnight */
            xastir_snprintf(weather->wx_prec_00,
                sizeof(weather->wx_prec_00),
                "%0.2f",
                rain_00);
        }
    } else {
        if (!from)
            weather->wx_rain_total[0]=0;
    }

    /* baro */
    if (data[16] != '-') { // '-' signifies invalid data
        substr(temp_data1,(char *)(data+16),4);
        xastir_snprintf(weather->wx_baro,
            sizeof(weather->wx_baro),
            "%0.1f",
            (float)strtol(temp_data1,&temp_conv,16)/10.0);
    } else {
        if (!from)
            weather->wx_baro[0]=0;
    }
    

    /* outdoor humidity */
    if (data[24] != '-') { // '-' signifies invalid data
        substr(temp_data1,(char *)(data+24),4);
        xastir_snprintf(weather->wx_hum,
            sizeof(weather->wx_hum),
            "%03d",
            (int)((float)strtol(temp_data1,&temp_conv,16)/10.0));
    } else {
        if (!from)
            weather->wx_hum[0]=0;
    }

    /* todays rain total */
    if (data[40] != '-') { // '-' signifies invalid data
        if (from) {
            substr(temp_data1,(char *)(data+40),4);
            xastir_snprintf(weather->wx_prec_00,
                sizeof(weather->wx_prec_00),
                "%0.2f",
                (float)strtol(temp_data1,&temp_conv,16)/100.0);
        }
    } else {
        if (!from)
            weather->wx_prec_00[0] = 0;
    }
}





//********************************************************************
// Decode Peet Brothers Ultimeter 2000 weather data (Packet mode)
//
// This function is called from db.c:data_add() only.  Used for
// decoding incoming packets, not for our own weather station data.
//
// The Ultimeter 2000 can be in any of three modes, Data Logging Mode,
// Packet Mode, or Complete Record Mode.  This routine handles only
// the Packet Mode.
//********************************************************************
void decode_U2000_P(int from, unsigned char *data, AprsWeatherRow *weather) {
    time_t last_speed_time;
    float last_speed;
    float computed_gust;
    char temp_data1[10];
    char *temp_conv;
    int len;

    last_speed      = 0.0;
    last_speed_time = 0;
    computed_gust   = 0.0;
    len = (int)strlen((char *)data);

    weather->wx_type = WX_TYPE;
    xastir_snprintf(weather->wx_station,
        sizeof(weather->wx_station),
        "U2k");

    /* get last gust speed */
    if (strlen(weather->wx_gust) > 0 && !from) {
        /* get last speed */
        last_speed = (float)atof(weather->wx_gust);
        last_speed_time = weather->wx_speed_sec_time;
    }

    // $ULTW   0031 00 37 02CE  0069 ---- 0000 86A0 0001 ---- 011901CC   0000 0005
    //         ^       ^  ^     ^    ^                   ^               ^    ^
    //         0       6  8     12   16                  32              44   48

    /* wind speed peak over last 5 min */
    if (data[0] != '-') { // '-' signifies invalid data
        substr(temp_data1,(char *)data,4);
        if (from) {
            xastir_snprintf(weather->wx_gust,
                sizeof(weather->wx_gust),
                "%03d",
                (int)(0.5 + ((float)strtol(temp_data1,&temp_conv,16)/10.0)*0.62137));
            /* this may be the only wind data */
            xastir_snprintf(weather->wx_speed,
                sizeof(weather->wx_speed),
                "%03d",
                (int)(0.5 + ((float)strtol(temp_data1,&temp_conv,16)/10.0)*0.62137));
        } else {
            /* local station and may be the only wind data */
            if (len < 51) {
                xastir_snprintf(weather->wx_speed,
                    sizeof(weather->wx_speed),
                    "%03d",
                    (int)(0.5 + ((float)strtol(temp_data1,&temp_conv,16)/10.0)*0.62137));
                computed_gust = compute_gust((float)atof(weather->wx_speed),
                                        last_speed,
                                        &last_speed_time);
                weather->wx_speed_sec_time = sec_now();
                xastir_snprintf(weather->wx_gust,
                    sizeof(weather->wx_gust),
                    "%03d",
                    (int)(0.5 + computed_gust));
            }
        }
    } else {
        if (!from)
            weather->wx_gust[0] = 0;
    }

    /* wind direction */
    //
    // Note that the first two digits here may be 00, or may be FF
    // if a direction calibration has been entered.  We should zero
    // them.
    //
    if (data[6] != '-') { // '-' signifies invalid data
        substr(temp_data1,(char *)(data+6),2);
        temp_data1[0] = '0';
        temp_data1[1] = '0';
        xastir_snprintf(weather->wx_course,
            sizeof(weather->wx_course),
            "%03d",
            (int)(((float)strtol(temp_data1,&temp_conv,16)/256.0)*360.0));
    } else {
        xastir_snprintf(weather->wx_course,
            sizeof(weather->wx_course),
            "000");
        if (!from)
            weather->wx_course[0] = 0;
    }

    /* outdoor temp */
    if (data[8] != '-') { // '-' signifies invalid data
        int temp4;

        substr(temp_data1,(char *)(data+8),4);
        temp4 = (int)strtol(temp_data1,&temp_conv,16);

        if (temp_data1[0] > '7') {  // Negative value, convert
            temp4 = (temp4 & (temp4-0x7FFF)) - 0x8000;
        }

        xastir_snprintf(weather->wx_temp,
            sizeof(weather->wx_temp),
            "%03d",
            (int)((float)((temp4<<16)/65536)/10.0));
    }
    else {
        if (!from)
            weather->wx_temp[0] = 0;
    }
    /* todays rain total (on some units) */
    if ((data[44]) != '-') { // '-' signifies invalid data
        if (from) {
            substr(temp_data1,(char *)(data+44),4);
            xastir_snprintf(weather->wx_prec_00,
                sizeof(weather->wx_prec_00),
                "%0.2f",
                (float)strtol(temp_data1,&temp_conv,16)/100.0);
        }
    } else {
        if (!from)
            weather->wx_prec_00[0] = 0;
    }

    /* rain total long term */
    if (data[12] != '-') { // '-' signifies invalid data
        substr(temp_data1,(char *)(data+12),4);
        xastir_snprintf(weather->wx_rain_total,
            sizeof(weather->wx_rain_total),
            "%0.2f",
            (float)strtol(temp_data1,&temp_conv,16)/100.0);
        if (!from) {
            /* local station */
            compute_rain((float)atof(weather->wx_rain_total));
            /*last hour rain */
            snprintf(weather->wx_rain,
                sizeof(weather->wx_rain),
                "%0.2f",
                rain_minute_total);
            /*last 24 hour rain */
            snprintf(weather->wx_prec_24,
                sizeof(weather->wx_prec_24),
                "%0.2f",
                rain_24);
            /* rain since midnight */
            snprintf(weather->wx_prec_00,
                sizeof(weather->wx_prec_00),
                "%0.2f",
                rain_00);
        }
    } else {
        if (!from)
            weather->wx_rain_total[0] = 0;
    }

    /* baro */
    if (data[16] != '-') { // '-' signifies invalid data
        substr(temp_data1,(char *)(data+16),4);
        xastir_snprintf(weather->wx_baro,
            sizeof(weather->wx_baro),
            "%0.1f",
            (float)strtol(temp_data1,&temp_conv,16)/10.0);
    } else {
        if (!from)
            weather->wx_baro[0] = 0;
    }

    /* outdoor humidity */
    if (data[32] != '-') { // '-' signifies invalid data
        substr(temp_data1,(char *)(data+32),4);
        xastir_snprintf(weather->wx_hum,
            sizeof(weather->wx_hum),
            "%03d",
            (int)((float)strtol(temp_data1,&temp_conv,16)/10.0));
    } else {
        if (!from)
            weather->wx_hum[0] = 0;
    }

    /* 1 min wind speed avg */
    if (len > 48 && (data[48]) != '-') { // '-' signifies invalid data
        substr(temp_data1,(char *)(data+48),4);
        xastir_snprintf(weather->wx_speed,
            sizeof(weather->wx_speed),
            "%03d",
            (int)(0.5 + ((float)strtol(temp_data1,&temp_conv,16)/10.0)*0.62137));
        if (from) {
            weather->wx_speed_sec_time = sec_now();
        } else {
            /* local station */
            computed_gust = compute_gust((float)atof(weather->wx_speed),
                                        last_speed,
                                        &last_speed_time);
            weather->wx_speed_sec_time = sec_now();
            xastir_snprintf(weather->wx_gust,
                sizeof(weather->wx_gust),
                "%03d",
                (int)(0.5 + computed_gust));
        }
    } else {
        if (!from) {
            if (len > 48)
                weather->wx_speed[0] = 0;
        }
    }
}





//*****************************************************************
// Decode Peet Brothers Ultimeter-II weather data
//
// This function is called from db.c:data_add() only.  Used for
// decoding incoming packets, not for our own weather station data.
//*****************************************************************
void decode_Peet_Bros(int from, unsigned char *data, AprsWeatherRow *weather, int type) {
    time_t last_speed_time;
    float last_speed;
    float computed_gust;
    char temp_data1[10];
    char *temp_conv;

    last_speed    = 0.0;
    computed_gust = 0.0;
    last_speed_time = 0;

    weather->wx_type = WX_TYPE;
    xastir_snprintf(weather->wx_station,
        sizeof(weather->wx_station),
        "UII");

    // '*' = MPH
    // '#' = km/h
    //
    // #  5 0B 75 0082 0082
    // *  7 00 76 0000 0000
    //    ^ ^  ^  ^
    //            rain [1/100 inch ?]
    //         outdoor temp
    //      wind speed [mph / km/h]
    //    wind dir

    /* wind direction */
    //
    // 0x00 is N
    // 0x04 is E
    // 0x08 is S
    // 0x0C is W
    //
    substr(temp_data1,(char *)data,1);
    snprintf(weather->wx_course,
        sizeof(weather->wx_course),
        "%03d",
        (int)(((float)strtol(temp_data1,&temp_conv,16)/16.0)*360.0));

    /* get last gust speed */
    if (strlen(weather->wx_gust) > 0 && !from) {
        /* get last speed */
        last_speed = (float)atof(weather->wx_gust);
        last_speed_time = weather->wx_speed_sec_time;
    }

    /* wind speed */
    substr(temp_data1,(char *)(data+1),2);
    if (type == APRS_WX4) {     // '#'  speed in km/h, convert to mph
        snprintf(weather->wx_speed,
            sizeof(weather->wx_speed),
            "%03d",
            (int)(0.5 + (float)(strtol(temp_data1,&temp_conv,16)*0.62137)));
    } else { // type == APRS_WX6,  '*'  speed in mph
        xastir_snprintf(weather->wx_speed,
            sizeof(weather->wx_speed),
            "%03d",
            (int)(0.5 + (float)strtol(temp_data1,&temp_conv,16)));
    }

    if (from) {
        weather->wx_speed_sec_time = sec_now();
    } else {
        /* local station */
        computed_gust = compute_gust((float)atof(weather->wx_speed),
                                last_speed,
                                &last_speed_time);
        weather->wx_speed_sec_time = sec_now();
        xastir_snprintf(weather->wx_gust,
            sizeof(weather->wx_gust),
            "%03d",
            (int)(0.5 + computed_gust));
    }

    /* outdoor temp */
    if (data[3] != '-') { // '-' signifies invalid data
        int temp4;

        substr(temp_data1,(char *)(data+3),2);
        temp4 = (int)strtol(temp_data1,&temp_conv,16);

        if (temp_data1[0] > '7') {  // Negative value, convert
            temp4 = (temp4 & (temp4-0x7FFF)) - 0x8000;
        }

        xastir_snprintf(weather->wx_temp,
            sizeof(weather->wx_temp),
            "%03d",
            temp4-56);
    } else {
        if (!from)
            weather->wx_temp[0] = 0;
    }

    // Rain divided by 100 for readings in hundredth of an inch
    if (data[5] != '-') { // '-' signifies invalid data
        substr(temp_data1,(char *)(data+5),4);
        xastir_snprintf(weather->wx_rain_total,
            sizeof(weather->wx_rain_total),
            "%0.2f",
            (float)strtol(temp_data1,&temp_conv,16)/100.0);
        if (!from) {
            /* local station */
            compute_rain((float)atof(weather->wx_rain_total));
            /*last hour rain */
            xastir_snprintf(weather->wx_rain,
                sizeof(weather->wx_rain),
                "%0.2f",
                rain_minute_total);
            /*last 24 hour rain */
            xastir_snprintf(weather->wx_prec_24,
                sizeof(weather->wx_prec_24),
                "%0.2f",
                rain_24);
            /* rain since midnight */
            xastir_snprintf(weather->wx_prec_00,
                sizeof(weather->wx_prec_00),
                "%0.2f",
                rain_00);
        }
    } else {
        if (!from)
            weather->wx_rain_total[0] = 0;
    }
}


#endif //INCLUDE_APRS
