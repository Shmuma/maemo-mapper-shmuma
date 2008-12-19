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

#ifndef MAEMO_MAPPER_APRS_MESSAGE
#define MAEMO_MAPPER_APRS_MESSAGE

#define MSG_INCREMENT 200

#include "types.h"


// Used for messages and bulletins
typedef struct {
    char active;
    TAprsPort port;
    char type;
    char heard_via_tnc;
    time_t sec_heard;
    time_t last_ack_sent;
    char packet_time[MAX_TIME];
    char call_sign[MAX_CALLSIGN+1];
    char from_call_sign[MAX_CALLSIGN+1];
    char message_line[MAX_MESSAGE_LENGTH+1];
    char seq[MAX_MESSAGE_ORDER+1];
    char acked;
    char position_known;
    time_t interval;
    int tries;
} Message;



// Struct used to create linked list of most recent ack's
typedef struct _ack_record {
    char callsign[MAX_CALLSIGN+1];
    char ack[5+1];
    struct _ack_record *next;
} ack_record;




int decode_message(gchar *call,gchar *path,gchar *message,gint port,gint third_party);


extern Message *msg_data;
extern long msg_index_end;
extern long *msg_index;
#endif

#endif //INCLUDE_APRS
