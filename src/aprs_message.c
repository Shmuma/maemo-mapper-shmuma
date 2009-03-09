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

#include "aprs_message.h"
#include "data.h"
#include "aprs.h"


/////////////////////////////////////////// Messages ///////////////////////////////////////////


long *msg_index;
long msg_index_end;
static long msg_index_max;

int log_wx_alert_data = 0;

Message *msg_data; // Array containing all messages,
                          // including ones we've transmitted (via
                          // loopback in the code)

time_t last_message_update = 0;
ack_record *ack_list_head = NULL;  // Head of linked list storing most recent ack's
int satellite_ack_mode;

int  new_message_data;
time_t last_message_remove;     // last time we did a check for message removing

// How often update_messages() will run, in seconds.
// This is necessary because routines like UpdateTime()
// call update_messages() VERY OFTEN.
//
// Actually, we just changed the code around so that we only call
// update_messages() with the force option, and only when we receive a
// message.  message_update_delay is no longer used, and we don't call
// update_messages() from UpdateTime() anymore.
static int message_update_delay = 300;




char *remove_trailing_spaces(char *data);
void update_messages(int force);




void clear_acked_message(char *from, char *to, char *seq) {
	// TODO - replace stub
}

int check_popup_window(char *from_call_sign, int group) {
	// TODO - replace stub
	return 1;
}

void bulletin_data_add(char *call_sign, char *from_call, char *data, char *seq, char type, TAprsPort port)
{
	// TODO - replace stub
}

int look_for_open_group_data(char *to) {
	// TODO - replace stub
	return 0;
}

void get_send_message_path(char *callsign, char *path, int path_size)
{
	// TODO - replace stub
}

void transmit_message_data_delayed(char *to, char *message,
                                   char *path, time_t when) {
	// TODO - replace stub
}

int process_directed_query(char *call,char *path,char *message,TAprsPort port) {
	// TODO - replace stub
	return 0;
}

void transmit_message_data(char *to, char *message, char *path)
{
	// TODO - replace stub
}

void popup_message(char *banner, char *message)
{
	
}

// Saves latest ack in a linked list.  We need this value in order
// to use Reply/Ack protocol when sending out messages.
void store_most_recent_ack(char *callsign, char *ack) {
    ack_record *p;
    int done = 0;
    char call[MAX_CALLSIGN+1];
    char new_ack[5+1];

    snprintf(call,
        sizeof(call),
        "%s",
        callsign);
    remove_trailing_spaces(call);

    // Get a copy of "ack".  We might need to change it.
    snprintf(new_ack,
        sizeof(new_ack),
        "%s",
        ack);

    // If it's more than 2 characters long, we can't use it for
    // Reply/Ack protocol as there's only space enough for two.
    // In this case we need to make sure that we blank out any
    // former ack that was 1 or 2 characters, so that communications
    // doesn't stop.
    if ( strlen(new_ack) > 2 ) {
        // It's too long, blank it out so that gets saved as "",
        // which will overwrite any previously saved ack's that were
        // short enough to use.
        new_ack[0] = '\0';
    }

    // Search for matching callsign through linked list
    p = ack_list_head;
    while ( !done && (p != NULL) ) {
        if (strcasecmp(call,p->callsign) == 0) {
            done++;
        }
        else {
            p = p->next;
        }
    }

    if (done) { // Found it.  Update the ack field.
        //fprintf(stderr,"Found callsign %s on recent ack list, Old:%s, New:%s\n",call,p->ack,new_ack);
        snprintf(p->ack,sizeof(p->ack),"%s",new_ack);
    }
    else {  // Not found.  Add a new record to the beginning of the
            // list.
        //fprintf(stderr,"New callsign %s, adding to list.  Ack: %s\n",call,new_ack);
        p = (ack_record *)malloc(sizeof(ack_record));
        CHECKMALLOC(p);

        snprintf(p->callsign,sizeof(p->callsign),"%s",call);
        snprintf(p->ack,sizeof(p->ack),"%s",new_ack);
        p->next = ack_list_head;
        ack_list_head = p;
    }
}





// Gets latest ack by callsign
char *get_most_recent_ack(char *callsign) {
    ack_record *p;
    int done = 0;
    char call[MAX_CALLSIGN+1];

    snprintf(call,
        sizeof(call),
        "%s",
        callsign);
    remove_trailing_spaces(call);

    // Search for matching callsign through linked list
    p = ack_list_head;
    while ( !done && (p != NULL) ) {
        if (strcasecmp(call,p->callsign) == 0) {
            done++;
        }
        else {
            p = p->next;
        }
    }

    if (done) { // Found it.  Return pointer to ack string.
        //fprintf(stderr,"Found callsign %s on linked list, returning ack: %s\n",call,p->ack);
        return(&p->ack[0]);
    }
    else {
        //fprintf(stderr,"Callsign %s not found\n",call);
        return(NULL);
    }
}





void init_message_data(void) {  // called at start of main

    new_message_data = 0;
    last_message_remove = sec_now();
}






void msg_clear_data(Message *clear) {
    int size;
    int i;
    unsigned char *data_ptr;

    data_ptr = (unsigned char *)clear;
    size=sizeof(Message);
    for(i=0;i<size;i++)
        *data_ptr++ = 0;
}



#ifdef MSG_DEBUG

void msg_copy_data(Message *to, Message *from) {
    int size;
    int i;
    unsigned char *data_ptr;
    unsigned char *data_ptr_from;

    data_ptr = (unsigned char *)to;
    data_ptr_from = (unsigned char *)from;
    size=sizeof(Message);
    for(i=0;i<size;i++)
        *data_ptr++ = *data_ptr_from++;
}
#endif /* MSG_DEBUG */





// Returns 1 if it's time to update the messages again
int message_update_time (void) {
    if ( sec_now() > (last_message_update + message_update_delay) )
        return(1);
    else
        return(0);
}





int msg_comp_active(const void *a, const void *b) {
    char temp_a[MAX_CALLSIGN+MAX_CALLSIGN+MAX_MESSAGE_ORDER+2];
    char temp_b[MAX_CALLSIGN+MAX_CALLSIGN+MAX_MESSAGE_ORDER+2];

    snprintf(temp_a, sizeof(temp_a), "%c%s%s%s",
            ((Message*)a)->active, ((Message*)a)->call_sign,
            ((Message*)a)->from_call_sign,
            ((Message*)a)->seq);
    snprintf(temp_b, sizeof(temp_b), "%c%s%s%s",
            ((Message*)b)->active, ((Message*)b)->call_sign,
            ((Message*)b)->from_call_sign,
            ((Message*)b)->seq);

    return(strcmp(temp_a, temp_b));
}





int msg_comp_data(const void *a, const void *b) {
    char temp_a[MAX_CALLSIGN+MAX_CALLSIGN+MAX_MESSAGE_ORDER+1];
    char temp_b[MAX_CALLSIGN+MAX_CALLSIGN+MAX_MESSAGE_ORDER+1];

    snprintf(temp_a, sizeof(temp_a), "%s%s%s",
            msg_data[*(long*)a].call_sign, msg_data[*(long *)a].from_call_sign,
            msg_data[*(long *)a].seq);
    snprintf(temp_b, sizeof(temp_b), "%s%s%s", msg_data[*(long*)b].call_sign,
            msg_data[*(long *)b].from_call_sign, msg_data[*(long *)b].seq);

    return(strcmp(temp_a, temp_b));
}





void msg_input_database(Message *m_fill) {
    void *m_ptr;
    long i;
    
//    fprintf(stderr, "DEBUG: Message: %s  %s\n", m_fill->call_sign, m_fill->message_line);
    
    if (msg_index_end == msg_index_max) {
        for (i = 0; i < msg_index_end; i++) {

            // Check for a record that is marked RECORD_NOTACTIVE.
            // If found, use that record instead of malloc'ing a new
            // one.
            if (msg_data[msg_index[i]].active == RECORD_NOTACTIVE) {

                // Found an unused record.  Fill it in.
                memcpy(&msg_data[msg_index[i]], m_fill, sizeof(Message));

// Sort msg_data
                qsort(msg_data, (size_t)msg_index_end, sizeof(Message), msg_comp_active);

                for (i = 0; i < msg_index_end; i++) {
                    msg_index[i] = i;
                    if (msg_data[i].active == RECORD_NOTACTIVE) {
                        msg_index_end = i;
                        break;
                    }
                }

// Sort msg_index
                qsort(msg_index, (size_t)msg_index_end, sizeof(long *), msg_comp_data);

                // All done with this message.
                return;
            }
        }

        // Didn't find free message record.  Fetch some more space.
        // Get more msg_data space.
        m_ptr = realloc(msg_data, (msg_index_max+MSG_INCREMENT)*sizeof(Message));
        if (m_ptr) {
            msg_data = m_ptr;

            // Get more msg_index space
            m_ptr = realloc(msg_index, (msg_index_max+MSG_INCREMENT)*sizeof(Message *));
            if (m_ptr) {
                msg_index = m_ptr;
                msg_index_max += MSG_INCREMENT;

//fprintf(stderr, "Max Message Array: %ld\n", msg_index_max);

            }
            else {
            	// TODO
                //XtWarning("Unable to allocate more space for message index.\n");
            }
        }
        else {
        	// TODO 
            //XtWarning("Unable to allocate more space for message database.\n");
        }
    }
    if (msg_index_end < msg_index_max) {
        msg_index[msg_index_end] = msg_index_end;

        // Copy message data into new message record.
        memcpy(&msg_data[msg_index_end++], m_fill, sizeof(Message));

// Sort msg_index
        qsort(msg_index, (size_t)msg_index_end, sizeof(long *), msg_comp_data);
    }
}





// Does a binary search through a sorted message database looking
// for a string match.
//
// If two or more messages match, this routine _should_ return the
// message with the latest timestamp.  This will ensure that earlier
// messages don't get mistaken for current messages, for the case
// where the remote station did a restart and is using the same
// sequence numbers over again.
//
long msg_find_data(Message *m_fill) {
    long record_start, record_mid, record_end, return_record, done;
    char tempfile[MAX_CALLSIGN+MAX_CALLSIGN+MAX_MESSAGE_ORDER+1];
    char tempfill[MAX_CALLSIGN+MAX_CALLSIGN+MAX_MESSAGE_ORDER+1];


    snprintf(tempfill, sizeof(tempfill), "%s%s%s",
            m_fill->call_sign,
            m_fill->from_call_sign,
            m_fill->seq);

    return_record = -1L;
    if (msg_index && msg_index_end >= 1) {
        /* more than one record */
         record_start=0L;
         record_end = (msg_index_end - 1);
         record_mid=(record_end-record_start)/2;

         done=0;
         while (!done) {

            /* get data for record start */
            snprintf(tempfile, sizeof(tempfile), "%s%s%s",
                    msg_data[msg_index[record_start]].call_sign,
                    msg_data[msg_index[record_start]].from_call_sign,
                    msg_data[msg_index[record_start]].seq);

            if (strcmp(tempfill, tempfile) < 0) {
                /* filename comes before */
                /*fprintf(stderr,"Before No data found!!\n");*/
                done=1;
                break;
            }
            else { /* get data for record end */

                snprintf(tempfile, sizeof(tempfile), "%s%s%s",
                        msg_data[msg_index[record_end]].call_sign,
                        msg_data[msg_index[record_end]].from_call_sign,
                        msg_data[msg_index[record_end]].seq);

                if (strcmp(tempfill,tempfile)>=0) { /* at end or beyond */
                    if (strcmp(tempfill, tempfile) == 0) {
                        return_record = record_end;
//fprintf(stderr,"record %ld",return_record);
                    }

                    done=1;
                    break;
                }
                else if ((record_mid == record_start) || (record_mid == record_end)) {
                    /* no mid for compare check to see if in the middle */
                    done=1;
                    snprintf(tempfile, sizeof(tempfile), "%s%s%s",
                            msg_data[msg_index[record_mid]].call_sign,
                            msg_data[msg_index[record_mid]].from_call_sign,
                            msg_data[msg_index[record_mid]].seq);
                    if (strcmp(tempfill,tempfile)==0) {
                        return_record = record_mid;
//fprintf(stderr,"record: %ld",return_record);
                    }
                }
            }
            if (!done) { /* get data for record mid */
                snprintf(tempfile, sizeof(tempfile), "%s%s%s",
                        msg_data[msg_index[record_mid]].call_sign,
                        msg_data[msg_index[record_mid]].from_call_sign,
                        msg_data[msg_index[record_mid]].seq);

                if (strcmp(tempfill, tempfile) == 0) {
                    return_record = record_mid;
//fprintf(stderr,"record %ld",return_record);
                    done = 1;
                    break;
                }

                if(strcmp(tempfill, tempfile)<0)
                    record_end = record_mid;
                else
                    record_start = record_mid;

                record_mid = record_start+(record_end-record_start)/2;
            }
        }
    }
    return(return_record);
}





void msg_replace_data(Message *m_fill, long record_num) {
    memcpy(&msg_data[msg_index[record_num]], m_fill, sizeof(Message));
}





void msg_get_data(Message *m_fill, long record_num) {
    memcpy(m_fill, &msg_data[msg_index[record_num]], sizeof(Message));
}





void msg_update_ack_stamp(long record_num) {

    //fprintf(stderr,"Attempting to update ack stamp: %ld\n",record_num);
    if ( (record_num >= 0) && (record_num < msg_index_end) ) {
        msg_data[msg_index[record_num]].last_ack_sent = sec_now();
        //fprintf(stderr,"Ack stamp: %ld\n",msg_data[msg_index[record_num]].last_ack_sent);
    }
    //fprintf(stderr,"\n\n\n*** Record: %ld ***\n\n\n",record_num);
}





// Called when we receive an ACK.  Sets the "acked" field in a
// Message which gets rid of the highlighting in the Send Message
// dialog for that message line.  This lets us know which messages
// have been acked and which have not.  If timeout is non-zero, then
// set acked to 2:  We use this in update_messages() to flag that
// "*TIMEOUT*" should prefix the string.  If cancelled is non-zero,
// set acked to 3:  We use this in update_messages() to flag that
// "*CANCELLED*" should prefix the string.
//
void msg_record_ack(char *to_call_sign,
                    char *my_call,
                    char *seq,
                    int timeout,
                    int cancel) {
    Message m_fill;
    long record;
    int do_update = 0;


    // Find the corresponding message in msg_data[i], set the
    // "acked" field to one.

    substr(m_fill.call_sign, to_call_sign, MAX_CALLSIGN);
    (void)remove_trailing_asterisk(m_fill.call_sign);

    substr(m_fill.from_call_sign, my_call, MAX_CALLSIGN);
    (void)remove_trailing_asterisk(m_fill.from_call_sign);

    substr(m_fill.seq, seq, MAX_MESSAGE_ORDER);
    (void)remove_trailing_spaces(m_fill.seq);
    (void)remove_leading_spaces(m_fill.seq);

    // Look for a message with the same to_call_sign, my_call,
    // and seq number
    record = msg_find_data(&m_fill);

    if (record == -1L) { // No match yet, try another tactic.
        if (seq[2] == '}' && strlen(seq) == 3) {

            // Try it again without the trailing '}' character
            m_fill.from_call_sign[2] = '\0';

            // Look for a message with the same to_call_sign,
            // my_call, and seq number (minus the trailing '}')
            record = msg_find_data(&m_fill);
        }
    }

    if(record != -1L) {     // Found a match!
        // Only cause an update if this is the first ack.  This
        // reduces dialog "flashing" a great deal
        if ( msg_data[msg_index[record]].acked == 0 ) {

            // Check for my callsign (including SSID).  If found,
            // update any open message dialogs
            if (is_my_call(msg_data[msg_index[record]].from_call_sign, 1) ) {

                //fprintf(stderr,"From: %s\tTo: %s\n",
                //    msg_data[msg_index[record]].from_call_sign,
                //    msg_data[msg_index[record]].call_sign);

                do_update++;
            }
        }
        else {  // This message has already been acked.
        }

        if (cancel)
            msg_data[msg_index[record]].acked = (char)3;
        else if (timeout)
            msg_data[msg_index[record]].acked = (char)2;
        else
            msg_data[msg_index[record]].acked = (char)1;

        // Set the interval to zero so that we don't display it
        // anymore in the dialog.  Same for tries.
        msg_data[msg_index[record]].interval = 0;
        msg_data[msg_index[record]].tries = 0;


    }


    if (do_update) {

        update_messages(1); // Force an update

        // Call check_popup_messages() here in order to pop up any
        // closed Send Message dialogs.  For first ack's or
        // CANCELLED messages it is less important, but for TIMEOUT
        // messages it is very important.
        //
// TODO
//        (void)check_popup_window(m_fill.call_sign, 2);  // Calls update_messages()
    }
}





// Called when we receive a REJ packet (reject).  Sets the "acked"
// field in a Message to 4 to indicate that the message has been
// rejected by the remote station.  This gets rid of the
// highlighting in the Send Message dialog for that message line.
// This lets us know which messages have been rejected and which
// have not.  We use this in update_messages() to flag that
// "*REJECTED*" should prefix the string.
//
// The most common source of REJ packets would be from sending to a
// D700A who's buffers are full, so that it can't take another
// message.
//
void msg_record_rej(char *to_call_sign,
                    char *my_call,
                    char *seq) {
    Message m_fill;
    long record;
    int do_update = 0;

    // Find the corresponding message in msg_data[i], set the
    // "acked" field to four.

    substr(m_fill.call_sign, to_call_sign, MAX_CALLSIGN);
    (void)remove_trailing_asterisk(m_fill.call_sign);

    substr(m_fill.from_call_sign, my_call, MAX_CALLSIGN);
    (void)remove_trailing_asterisk(m_fill.from_call_sign);

    substr(m_fill.seq, seq, MAX_MESSAGE_ORDER);
    (void)remove_trailing_spaces(m_fill.seq);
    (void)remove_leading_spaces(m_fill.seq);

    // Look for a message with the same to_call_sign, my_call,
    // and seq number
    record = msg_find_data(&m_fill);

    if (record == -1L) { // No match yet, try another tactic.
        if (seq[2] == '}' && strlen(seq) == 3) {

            // Try it again without the trailing '}' character
            m_fill.from_call_sign[2] = '\0';

            // Look for a message with the same to_call_sign,
            // my_call, and seq number (minus the trailing '}')
            record = msg_find_data(&m_fill);
        }
    }

    if(record != -1L) {     // Found a match!
        // Only cause an update if this is the first rej.  This
        // reduces dialog "flashing" a great deal
        if ( msg_data[msg_index[record]].acked == 0 ) {

            // Check for my callsign (including SSID).  If found,
            // update any open message dialogs
            if (is_my_call(msg_data[msg_index[record]].from_call_sign, 1) ) {

                //fprintf(stderr,"From: %s\tTo: %s\n",
                //    msg_data[msg_index[record]].from_call_sign,
                //    msg_data[msg_index[record]].call_sign);

                do_update++;
            }
        }
        else {  // This message has already been acked.
        }

        // Actually record the REJ here
        msg_data[msg_index[record]].acked = (char)4;

        // Set the interval to zero so that we don't display it
        // anymore in the dialog.  Same for tries.
        msg_data[msg_index[record]].interval = 0;
        msg_data[msg_index[record]].tries = 0;

    }


    if (do_update) {

        update_messages(1); // Force an update

        // Call check_popup_messages() here in order to pop up any
        // closed Send Message dialogs.  For first ack's or
        // CANCELLED messages it is less important, but for TIMEOUT
        // messages it is very important.
        //
// TODO
//        (void)check_popup_window(m_fill.call_sign, 2);  // Calls update_messages()
    }
}





// Called from check_and_transmit_messages().  Updates the interval
// field in our message record for the message currently being
// transmitted.  We'll use this in the Send Message dialog to
// display the current message interval.
//
void msg_record_interval_tries(char *to_call_sign,
                    char *my_call,
                    char *seq,
                    time_t interval,
                    int tries) {
    Message m_fill;
    long record;


    // Find the corresponding message in msg_data[i]

    substr(m_fill.call_sign, to_call_sign, MAX_CALLSIGN);
    (void)remove_trailing_asterisk(m_fill.call_sign);

    substr(m_fill.from_call_sign, my_call, MAX_CALLSIGN);
    (void)remove_trailing_asterisk(m_fill.from_call_sign);

    substr(m_fill.seq, seq, MAX_MESSAGE_ORDER);
    (void)remove_trailing_spaces(m_fill.seq);
    (void)remove_leading_spaces(m_fill.seq);

    // Look for a message with the same to_call_sign, my_call,
    // and seq number
    record = msg_find_data(&m_fill);
    if(record != -1L) {     // Found a match!

        msg_data[msg_index[record]].interval = interval;
        msg_data[msg_index[record]].tries = tries;
    }

    update_messages(1); // Force an update
}





// Returns: time_t for last_ack_sent
//          -1 if the message doesn't pass our tests
//           0 if it is a new message.
//
// Also returns the record number found if not passed a NULL pointer
// in record_out or -1L if it's a new record.
//
time_t msg_data_add(char *call_sign, char *from_call, char *data,
        char *seq, char type, TAprsPort port, long *record_out) {
    Message m_fill;
    long record;
    char time_data[MAX_TIME];
    int do_msg_update = 0;
    time_t last_ack_sent;
    int distance = -1;
    char temp[10];
    int group_message = 0;


//fprintf(stderr,"from:%s, to:%s, seq:%s\n", from_call, call_sign, seq);

    // Set the default output condition.  We'll change this later if
    // we need to.
    if (record_out != NULL)
        *record_out = -1l;

    // Check for some reasonable string in call_sign parameter
    if (call_sign == NULL || strlen(call_sign) == 0) {

        return((time_t)-1l);
    }
//else
//fprintf(stderr,"msg_data_add():call_sign: %s\n", call_sign);
 
    if ( (data != NULL) && (strlen(data) > MAX_MESSAGE_LENGTH) ) {

        return((time_t)-1l);
    }

    substr(m_fill.call_sign, call_sign, MAX_CALLSIGN);
    (void)remove_trailing_asterisk(m_fill.call_sign);

    substr(m_fill.from_call_sign, from_call, MAX_CALLSIGN);
    (void)remove_trailing_asterisk(m_fill.call_sign);

    substr(m_fill.seq, seq, MAX_MESSAGE_ORDER);
    (void)remove_trailing_spaces(m_fill.seq);
    (void)remove_leading_spaces(m_fill.seq);

// If the sequence number is blank, then it may have been a query,
// directed query, or group message.  Assume it is a new message in
// each case and add it.

    if (seq[0] != '\0') {   // Normal station->station messaging or
                            // bulletins
        // Look for a message with the same call_sign,
        // from_call_sign, and seq number
        record = msg_find_data(&m_fill);
//fprintf(stderr,"RECORD %ld  \n",record);
//fprintf(stderr,"Normal station->station message\n");
    }
    else {  // Group message/query/etc.
        record = -1L;
        group_message++;    // Flag it as a group message
//fprintf(stderr,"Group message/query/etc\n");
    }
    msg_clear_data(&m_fill);
    if(record != -1L) { /* fill old data */
        msg_get_data(&m_fill, record);
        last_ack_sent = m_fill.last_ack_sent;
        //fprintf(stderr,"Found: last_ack_sent: %ld\n",m_fill.last_ack_sent);

        //fprintf(stderr,"Found a duplicate message.  Updating fields, seq %s\n",seq);

        // If message is different this time, do an update to the
        // send message window and update the sec_heard field.  The
        // remote station must have restarted and is re-using the
        // sequence numbers.  What a pain!
        if (strcmp(m_fill.message_line,data) != 0) {
            m_fill.sec_heard = sec_now();
            last_ack_sent = (time_t)0;
//fprintf(stderr,"Message is different this time: Setting last_ack_sent to 0\n");
 
            if (type != MESSAGE_BULLETIN) { // Not a bulletin
                do_msg_update++;
            }
        }

        // If message is the same, but the sec_heard field is quite
        // old (more than 8 hours), the remote station must have
        // restarted, is re-using the sequence numbers, and just
        // happened to send the same message with the same sequence
        // number.  Again, what a pain!  Either that, or we
        // connected to a spigot with a _really_ long queue!
        if (m_fill.sec_heard < (sec_now() - (8 * 60 * 60) )) {
            m_fill.sec_heard = sec_now();
            last_ack_sent = (time_t)0;
//fprintf(stderr,"Found >8hrs old: Setting last_ack_sent to 0\n");

            if (type != MESSAGE_BULLETIN) { // Not a bulletin
                do_msg_update++;
            }
        }

        // Check for zero time
        if (m_fill.sec_heard == (time_t)0) {
            m_fill.sec_heard = sec_now();
            fprintf(stderr,"Zero time on a previous message.\n");
        }
    }
    else {
        // Only do this if it's a new message.  This keeps things
        // more in sequence by not updating the time stamps
        // constantly on old messages that don't get ack'ed.
        m_fill.sec_heard = sec_now();
        last_ack_sent = (time_t)0;
        //fprintf(stderr,"New msg: Setting last_ack_sent to 0\n");

        if (type != MESSAGE_BULLETIN) { // Not a bulletin
//fprintf(stderr,"Found new message\n");
            do_msg_update++;    // Always do an update to the
                                // message window for new messages
        }
    }

    /* FROM */
    m_fill.port =port;
    m_fill.active=RECORD_ACTIVE;
    m_fill.type=type;
    if (m_fill.heard_via_tnc != VIA_TNC)
        m_fill.heard_via_tnc = (port == APRS_PORT_TTY) ? VIA_TNC : NOT_VIA_TNC;

    distance = (int)(distance_from_my_station(from_call,temp, sizeof(temp)) + 0.9999);
 
    if (distance != 0) {    // Have a posit from the sending station
        m_fill.position_known = 1;
        //fprintf(stderr,"Position known: %s\n",from_call);
    }
    else {
        //fprintf(stderr,"Position not known: %s\n",from_call);
    }

    substr(m_fill.call_sign,call_sign,MAX_CALLSIGN);
    (void)remove_trailing_asterisk(m_fill.call_sign);

    substr(m_fill.from_call_sign,from_call,MAX_CALLSIGN);
    (void)remove_trailing_asterisk(m_fill.from_call_sign);

    // Update the message field
    substr(m_fill.message_line,data,MAX_MESSAGE_LENGTH);

    substr(m_fill.seq,seq,MAX_MESSAGE_ORDER);
    (void)remove_trailing_spaces(m_fill.seq);
    (void)remove_leading_spaces(m_fill.seq);

    // Create a timestamp from the current time
    snprintf(m_fill.packet_time,
        sizeof(m_fill.packet_time),
        "%s",
        get_time(time_data));

    if(record == -1L) {     // No old record found
        if (group_message)
            m_fill.acked = 1;   // Group msgs/queries need no ack
        else
            m_fill.acked = 0;   // We can't have been acked yet

        m_fill.interval = 0;
        m_fill.tries = 0;

        // We'll be sending an ack right away if this is a new
        // message, so might as well set the time now so that we
        // don't care about failing to set it in
        // msg_update_ack_stamp due to the record number being -1.
        m_fill.last_ack_sent = sec_now();

        msg_input_database(&m_fill);    // Create a new entry
        //fprintf(stderr,"No record found: Setting last_ack_sent to sec_now()00\n");
    }
    else {  // Old record found
        //fprintf(stderr,"Replacing the message in the database, seq %s\n",seq);
        msg_replace_data(&m_fill, record);  // Copy fields from m_fill to record
    }

    /* display messages */
// TODO
 //   if (type == MESSAGE_MESSAGE)
//        all_messages(from,call_sign,from_call,data);

    // Check for my callsign (including SSID).  If found, update any
    // open message dialogs
    if (       is_my_call(m_fill.from_call_sign, 1)
            || is_my_call(m_fill.call_sign, 1) ) {

        if (do_msg_update) {
            update_messages(1); // Force an update
        }
    }
 

    // Return the important variables we'll need
    if (record_out != NULL)
        *record_out = record;

//fprintf(stderr,"\nrecord_out:%ld record %ld\n",*record_out,record);
    return(last_ack_sent);

}   // End of msg_data_add()





// alert_data_add:  Function which adds NWS weather alerts to the
// alert hash.
//
// This function adds alerts directly to the alert hash, bypassing
// the message list and associated message-scan functions.
//
void alert_data_add(char *call_sign, char *from_call, char *data,
        char *seq, char type, TAprsPort port) {
    Message m_fill;
    char time_data[MAX_TIME];



/*
    if (log_wx_alert_data && from != DATA_VIA_FILE) {
        char temp_msg[MAX_MESSAGE_LENGTH+1];

        // Attempt to reconstruct the original weather alert packet
        // here, minus the path.
        snprintf(temp_msg,
            sizeof(temp_msg),
            "%s>APRS::%-9s:%s{%s",
            from_call,
            call_sign,
            data,
            seq);
        log_data( get_user_base_dir(LOGFILE_WX_ALERT), temp_msg);
//        fprintf(stderr, "%s\n", temp_msg);
    }
*/

    if ( (data != NULL) && (strlen(data) > MAX_MESSAGE_LENGTH) ) {
        return;
    }

    substr(m_fill.call_sign, call_sign, MAX_CALLSIGN);
    (void)remove_trailing_asterisk(m_fill.call_sign);

    substr(m_fill.from_call_sign, from_call, MAX_CALLSIGN);
    (void)remove_trailing_asterisk(m_fill.call_sign);

    substr(m_fill.seq, seq, MAX_MESSAGE_ORDER);
    (void)remove_trailing_spaces(m_fill.seq);
    (void)remove_leading_spaces(m_fill.seq);

    m_fill.sec_heard = sec_now();

    /* FROM */
    m_fill.port=port;
    m_fill.active=RECORD_ACTIVE;
    m_fill.type=type;

    // We don't have a value filled in yet here!
    //if (m_fill.heard_via_tnc != VIA_TNC)
    m_fill.heard_via_tnc = (port == APRS_PORT_TTY) ? VIA_TNC : NOT_VIA_TNC;

    substr(m_fill.call_sign,call_sign,MAX_CALLSIGN);
    (void)remove_trailing_asterisk(m_fill.call_sign);

    substr(m_fill.from_call_sign,from_call,MAX_CALLSIGN);
    (void)remove_trailing_asterisk(m_fill.from_call_sign);

    // Update the message field
    substr(m_fill.message_line,data,MAX_MESSAGE_LENGTH);

    substr(m_fill.seq,seq,MAX_MESSAGE_ORDER);
    (void)remove_trailing_spaces(m_fill.seq);
    (void)remove_leading_spaces(m_fill.seq);

    // Create a timestamp from the current time
    snprintf(m_fill.packet_time,
        sizeof(m_fill.packet_time),
        "%s",
        get_time(time_data));

    // Go try to add it to our alert hash.  alert_build_list() will
    // check for duplicates before adding it.

// TODO
//    alert_build_list(&m_fill);

    // This function fills in the Shapefile filename and index
    // so that we can later draw it.
// TODO
//    fill_in_new_alert_entries();


}   // End of alert_data_add()
 




// What I'd like to do for the following routine:  Use
// XmTextGetInsertionPosition() or XmTextGetCursorPosition() to
// find the last of the text.  Could also save the position for
// each SendMessage window.  Compare the timestamps of messages
// found with the last update time.  If newer, then add them to
// the end.  This should stop the incessant scrolling.

// Another idea, easier method:  Create a buffer.  Snag out the
// messages from the array and sort by time.  Put them into a
// buffer.  Figure out the length of the text widget, and append
// the extra length of the buffer onto the end of the text widget.
// Once the message data is turned into a linked list, it might
// be sorted already by time, so this window will look better
// anyway.

// Calling update_messages with force == 1 will cause an update
// no matter what message_update_time() says.
void update_messages(int force) {
    // TODO
}
/*
void update_messages(int force) {
    static XmTextPosition pos;
    char temp1[MAX_CALLSIGN+1];
    char temp2[500];
    char stemp[20];
    long i;
    int mw_p;
    char *temp_ptr;


    if ( message_update_time() || force) {

//fprintf(stderr,"update_messages()\n");

        //fprintf(stderr,"Um %d\n",(int)sec_now() );

        // go through all mw_p's! 

        // Perform this for each message window
        for (mw_p=0; msg_index && mw_p < MAX_MESSAGE_WINDOWS; mw_p++) {
            //pos=0;

begin_critical_section(&send_message_dialog_lock, "db.c:update_messages" );

            if (mw[mw_p].send_message_dialog!=NULL) {

//fprintf(stderr,"\n");

//fprintf(stderr,"found send_message_dialog\n");

                // Clear the text from message window
                XmTextReplace(mw[mw_p].send_message_text,
                    (XmTextPosition) 0,
                    XmTextGetLastPosition(mw[mw_p].send_message_text),
                    "");

                // Snag the callsign you're dealing with from the message dialogue
                if (mw[mw_p].send_message_call_data != NULL) {
                    temp_ptr = XmTextFieldGetString(mw[mw_p].send_message_call_data);
                    snprintf(temp1,
                        sizeof(temp1),
                        "%s",
                        temp_ptr);
                    XtFree(temp_ptr);

                    new_message_data--;
                    if (new_message_data<0)
                        new_message_data=0;

                    if(strlen(temp1)>0) {   // We got a callsign from the dialog so
                        // create a linked list of the message indexes in time-sorted order

                        typedef struct _index_record {
                            int index;
                            time_t sec_heard;
                            struct _index_record *next;
                        } index_record;
                        index_record *head = NULL;
                        index_record *p_prev = NULL;
                        index_record *p_next = NULL;

                        // Allocate the first record (a dummy record)
                        head = (index_record *)malloc(sizeof(index_record));
                        CHECKMALLOC(head);

                        head->index = -1;
                        head->sec_heard = (time_t)0;
                        head->next = NULL;

                        (void)remove_trailing_spaces(temp1);
                        (void)to_upper(temp1);

                        pos = 0;
                        // Loop through looking for messages to/from
                        // that callsign (including SSID)
                        for (i = 0; i < msg_index_end; i++) {
                            if (msg_data[msg_index[i]].active == RECORD_ACTIVE
                                    && (strcmp(temp1, msg_data[msg_index[i]].from_call_sign) == 0
                                        || strcmp(temp1,msg_data[msg_index[i]].call_sign) == 0)
                                    && (is_my_call(msg_data[msg_index[i]].from_call_sign, 1)
                                        || is_my_call(msg_data[msg_index[i]].call_sign, 1)
                                        || mw[mw_p].message_group ) ) {
                                int done = 0;

                                // Message matches our parameters so
                                // save the relevant data about the
                                // message in our linked list.  Compare
                                // the sec_heard field to see whether
                                // we're higher or lower, and insert the
                                // record at the correct spot in the
                                // list.  We end up with a time-sorted
                                // list.
                                p_prev  = head;
                                p_next = p_prev->next;
                                while (!done && (p_next != NULL)) {  // Loop until end of list or record inserted

                                    //fprintf(stderr,"Looping, looking for insertion spot\n");

                                    if (p_next->sec_heard <= msg_data[msg_index[i]].sec_heard) {
                                        // Advance one record
                                        p_prev = p_next;
                                        p_next = p_prev->next;
                                    }
                                    else {  // We found the correct insertion spot
                                        done++;
                                    }
                                }

                                //fprintf(stderr,"Inserting\n");

                                // Add the record in between p_prev and
                                // p_next, even if we're at the end of
                                // the list (in that case p_next will be
                                // NULL.
                                p_prev->next = (index_record *)malloc(sizeof(index_record));
                                CHECKMALLOC(p_prev->next);

                                p_prev->next->next = p_next; // Link to rest of records or NULL
                                p_prev->next->index = i;
                                p_prev->next->sec_heard = msg_data[msg_index[i]].sec_heard;
// Remember to free this entire linked list before exiting the loop for
// this message window!
                            }
                        }
                        // Done processing the entire list for this
                        // message window.

                        //fprintf(stderr,"Done inserting/looping\n");

                        if (head->next != NULL) {   // We have messages to display
                            int done = 0;

                            //fprintf(stderr,"We have messages to display\n");

                            // Run through the linked list and dump the
                            // info out.  It's now in time-sorted order.

// Another optimization would be to keep a count of records added, then
// later when we were dumping it out to the window, only dump the last
// XX records out.

                            p_prev = head->next;    // Skip the first dummy record
                            p_next = p_prev->next;
                            while (!done && (p_prev != NULL)) {  // Loop until end of list
                                int j = p_prev->index;  // Snag the index out of the record
                                char prefix[50];
                                char interval_str[50];
                                int offset = 22;    // Offset for highlighting


                                //fprintf(stderr,"\nLooping through, reading messages\n");
 
//fprintf(stderr,"acked: %d\n",msg_data[msg_index[j]].acked);
 
                                // Message matches so snag the important pieces into a string
                                snprintf(stemp, sizeof(stemp),
                                    "%c%c/%c%c %c%c:%c%c",
                                    msg_data[msg_index[j]].packet_time[0],
                                    msg_data[msg_index[j]].packet_time[1],
                                    msg_data[msg_index[j]].packet_time[2],
                                    msg_data[msg_index[j]].packet_time[3],
                                    msg_data[msg_index[j]].packet_time[8],
                                    msg_data[msg_index[j]].packet_time[9],
                                    msg_data[msg_index[j]].packet_time[10],
                                    msg_data[msg_index[j]].packet_time[11]
                                );

// Somewhere in here we appear to be losing the first message.  It
// doesn't get written to the window later in the QSO.  Same for
// closing the window and re-opening it, putting the same callsign
// in and pressing "New Call" button.  First message is missing.

                                // Label the message line with who sent it.
                                // If acked = 2 a timeout has occurred
                                // If acked = 3 a cancel has occurred
                                if (msg_data[msg_index[j]].acked == 2) {
                                    snprintf(prefix,
                                        sizeof(prefix),
                                        "%s ",
                                        langcode("WPUPMSB016") ); // "*TIMEOUT*"
                                }
                                else if (msg_data[msg_index[j]].acked == 3) {
                                    snprintf(prefix,
                                        sizeof(prefix),
                                        "%s ",
                                        langcode("WPUPMSB017") ); // "*CANCELLED*"
                                }
                                else if (msg_data[msg_index[j]].acked == 4) {
                                    snprintf(prefix,
                                        sizeof(prefix),
                                        "%s ",
                                        langcode("WPUPMSB018") ); // "*REJECTED*"
                                }
                                else prefix[0] = '\0';

                                if (msg_data[msg_index[j]].interval) {
                                    snprintf(interval_str,
                                        sizeof(interval_str),
                                        ">%d/%lds",
                                        msg_data[msg_index[j]].tries + 1,
                                        (long)msg_data[msg_index[j]].interval);

                                    // Don't highlight the interval
                                    // value
                                    offset = offset + strlen(interval_str);
                                }
                                else {
                                    interval_str[0] = '\0';
                                }

                                snprintf(temp2, sizeof(temp2),
                                    "%s %-9s%s>%s%s\n",
                                    // Debug code.  Trying to find sorting error
                                    //"%ld  %s  %-9s>%s\n",
                                    //msg_data[msg_index[j]].sec_heard,
                                    stemp,
                                    msg_data[msg_index[j]].from_call_sign,
                                    interval_str,
                                    prefix,
                                    msg_data[msg_index[j]].message_line);

//fprintf(stderr,"message: %s\n", msg_data[msg_index[j]].message_line);
//fprintf(stderr,"update_messages: %s|%s", temp1, temp2);
 
                                // Replace the text from pos to pos+strlen(temp2) by the string "temp2"
                                if (mw[mw_p].send_message_text != NULL) {

                                    // Insert the text at the end
//                                    XmTextReplace(mw[mw_p].send_message_text,
//                                            pos,
//                                            pos+strlen(temp2),
//                                            temp2);

                                    XmTextInsert(mw[mw_p].send_message_text,
                                            pos,
                                            temp2);
 
                                    // Set highlighting based on the
                                    // "acked" field.  Callsign
                                    // match here includes SSID.
//fprintf(stderr,"acked: %d\t",msg_data[msg_index[j]].acked);
                                    if ( (msg_data[msg_index[j]].acked == 0)    // Not acked yet
                                            && ( is_my_call(msg_data[msg_index[j]].from_call_sign, 1)) ) {
//fprintf(stderr,"Setting underline\t");
                                        XmTextSetHighlight(mw[mw_p].send_message_text,
                                            pos+offset,
                                            pos+strlen(temp2),
                                            //XmHIGHLIGHT_SECONDARY_SELECTED); // Underlining
                                            XmHIGHLIGHT_SELECTED);         // Reverse Video
                                    }
                                    else {  // Message was acked, get rid of highlighting
//fprintf(stderr,"Setting normal\t");
                                        XmTextSetHighlight(mw[mw_p].send_message_text,
                                            pos+offset,
                                            pos+strlen(temp2),
                                            XmHIGHLIGHT_NORMAL);
                                    }

//fprintf(stderr,"Text: %s\n",temp2); 

                                    pos += strlen(temp2);

                                }

                                // Advance to the next record in the list
                                p_prev = p_next;
                                if (p_next != NULL)
                                    p_next = p_prev->next;

                            }   // End of while
                        }   // End of if
                        else {  // No messages matched, list is empty
                        }

// What does this do?  Move all of the text?
//                        if (pos > 0) {
//                            if (mw[mw_p].send_message_text != NULL) {
//                                XmTextReplace(mw[mw_p].send_message_text,
//                                        --pos,
//                                        XmTextGetLastPosition(mw[mw_p].send_message_text),
//                                        "");
//                            }
//                        }

                        //fprintf(stderr,"Free'ing list\n");

                        // De-allocate the linked list
                        p_prev = head;
                        while (p_prev != NULL) {

                            //fprintf(stderr,"You're free!\n");

                            p_next = p_prev->next;
                            free(p_prev);
                            p_prev = p_next;
                        }

                        // Show the last added message in the window
                        XmTextShowPosition(mw[mw_p].send_message_text,
                            pos);
                    }
                }
            }

end_critical_section(&send_message_dialog_lock, "db.c:update_messages" );

        }
        last_message_update = sec_now();

//fprintf(stderr,"Message index end: %ld\n",msg_index_end);
 
    }
}
*/




void mdelete_messages_from(char *from) {
    long i;

    // Mark message records with RECORD_NOTACTIVE.  This will mark
    // them for re-use.
    for (i = 0; msg_index && i < msg_index_end; i++)
        if (strcmp(msg_data[i].call_sign, _aprs_mycall) == 0 
        		&& strcmp(msg_data[i].from_call_sign, from) == 0)
            msg_data[i].active = RECORD_NOTACTIVE;
}





void mdelete_messages_to(char *to) {
    long i;

    // Mark message records with RECORD_NOTACTIVE.  This will mark
    // them for re-use.
    for (i = 0; msg_index && i < msg_index_end; i++)
        if (strcmp(msg_data[i].call_sign, to) == 0)
            msg_data[i].active = RECORD_NOTACTIVE;
}





void mdelete_messages(char *to_from) {
    long i;

    // Mark message records with RECORD_NOTACTIVE.  This will mark
    // them for re-use.
    for (i = 0; msg_index && i < msg_index_end; i++)
        if (strcmp(msg_data[i].call_sign, to_from) == 0 || strcmp(msg_data[i].from_call_sign, to_from) == 0)
            msg_data[i].active = RECORD_NOTACTIVE;
}





void mdata_delete_type(const char msg_type, const time_t reference_time) {
    long i;

    // Mark message records with RECORD_NOTACTIVE.  This will mark
    // them for re-use.
    for (i = 0; msg_index && i < msg_index_end; i++)

        if ((msg_type == '\0' || msg_type == msg_data[i].type)
                && msg_data[i].active == RECORD_ACTIVE
                && msg_data[i].sec_heard < reference_time)

            msg_data[i].active = RECORD_NOTACTIVE;
}





void check_message_remove(time_t curr_sec) {       // called in timing loop

    // Time to check for old messages again?  (Currently every ten
    // minutes)
#ifdef EXPIRE_DEBUG
    if ( last_message_remove < (curr_sec - DEBUG_MESSAGE_REMOVE_CYCLE) ) {
#else // EXPIRE_DEBUG
    if ( last_message_remove < (curr_sec - MESSAGE_REMOVE_CYCLE) ) {
#endif

        // Yes it is.  Mark all messages that are older than
        // sec_remove with the RECORD_NOTACTIVE flag.  This will
        // mark them for re-use.
#ifdef EXPIRE_DEBUG
        mdata_delete_type('\0', curr_sec-DEBUG_MESSAGE_REMOVE);
#else   // EXPIRE_DEBUG
        mdata_delete_type('\0', curr_sec-_aprs_sec_remove);
#endif

        last_message_remove = curr_sec;
    }

    // Should we sort them at this point so that the unused ones are
    // near the end?  It looks like the message input functions do
    // this, so I guess we don't need to do it here.
}





void mscan_file(char msg_type, void (*function)(Message *)) {
    long i;

    for (i = 0; msg_index && i < msg_index_end; i++)
        if ((msg_type == '\0' || msg_type == msg_data[msg_index[i]].type) &&
                msg_data[msg_index[i]].active == RECORD_ACTIVE)
            function(&msg_data[msg_index[i]]);
}





void mprint_record(Message *m_fill) {
/*
    fprintf(stderr,
        "%-9s>%-9s %s:%5s %s:%c :%s\n",
        m_fill->from_call_sign,
        m_fill->call_sign,
        langcode("WPUPMSB013"), // "seq"
        m_fill->seq,
        langcode("WPUPMSB014"), // "type"
        m_fill->type,
        m_fill->message_line);
*/
}





void mdisplay_file(char msg_type) {
    fprintf(stderr,"\n\n");
    mscan_file(msg_type, mprint_record);
    fprintf(stderr,"\tmsg_index_end %ld, msg_index_max %ld\n", msg_index_end, msg_index_max);
}





int decode_message(gchar *call,gchar *path,gchar *message,gint port,gint third_party) 
{
    char *temp_ptr;
    char ipacket_message[300];
    char message_plus_acks[MAX_MESSAGE_LENGTH + 10];
    char from_call[MAX_CALLSIGN+1];
    char ack[20];
    int ok, len;
    char addr[9+1];
    char addr9[9+1];
    char msg_id[5+1];
    char orig_msg_id[5+1];
    char ack_string[6];
    int done;
    int reply_ack = 0;
    int to_my_call = 0;
    int to_my_base_call = 0;
    int from_my_call = 0;


    // :xxxxxxxxx:____0-67____             message              printable, except '|', '~', '{'
    // :BLNn     :____0-67____             general bulletin     printable, except '|', '~'
    // :BLNnxxxxx:____0-67____           + Group Bulletin
    // :BLNX     :____0-67____             Announcement
    // :NWS-xxxxx:____0-67____             NWS Service Bulletin
    // :NWS_xxxxx:____0-67____             NWS Service Bulletin
    // :xxxxxxxxx:ackn1-5n               + ack
    // :xxxxxxxxx:rejn1-5n               + rej
    // :xxxxxxxxx:____0-67____{n1-5n     + message
    // :NTS....
    //  01234567890123456
    // 01234567890123456    old
    // we get message with already extracted data ID


    if (is_my_call(call, 1) ) { // Check SSID also
        from_my_call++;
    }

    ack_string[0] = '\0';   // Clear out the Reply/Ack result string

    len = (int)strlen(message);
    ok = (int)(len > 9 && message[9] == ':');

    if (ok) {
    	
//fprintf(stderr,"DEBUG: decode_message: from %s: %s\n", call, message);
//return 0;
    	
        substr(addr9,message,9); // extract addressee
        snprintf(addr,
            sizeof(addr),
            "%s",
            addr9);
        (void)remove_trailing_spaces(addr);

        if (is_my_call(addr,1)) { // Check includes SSID
            to_my_call++;
        }

        if (is_my_call(addr,0)) { // Check ignores SSID.  We use
                                  // this to catch messages to some
                                  // of our other SSID's
            to_my_base_call++;
        }

        message = message + 10; // pointer to message text

        // Save the message text and the acks/reply-acks before we
        // extract the acks below.
        snprintf(message_plus_acks,
            sizeof(message_plus_acks),
            "%s",
            message);

        temp_ptr = strrchr(message,'{'); // look for message ID after
                                         //*last* { in message.
        msg_id[0] = '\0';
        if (temp_ptr != NULL) {
            substr(msg_id,temp_ptr+1,5); // extract message ID, could be non-digit
            temp_ptr[0] = '\0';          // adjust message end (chops off message ID)
        }

        // Save the original msg_id away.
        snprintf(orig_msg_id,
            sizeof(orig_msg_id),
            "%s",
            msg_id);

        // Check for Reply/Ack protocol in msg_id, which looks like
        // this:  "{XX}BB", where XX is the sequence number for the
        // message, and BB is the ack for the previous message from
        // my station.  I've also seen this from APRS+: "{XX}B", so
        // perhaps this is also possible "{X}B" or "{X}BB}".  We can
        // also get auto-reply responses from APRS+ that just have
        // "}X" or "}XX" at the end.  We decode those as well.
        //

        temp_ptr = strstr(msg_id,"}"); // look for Reply Ack in msg_id

        if (temp_ptr != NULL) { // Found Reply/Ack protocol!
 
            reply_ack++;

// Put this code into the UI message area as well (if applicable).

            // Separate out the extra ack so that we can deal with
            // it properly.
            snprintf(ack_string,
                sizeof(ack_string),
                "%s",
                temp_ptr+1); // After the '}' character!

            // Terminate it here so that rest of decode works
            // properly.  We can get duplicate messages
            // otherwise.
            //
// Note that we modify msg_id here.  Use orig_msg_id if we need the
// unmodified version (full REPLY-ACK version) later.
            //
            temp_ptr[0] = '\0'; // adjust msg_id end

        }
        else {  // Look for Reply Ack in message without sequence
                // number
            temp_ptr = strstr(message,"}");

            if (temp_ptr != NULL) {
                int yy = 0;

                reply_ack++;

// Put this code into the UI message area as well (if applicable).
                snprintf(ack_string,
                    sizeof(ack_string),
                    "%s",
                    temp_ptr+1);    // After the '}' character!

                ack_string[yy] = '\0';  // Terminate the string

                // Terminate it here so that rest of decode works
                // properly.  We can get duplicate messages
                // otherwise.
                temp_ptr[0] = '\0'; // adjust message end
            } 
        }

        done = 0;
    }
    else {
        done = 1;                               // fall through...
    }

    len = (int)strlen(message);
    //--------------------------------------------------------------------------
    if (!done && len > 3 && strncmp(message,"ack",3) == 0) {              // ACK

        // Received an ACK packet.  Note that these can carry the
        // REPLY-ACK protocol or a single ACK sequence number plus
        // perhaps an extra '}' on the end.  They should have one of
        // these formats:
        //      ack1        Normal ACK
        //      ackY        Normal ACK
        //      ack23       Normal ACK
        //      ackfH       Normal ACK
        //      ack23{      REPLY-ACK Protocol
        //      ack2Q}3d    REPLY-ACK Protocol

        substr(msg_id,message+3,5);
        // fprintf(stderr,"ACK: %s: |%s| |%s|\n",call,addr,msg_id);

        if (to_my_call) { // Check SSID also

            // Note:  This function handles REPLY-ACK protocol just
            // fine, stripping off the 2nd ack if present.  It uses
            // only the first sequence number.
            clear_acked_message(call,addr,msg_id);  // got an ACK for me

            // This one also handles REPLY-ACK protocol just fine.
            msg_record_ack(call,addr,msg_id,0,0);   // Record the ack for this message
        }
        else {  // ACK is for another station
            // Now if I have Igate on and I allow to retransmit station data
            // check if this message is to a person I have heard on my TNC within an X
            // time frame. If if is a station I heard and all the conditions are ok
            // spit the ACK out on the TNC -FG
/*
 * TODO - Add igate support
            if (operate_as_an_igate>1
                    && from==DATA_VIA_NET
                    && !from_my_call     // Check SSID also
                    && port != -1) {    // Not from a log file
                char short_path[100];

                shorten_path(path,short_path,sizeof(short_path));

                // Only send '}' and the ack_string if it's not
                // empty, else just end the packet with the message
                // string.  This keeps us from appending a '}' when
                // it's not called for.
                snprintf(ipacket_message,
                    sizeof(ipacket_message),
                    "}%s>%s,TCPIP,%s*::%s:%s",
 
                    call,
                    short_path,
                    my_callsign,
                    addr9,
                    message_plus_acks);


                output_igate_rf(call,
                    addr,
                    path,
                    ipacket_message,
                    port,
                    third_party,
                    NULL);

                igate_msgs_tx++;
            }
*/
        }
        done = 1;
    }
    //--------------------------------------------------------------------------
    if (!done && len > 3 && strncmp(message,"rej",3) == 0) {              // REJ

        substr(msg_id,message+3,5);

        if (to_my_call) {   // Check SSID also

            // Note:  This function handles REPLY-ACK protocol just
            // fine, stripping off the 2nd ack if present.  It uses
            // only the first sequence number.
            clear_acked_message(call,addr,msg_id);  // got an REJ for me

            // This one also handles REPLY-ACK protocol just fine.
            msg_record_rej(call,addr,msg_id);   // Record the REJ for this message
        }
        else {  // REJ is for another station
            /* Now if I have Igate on and I allow to retransmit station data           */
            /* check if this message is to a person I have heard on my TNC within an X */
            /* time frame. If if is a station I heard and all the conditions are ok    */
            /* spit the REJ out on the TNC                                             */
        	
/*
 * TODO - Add igate support
            if (operate_as_an_igate>1
                    && from==DATA_VIA_NET
                    && !from_my_call    // Check SSID also
                    && port != -1) {    // Not from a log file
                char short_path[100];

                shorten_path(path,short_path,sizeof(short_path));

                // Only send '}' and the rej_string if it's not
                // empty, else just end the packet with the message
                // string.  This keeps us from appending a '}' when
                // it's not called for.
                snprintf(ipacket_message,
                    sizeof(ipacket_message),
                    "}%s>%s,TCPIP,%s*::%s:%s",
 
                    call,
                    short_path,
                    my_callsign,
                    addr9,
                    message_plus_acks);


                output_igate_rf(call,
                    addr,
                    path,
                    ipacket_message,
                    port,
                    third_party,
                    NULL);

                igate_msgs_tx++;
            }
*/
        }

        done = 1;
    }
    
    //--------------------------------------------------------------------------
    if (!done && strncmp(addr,"BLN",3) == 0) {                       // Bulletin
        // fprintf(stderr,"found BLN: |%s| |%s|\n",addr,message);
        bulletin_data_add(addr,call,message,"",MESSAGE_BULLETIN,port);
        done = 1;
    }

    //--------------------------------------------------------------------------
    if (!done && strlen(msg_id) > 0 && to_my_call) {         // Message for me (including SSID check)
                                                             // with msg_id (sequence number)
        time_t last_ack_sent;
        long record;


// Remember to put this code into the UI message area as well (if
// applicable).

        // Check for Reply/Ack
        if (reply_ack && strlen(ack_string) != 0) { // Have a free-ride ack to deal with

            clear_acked_message(call,addr,ack_string);  // got an ACK for me

            msg_record_ack(call,addr,ack_string,0,0);   // Record the ack for this message
        }

        // Save the ack 'cuz we might need it while talking to this
        // station.  We need it to implement Reply/Ack protocol.

// Note that msg_id has already been truncated by this point.
// orig_msg_id contains the full REPLY-ACK text.

//fprintf(stderr, "store_most_recent_ack()\n");
        store_most_recent_ack(call,msg_id);
 
        // fprintf(stderr,"found Msg w line to me: |%s| |%s|\n",message,msg_id);
        last_ack_sent = msg_data_add(addr,
                            call,
                            message,
                            msg_id,
                            MESSAGE_MESSAGE,
                            port, 
                            &record); // id_fixed

        // Here we need to know if it is a new message or an old.
        // If we've already received it, we don't want to kick off
        // the alerts or pop up the Send Message dialog again.  If
        // last_ack_sent == (time_t)0, then it is a new message.
        //
        if (last_ack_sent == (time_t)0l && record == -1l) { // Msg we've never received before

            new_message_data += 1;

            // Note that the check_popup_window() function will
            // re-create a Send Message dialog if one doesn't exist
            // for this QSO.  Only call it for the first message
            // line or the first ack, not for any repeats.
            //
            (void)check_popup_window(call, 2);  // Calls update_messages()

            //update_messages(1); // Force an update


        }

        // Try to only send an ack out once per 30 seconds at the
        // fastest.
//WE7U
// Does this 30-second check work?
        //
        if ( (last_ack_sent != (time_t)-1l)   // Not an error
                && (last_ack_sent + 30 ) < sec_now()
                && !satellite_ack_mode // Disable separate ack's for satellite work
                && port != -1 ) {   // Not from a log file

            char path[MAX_LINE_SIZE+1];

            // Update the last_ack_sent field for the message
            msg_update_ack_stamp(record);

            pad_callsign(from_call,call);         /* ack the message */


            // Attempt to snag a custom path out of the Send Message
            // dialog, if set.  If not set, path will contain '\0';
            get_send_message_path(call, path, MAX_LINE_SIZE+1);
//fprintf(stderr,"Path: %s\n", path);


            // In this case we want to send orig_msg_id back, not
            // the (possibly) truncated msg_id.  This is per Bob B's
            // Reply/Ack spec, sent to xastir-dev on Nov 14, 2001.
            snprintf(ack, sizeof(ack), ":%s:ack%s",from_call,orig_msg_id);

//WE7U
// Need to figure out the reverse path for this one instead of
// passing a NULL for the path?  Probably not, as auto-calculation
// of paths isn't a good idea.
//
// What we need to do here is check whether we have a custom path
// set for this QSO.  If so, pass that path along as the transmit
// path.  messages.h:Message_Window struct has the send_message_path
// variable in it.  If a Message_Window still exists for this QSO
// then we can snag the user-entered path from there.  If the struct
// has already been destroyed then we have nowhere to snag the
// custom path from and have to rely on the default paths in each
// interface properties dialog instead.  Then again, we _could_ snag
// the path out of the last received message in the message database
// for that case.  Might be better to disable the Close button, or
// warn the user that the custom path will be lost if they close the
// Send Message dialog.


            // Send out the immediate ACK
            if (path[0] == '\0')
                transmit_message_data(call,ack,NULL);
            else
                transmit_message_data(call,ack,path);


            if (record != -1l) { // Msg we've received before

                // It's a message that we've received before,
                // consider sending an extra ACK in about 30 seconds
                // to try to get it to the remote station.  Perhaps
                // another one in 60 seconds as well.

                if (path[0] == '\0') {
                    transmit_message_data_delayed(call,ack,NULL,sec_now()+30);
                    transmit_message_data_delayed(call,ack,NULL,sec_now()+60);
                    transmit_message_data_delayed(call,ack,NULL,sec_now()+120);
                }
                else {
                    transmit_message_data_delayed(call,ack,path,sec_now()+30);
                    transmit_message_data_delayed(call,ack,path,sec_now()+60);
                    transmit_message_data_delayed(call,ack,path,sec_now()+120);
                }
            }

/*
 * TODO 
            if (auto_reply == 1) {

                snprintf(ipacket_message,
                    sizeof(ipacket_message), "AA:%s", auto_reply_message);

                if (!from_my_call) // Check SSID also
                    output_message(my_callsign, call, ipacket_message, "");
            }
*/
        }


        done = 1;
    }

    //--------------------------------------------------------------------------
    if (!done && strlen(msg_id) == 0 && to_my_call) {   // Message for me (including SSID check)
                                                        // but without message-ID.
        // These should appear in a Send Message dialog and should
        // NOT get ack'ed.  Kenwood radios send this message type as
        // an auto-answer or a buffer-full message.  They look
        // something like:
        //
        //      :WE7U-13 :Not at keyboard.
        //

        time_t last_ack_sent;
        long record;


        if (len > 2
                && message[0] == '?'
                && port != -1   // Not from a log file
                && to_my_call) { // directed query (check SSID also)
            // Smallest query known is "?WX".
            done = process_directed_query(call,path,message+1,port);
        }

        // fprintf(stderr,"found Msg w line to me: |%s| |%s|\n",message,msg_id);
        last_ack_sent = msg_data_add(addr,
                            call,
                            message,
                            msg_id,
                            MESSAGE_MESSAGE,
                            port,
                            &record); // id_fixed

        // Here we need to know if it is a new message or an old.
        // If we've already received it, we don't want to kick off
        // the alerts or pop up the Send Message dialog again.  If
        // last_ack_sent == (time_t)0, then it is a new message.
        //
        if (last_ack_sent == (time_t)0l && record == -1l) { // Msg we've never received before

            new_message_data += 1;

            // Note that the check_popup_window() function will
            // re-create a Send Message dialog if one doesn't exist
            // for this QSO.  Only call it for the first message
            // line or the first ack, not for any repeats.
            //
//fprintf(stderr,"***check_popup_window 1\n");
            (void)check_popup_window(call, 2);  // Calls update_messages()

            //update_messages(1); // Force an update

        }

        // Update the last_ack_sent field for the message, even
        // though we won't be sending an ack in response.
        msg_update_ack_stamp(record);


        done = 1;
    }

    //--------------------------------------------------------------------------
    if (!done
            && ( (strncmp(addr,"NWS-",4) == 0)          // NWS weather alert
              || (strncmp(addr,"NWS_",4) == 0) ) ) {    // NWS weather alert compressed

        // could have sort of line number
        //fprintf(stderr,"found NWS: |%s| |%s| |%s|\n",addr,message,msg_id);

        (void)alert_data_add(addr,
            call,
            message,
            msg_id,
            MESSAGE_NWS,
            port);

        done = 1;
        
/*
 * TODO - add igate support
        if (operate_as_an_igate>1
                && from==DATA_VIA_NET
                && !from_my_call // Check SSID also
                && port != -1) { // Not from a log file
            char short_path[100];

            shorten_path(path,short_path,sizeof(short_path));

            snprintf(ipacket_message,
                sizeof(ipacket_message),
                "}%s>%s,TCPIP,%s*::%s:%s",
                call,
                short_path,
                my_callsign,
                addr9,
                message);

            output_nws_igate_rf(call,
                path,
                ipacket_message,
                port,
                third_party);
        }
*/
    }

    //--------------------------------------------------------------------------
    if (!done && strncmp(addr,"SKY",3) == 0) {  // NWS weather alert additional info

        // could have sort of line number
        //fprintf(stderr,"found SKY: |%s| |%s| |%s|\n",addr,message,msg_id);

		// We don't wish to record these in memory.  They cause an infinite
		// loop in the current code and a massive memory leak.
		return(1);  // Tell the calling program that the packet was ok so
		            // that it doesn't add it with data_add() itself!
		

        done = 1;
/*
 * TODO - add igate support
        if (operate_as_an_igate>1
                && from==DATA_VIA_NET
                && !from_my_call    // Check SSID also
                && port != -1) { // Not from a log file
            char short_path[100];

            shorten_path(path,short_path,sizeof(short_path));

            snprintf(ipacket_message,
                sizeof(ipacket_message),
                "}%s>%s,TCPIP,%s*::%s:%s",
                call,
                short_path,
                my_callsign,
                addr9,
                message);

            output_nws_igate_rf(call,
                path,
                ipacket_message,
                port,
                third_party);
        }
*/
    }

    //--------------------------------------------------------------------------
    if (!done && strlen(msg_id) > 0) {  // Other message with linenumber.  This
                                        // is either a message for someone else
                                        // or a message for another one of my
                                        // SSID's.
        long record_out;
        time_t last_ack_sent;
        char message_plus_note[MAX_MESSAGE_LENGTH + 30];
 

        if (to_my_base_call && !from_my_call) {
            // Special case:  We saw a message w/msg_id that was to
            // one of our other SSID's, but it was not from
            // ourselves.  That last bit (!from_my_call) is
            // important in the case where we're working an event
            // with several stations using the same callsign.
            //
            // Store as if it came to my callsign, with a zeroed-out
            // msg_id so we can't try to ack it.  We also need some
            // other indication in the "Send Message" dialog as to
            // what's happening.  Perhaps add the original callsign
            // to the message itself in a note at the start?
            //
            snprintf(message_plus_note,
                sizeof(message_plus_note),
                "(Sent to:%s) %s",
                addr,
                message);
            last_ack_sent = msg_data_add(_aprs_mycall,
                call,
                message_plus_note,
                "",
                MESSAGE_MESSAGE,
                port,
                &record_out);
        }
        else {  // Normal case, messaging between other people
            last_ack_sent = msg_data_add(addr,
                call,
                message,
                msg_id,
                MESSAGE_MESSAGE,
                port,
                &record_out);
        }
 
        new_message_data += look_for_open_group_data(addr);
 
        // Note that the check_popup_window() function will
        // re-create a Send Message dialog if one doesn't exist for
        // this QSO.  Only call it for the first message line or the
        // first ack, not for any repeats.
        //
        if (last_ack_sent == (time_t)0l && record_out == -1l) { // Msg we've never received before

            // Callsign check here also checks SSID for exact match
// We need to do an SSID-non-specific check here so that we can pick
// up messages intended for other stations of ours.

            if ((to_my_base_call && check_popup_window(call, 2) != -1)
                    || check_popup_window(call, 0) != -1
                    || check_popup_window(addr, 1) != -1) {
                update_messages(1); // Force an update
            }
        }

        /* Now if I have Igate on and I allow to retransmit station data           */
        /* check if this message is to a person I have heard on my TNC within an X */
        /* time frame. If if is a station I heard and all the conditions are ok    */
        /* spit the message out on the TNC -FG                                     */
  /*
   * TODO - add igate support
        if (operate_as_an_igate>1
                && last_ack_sent != (time_t)-1l
                && from==DATA_VIA_NET
                && !from_my_call        // Check SSID also
                && !to_my_call          // Check SSID also
                && port != -1) {    // Not from a log file
            char short_path[100];

            shorten_path(path,short_path,sizeof(short_path));
            snprintf(ipacket_message,
                sizeof(ipacket_message),
                "}%s>%s,TCPIP,%s*::%s:%s",
                call,
                short_path,
                my_callsign,
                addr9,
                message_plus_acks);


            output_igate_rf(call,
                addr,
                path,
                ipacket_message,
                port,
                third_party,
                NULL);

            igate_msgs_tx++;
        }
*/
        done = 1;
    }

    //--------------------------------------------------------------------------

    if (!done) {                                   // message without line number
        long record_out;
        time_t last_ack_sent;


        last_ack_sent = msg_data_add(addr,
            call,
            message,
            "",
            MESSAGE_MESSAGE,
            port,
            &record_out);

        new_message_data++;      // ??????

        // Note that the check_popup_window() function will
        // re-create a Send Message dialog if one doesn't exist for
        // this QSO.  Only call it for the first message line or the
        // first ack, not for any repeats.
        //
        if (last_ack_sent == (time_t)0l && record_out == -1l) { // Msg we've never received before
            if (check_popup_window(addr, 1) != -1) {
                //update_messages(1); // Force an update
            }
        }

        // Could be response to a query.  Popup a messsage.

// Check addr for my_call and !third_party, then check later in the
// packet for my_call if it is a third_party message?  Depends on
// what the packet looks like by this point.
        if ( last_ack_sent != (time_t)-1l
                && (message[0] != '?')
                && to_my_call ) { // Check SSID also

            // We no longer wish to have both popups and the Send
            // Group Message dialogs come up for every query
            // response, so we use popup_message() here instead of
            // popup_message_always() so that by default we'll see
            // the below message in STDERR.  If --with-errorpopups
            // has been configured in, we'll get a popup as well.
            // Send Group Message dialogs work well for multi-line
            // query responses, so we'll leave it that way.
            //
// TODO
//            popup_message(langcode("POPEM00018"),message);

            // Check for Reply/Ack.  APRS+ sends an AA: response back
            // for auto-reply, with an embedded free-ride Ack.
            if (strlen(ack_string) != 0) {  // Have an extra ack to deal with

                clear_acked_message(call,addr,ack_string);  // got an ACK for me

                msg_record_ack(call,addr,ack_string,0,0);   // Record the ack for this message
            }
        }
 
        // done = 1;
    }

    //--------------------------------------------------------------------------

    if (ok)
        (void)data_add(STATION_CALL_DATA,
            call,
            path,
            message,
            port,
            NULL,
            third_party,
            0,  // Not a packet from my station
            0); // Not my object/item


    return(ok);
}

#endif //INCLUDE_APRS
