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

#define _GNU_SOURCE

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <hildon-widgets/hildon-note.h>
#include <hildon-widgets/hildon-banner.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-inet-connection.h>
#include <errno.h>

#include "types.h"
#include "data.h"
#include "defines.h"

#include "display.h"
#include "gps.h"
#include "gpsbt.h"
#include "path.h"
#include "util.h"

static volatile GThread *_gps_thread = NULL;
static GMutex *_gps_init_mutex = NULL;
static volatile gint _gps_rcvr_retry_count = 0;

static gint _gmtoffset = 0;


#define MACRO_PARSE_INT(tofill, str) { \
    gchar *error_check; \
    (tofill) = strtol((str), &error_check, 10); \
    if(error_check == (str)) \
    { \
        g_printerr("Line %d: Failed to parse string as int: %s\n", \
                __LINE__, str); \
        MACRO_BANNER_SHOW_INFO(_window, \
                _("Invalid NMEA input from receiver!")); \
        return; \
    } \
}
#define MACRO_PARSE_FLOAT(tofill, str) { \
    gchar *error_check; \
    (tofill) = g_ascii_strtod((str), &error_check); \
    if(error_check == (str)) \
    { \
        g_printerr("Failed to parse string as float: %s\n", str); \
        MACRO_BANNER_SHOW_INFO(_window, \
                _("Invalid NMEA input from receiver!")); \
        return; \
    } \
}
static void
gps_parse_rmc(gchar *sentence)
{
    /* Recommended Minimum Navigation Information C
     *  1) UTC Time
     *  2) Status, V=Navigation receiver warning A=Valid
     *  3) Latitude
     *  4) N or S
     *  5) Longitude
     *  6) E or W
     *  7) Speed over ground, knots
     *  8) Track made good, degrees true
     *  9) Date, ddmmyy
     * 10) Magnetic Variation, degrees
     * 11) E or W
     * 12) FAA mode indicator (NMEA 2.3 and later)
     * 13) Checksum
     */
    gchar *token, *dpoint, *gpsdate = NULL;
    gdouble tmpd = 0.f;
    gint tmpi = 0;
    gboolean newly_fixed = FALSE;
    vprintf("%s(): %s\n", __PRETTY_FUNCTION__, sentence);

#define DELIM ","

    /* Parse time. */
    token = strsep(&sentence, DELIM);
    if(token && *token)
        gpsdate = token;

    token = strsep(&sentence, DELIM);
    /* Token is now Status. */
    if(token && *token == 'A')
    {
        /* Data is valid. */
        if(_gps_state < RCVR_FIXED)
        {
            newly_fixed = TRUE;
            set_conn_state(RCVR_FIXED);
        }
    }
    else
    {
        /* Data is invalid - not enough satellites?. */
        if(_gps_state > RCVR_UP)
        {
            set_conn_state(RCVR_UP);
            track_insert_break(FALSE);
        }
    }

    /* Parse the latitude. */
    token = strsep(&sentence, DELIM);
    if(token && *token)
    {
        dpoint = strchr(token, '.');
        if(!dpoint) /* handle buggy NMEA */
            dpoint = token + strlen(token);
        MACRO_PARSE_FLOAT(tmpd, dpoint - 2);
        dpoint[-2] = '\0';
        MACRO_PARSE_INT(tmpi, token);
        _gps.lat = tmpi + (tmpd * (1.0 / 60.0));
    }

    /* Parse N or S. */
    token = strsep(&sentence, DELIM);
    if(token && *token == 'S')
        _gps.lat = -_gps.lat;

    /* Parse the longitude. */
    token = strsep(&sentence, DELIM);
    if(token && *token)
    {
        dpoint = strchr(token, '.');
        if(!dpoint) /* handle buggy NMEA */
            dpoint = token + strlen(token);
        MACRO_PARSE_FLOAT(tmpd, dpoint - 2);
        dpoint[-2] = '\0';
        MACRO_PARSE_INT(tmpi, token);
        _gps.lon = tmpi + (tmpd * (1.0 / 60.0));
    }

    /* Parse E or W. */
    token = strsep(&sentence, DELIM);
    if(token && *token == 'W')
        _gps.lon = -_gps.lon;

    /* Parse speed over ground, knots. */
    token = strsep(&sentence, DELIM);
    if(token && *token)
    {
        MACRO_PARSE_FLOAT(_gps.speed, token);
        if(_gps.fix > 1)
            _gps.maxspeed = MAX(_gps.maxspeed, _gps.speed);
    }

    /* Parse heading, degrees from true north. */
    token = strsep(&sentence, DELIM);
    if(token && *token)
    {
        MACRO_PARSE_FLOAT(_gps.heading, token);
    }

    /* Parse date. */
    token = strsep(&sentence, DELIM);
    if(token && *token && gpsdate)
    {
        struct tm time;
        gpsdate[6] = '\0'; /* Make sure time is 6 chars long. */
        strcat(gpsdate, token);
        strptime(gpsdate, "%H%M%S%d%m%y", &time);
        _pos.time = mktime(&time) + _gmtoffset;
    }
    else
        _pos.time = time(NULL);

    /* Translate data into integers. */
    latlon2unit(_gps.lat, _gps.lon, _pos.unitx, _pos.unity);

    /* Add new data to track. */
    if(_gps_state == RCVR_FIXED)
    {
        if(track_add(_pos.time, newly_fixed))
        {
            /* Move mark to new location. */
            map_refresh_mark(FALSE);
        }
        else
        {
            map_move_mark();
        }
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

static void
gps_parse_gga(gchar *sentence)
{
    /* GGA          Global Positioning System Fix Data
     1. Fix Time
     2. Latitude
     3. N or S
     4. Longitude
     5. E or W
     6. Fix quality
                   0 = invalid
                   1 = GPS fix (SPS)
                   2 = DGPS fix
                   3 = PPS fix
                   4 = Real Time Kinematic
                   5 = Float RTK
                   6 = estimated (dead reckoning) (2.3 feature)
                   7 = Manual input mode
                   8 = Simulation mode
     7. Number of satellites being tracked
     8. Horizontal dilution of position
     9. Altitude, Meters, above mean sea level
     10. Alt unit (meters)
     11. Height of geoid (mean sea level) above WGS84 ellipsoid
     12. unit
     13. (empty field) time in seconds since last DGPS update
     14. (empty field) DGPS station ID number
     15. the checksum data
     */
    gchar *token;
    vprintf("%s(): %s\n", __PRETTY_FUNCTION__, sentence);

#define DELIM ","

    /* Skip Fix time */
    token = strsep(&sentence, DELIM);
    /* Skip latitude */
    token = strsep(&sentence, DELIM);
    /* Skip N or S */
    token = strsep(&sentence, DELIM);
    /* Skip longitude */
    token = strsep(&sentence, DELIM);
    /* Skip S or W */
    token = strsep(&sentence, DELIM);

    /* Parse Fix quality */
    token = strsep(&sentence, DELIM);
    if(token && *token)
        MACRO_PARSE_INT(_gps.fixquality, token);

    /* Skip number of satellites */
    token = strsep(&sentence, DELIM);

    /* Parse Horizontal dilution of position */
    token = strsep(&sentence, DELIM);
    if(token && *token)
        MACRO_PARSE_INT(_gps.hdop, token);

    /* Altitude */
    token = strsep(&sentence, DELIM);
    if(token && *token)
    {
        MACRO_PARSE_FLOAT(_pos.altitude, token);
    }
    else
        _pos.altitude = 0;

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

static void
gps_parse_gsa(gchar *sentence)
{
    /* GPS DOP and active satellites
     *  1) Auto selection of 2D or 3D fix (M = manual)
     *  2) 3D fix - values include: 1 = no fix, 2 = 2D, 3 = 2D
     *  3) PRNs of satellites used for fix
     *     (space for 12)
     *  4) PDOP (dilution of precision)
     *  5) Horizontal dilution of precision (HDOP)
     *  6) Vertical dilution of precision (VDOP)
     *  7) Checksum
     */
    gchar *token;
    gint i;
    vprintf("%s(): %s\n", __PRETTY_FUNCTION__, sentence);

#define DELIM ","

    /* Skip Auto selection. */
    token = strsep(&sentence, DELIM);

    /* 3D fix. */
    token = strsep(&sentence, DELIM);
    if(token && *token)
        MACRO_PARSE_INT(_gps.fix, token);

    _gps.satinuse = 0;
    for(i = 0; i < 12; i++)
    {
        token = strsep(&sentence, DELIM);
        if(token && *token)
            _gps.satforfix[_gps.satinuse++] = atoi(token);
    }

    /* PDOP */
    token = strsep(&sentence, DELIM);
    if(token && *token)
        MACRO_PARSE_FLOAT(_gps.pdop, token);

    /* HDOP */
    token = strsep(&sentence, DELIM);
    if(token && *token)
        MACRO_PARSE_FLOAT(_gps.hdop, token);

    /* VDOP */
    token = strsep(&sentence, DELIM);
    if(token && *token)
        MACRO_PARSE_FLOAT(_gps.vdop, token);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

static void
gps_parse_gsv(gchar *sentence)
{
    /* Must be GSV - Satellites in view
     *  1) total number of messages
     *  2) message number
     *  3) satellites in view
     *  4) satellite number
     *  5) elevation in degrees (0-90)
     *  6) azimuth in degrees to true north (0-359)
     *  7) SNR in dB (0-99)
     *  more satellite infos like 4)-7)
     *  n) checksum
     */
    gchar *token;
    gint msgcnt = 0, nummsgs = 0;
    static gint running_total = 0;
    static gint num_sats_used = 0;
    static gint satcnt = 0;
    vprintf("%s(): %s\n", __PRETTY_FUNCTION__, sentence);

    /* Parse number of messages. */
    token = strsep(&sentence, DELIM);
    if(token && *token)
        MACRO_PARSE_INT(nummsgs, token);

    /* Parse message number. */
    token = strsep(&sentence, DELIM);
    if(token && *token)
        MACRO_PARSE_INT(msgcnt, token);

    /* Parse number of satellites in view. */
    token = strsep(&sentence, DELIM);
    if(token && *token)
    {
        MACRO_PARSE_INT(_gps.satinview, token);
        if(_gps.satinview > 12) /* Handle buggy NMEA. */
            _gps.satinview = 12;
    }

    /* Loop until there are no more satellites to parse. */
    while(sentence && satcnt < 12)
    {
        /* Get token for Satellite Number. */
        token = strsep(&sentence, DELIM);
        if(token && *token)
            _gps_sat[satcnt].prn = atoi(token);

        /* Get token for elevation in degrees (0-90). */
        token = strsep(&sentence, DELIM);
        if(token && *token)
            _gps_sat[satcnt].elevation = atoi(token);

        /* Get token for azimuth in degrees to true north (0-359). */
        token = strsep(&sentence, DELIM);
        if(token && *token)
            _gps_sat[satcnt].azimuth = atoi(token);

        /* Get token for SNR. */
        token = strsep(&sentence, DELIM);
        if(token && *token && (_gps_sat[satcnt].snr = atoi(token)))
        {
            /* SNR is non-zero - add to total and count as used. */
            running_total += _gps_sat[satcnt].snr;
            num_sats_used++;
        }
        satcnt++;
    }

    if(msgcnt == nummsgs)
    {
        /*  This is the last message. Calculate signal strength. */
        if(num_sats_used)
        {
            if(_gps_state == RCVR_UP)
            {
                gdouble fraction = running_total * sqrtf(num_sats_used)
                    / num_sats_used / 100.0;
                BOUND(fraction, 0.0, 1.0);
                hildon_banner_set_fraction(
                        HILDON_BANNER(_fix_banner), fraction);

                /* Keep awake while they watch the progress bar. */
                UNBLANK_SCREEN(TRUE, FALSE);
            }
            running_total = 0;
            num_sats_used = 0;
        }
        satcnt = 0;
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

static gboolean
gps_handle_error_idle(gchar *error)
{
    printf("%s(%s)\n", __PRETTY_FUNCTION__, error);

    /* Ask for re-try. */
    if(++_gps_rcvr_retry_count > 2)
    {
        GtkWidget *confirm;
        gchar buffer[BUFFER_SIZE];

        /* Reset retry count. */
        _gps_rcvr_retry_count = 0;

        snprintf(buffer, sizeof(buffer), "%s\nRetry?", error);
        confirm = hildon_note_new_confirmation(GTK_WINDOW(_window), buffer);

        rcvr_disconnect();

        if(GTK_RESPONSE_OK == gtk_dialog_run(GTK_DIALOG(confirm)))
            rcvr_connect(); /* Try again. */
        else
        {
            /* Disable GPS. */
            gtk_check_menu_item_set_active(
                    GTK_CHECK_MENU_ITEM(_menu_enable_gps_item), FALSE);
        }

        /* Ask user to re-connect. */
        gtk_widget_destroy(confirm);
    }
    else
    {
        rcvr_disconnect();
        rcvr_connect(); /* Try again. */
    }

    g_free(error);

    vprintf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
    return FALSE;
}

static gboolean
gps_parse_nmea_idle(gchar *nmea)
{
    printf("%s(%s)\n", __PRETTY_FUNCTION__, nmea);

    if(_enable_gps && _gps_state >= RCVR_DOWN)
    {
        if(!strncmp(nmea + 3, "GSV", 3))
        {
            if(_gps_state == RCVR_UP || _gps_info || _satdetails_on)
                gps_parse_gsv(nmea + 7);
        }
        else if(!strncmp(nmea + 3, "RMC", 3))
            gps_parse_rmc(nmea + 7);
        else if(!strncmp(nmea + 3, "GGA", 3))
            gps_parse_gga(nmea + 7);
        else if(!strncmp(nmea + 3, "GSA", 3))
            gps_parse_gsa(nmea + 7);

        if(_gps_info)
            gps_display_data();
        if(_satdetails_on)
            gps_display_details();
    }

    g_free(nmea);

    vprintf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
    return FALSE;
}

static GpsRcvrInfo*
gri_clone(GpsRcvrInfo *gri)
{
    GpsRcvrInfo *ret = g_new(GpsRcvrInfo, 1);
    ret->type = gri->type;
    ret->bt_mac = g_strdup(gri->bt_mac);
    ret->file_path = g_strdup(gri->file_path);
    ret->gpsd_host = g_strdup(gri->gpsd_host);
    ret->gpsd_port = gri->gpsd_port;
    return ret;
}

static void
gri_free(GpsRcvrInfo *gri)
{
    g_free(gri->bt_mac);
    g_free(gri->file_path);
    g_free(gri->gpsd_host);
    g_free(gri);
}

static void
thread_read_nmea(GpsRcvrInfo *gri)
{
    gchar buf[BUFFER_SIZE];
    gchar *buf_curr = buf;
    gchar *buf_last = buf + sizeof(buf) - 1;
    GnomeVFSFileSize bytes_read;
    GnomeVFSResult vfs_result;
    gchar *gpsd_host = NULL;
    gint gpsd_port = 0;
    GnomeVFSInetConnection *iconn = NULL;
    GnomeVFSSocket *socket = NULL;
    GThread *my_thread = g_thread_self();
    gboolean error = FALSE;
    gpsbt_t gps_context;
    gboolean is_context = FALSE;

    printf("%s(%d)\n", __PRETTY_FUNCTION__, gri->type);

    /* Lock/Unlock the mutex to ensure that _gps_thread is done being set. */
    g_mutex_lock(_gps_init_mutex);
    g_mutex_unlock(_gps_init_mutex);

    switch(gri->type)
    {
        case GPS_RCVR_BT:
        {
            gchar errstr[BUFFER_SIZE] = "";
            /* We need to start gpsd (via gpsbt) first. */
            memset(&gps_context, 0, sizeof(gps_context));
            errno = 0;
            if(gpsbt_start(gri->bt_mac, 0, 0, 0, errstr, sizeof(errstr),
                        0, &gps_context) < 0)
            {
                g_printerr("Error connecting to GPS receiver: (%d) %s (%s)\n",
                        errno, strerror(errno), errstr);
                g_idle_add((GSourceFunc)gps_handle_error_idle,
                        g_strdup_printf("%s",
                        _("Error connecting to GPS receiver.")));
                error = TRUE;
            }
            else
            {
                /* Success.  Set gpsd_host and gpsd_port. */
                gpsd_host = "127.0.0.1";
                gpsd_port = GPSD_PORT_DEFAULT;
                is_context = TRUE;
            }
            break;
        }
        case GPS_RCVR_GPSD:
        {
            /* Set gpsd_host and gpsd_port. */
            gpsd_host = gri->gpsd_host;
            gpsd_port = gri->gpsd_port;
            break;
        }
        case GPS_RCVR_FILE:
        {
            /* Use gpsmgr to create a GPSD that uses the file. */
            gchar *gpsd_prog;
            gchar *gpsd_ctrl_sock;
            gchar *devs[2];
            devs[0] = gri->file_path;
            devs[1] = NULL;

            /* Caller can override the name of the gpsd program and
             * the used control socket. */
            gpsd_prog = getenv("GPSD_PROG");
            gpsd_ctrl_sock = getenv("GPSD_CTRL_SOCK");

            if (!gpsd_prog)
                gpsd_prog = "gpsd";

            memset(&gps_context, 0, sizeof(gps_context));
            errno = 0;
            if(gpsmgr_start(gpsd_prog, devs, gpsd_ctrl_sock,
                        0, 0, &gps_context.mgr) < 0)
            {
                g_printerr("Error opening GPS device: (%d) %s\n",
                        errno, strerror(errno));
                g_idle_add((GSourceFunc)gps_handle_error_idle,
                        g_strdup_printf("%s",
                        _("Error opening GPS device.")));
                error = TRUE;
            }
            else
            {
                /* Success.  Set gpsd_host and gpsd_port. */
                gpsd_host = "127.0.0.1";
                gpsd_port = GPSD_PORT_DEFAULT;
                is_context = TRUE;
            }
            break;
        }
        default:
            error = TRUE;
    }

    if(!error && my_thread == _gps_thread)
    {
        gint tryno;

        /* Attempt to connect to GPSD. */
        for(tryno = 0; tryno < 10; tryno++)
        {
            /* Create a socket to interact with GPSD. */
            GTimeVal timeout = { 10, 0 };

            if(GNOME_VFS_OK != (vfs_result = gnome_vfs_inet_connection_create(
                            &iconn,
                            gri->type == GPS_RCVR_GPSD ? gri->gpsd_host
                                                       : "127.0.0.1",
                            gri->type == GPS_RCVR_GPSD ? gri->gpsd_port
                                                       : GPSD_PORT_DEFAULT,
                            NULL))
               || NULL == (socket = gnome_vfs_inet_connection_to_socket(iconn))
               || GNOME_VFS_OK != (vfs_result = gnome_vfs_socket_set_timeout(
                       socket, &timeout, NULL))
               || GNOME_VFS_OK != (vfs_result = gnome_vfs_socket_write(socket,
                       "r+\r\n", sizeof("r+\r\n"), &bytes_read, NULL))
               || bytes_read != sizeof("r+\r\n"))
            {
                sleep(1);
            }
            else
                break;
        }

        if(!iconn)
        {
            g_printerr("Error connecting to GPSD server: (%d) %s\n",
                    vfs_result, gnome_vfs_result_to_string(vfs_result));
            g_idle_add((GSourceFunc)gps_handle_error_idle,
                    g_strdup_printf("%s",
                    _("Error connecting to GPSD server.")));
            error = TRUE;
        }
    }

    if(!error && my_thread == _gps_thread)
    {
        while(my_thread == _gps_thread)
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
                if(my_thread == _gps_thread)
                {
                    /* Error wasn't user-initiated. */
                    g_idle_add((GSourceFunc)gps_handle_error_idle,
                            g_strdup_printf("%s",
                                _("Error reading GPS data.")));
                    error = TRUE;
                }
                break;
            }

            /* Loop through the buffer and read each NMEA sentence. */
            buf_curr += bytes_read;
            *buf_curr = '\0'; /* append a \0 so we can read as string */
            while(my_thread == _gps_thread && (eol = strchr(buf, '\n')))
            {
                gint csum = 0;
                if(*buf == '$')
                {
                    gchar *sptr = buf + 1; /* Skip the $ */
                    /* This is the beginning of a sentence; okay to parse. */
                    *eol = '\0'; /* overwrite \n with \0 */
                    while(*sptr && *sptr != '*')
                        csum ^= *sptr++;

                    /* If we're at a \0 (meaning there is no checksum), or if
                     * the checksum is good, then parse the sentence. */
                    if(!*sptr || csum == strtol(sptr + 1, NULL, 16))
                    {
                        if(*sptr)
                            *sptr = '\0'; /* take checksum out of the buffer.*/
                        if(my_thread == _gps_thread)
                            g_idle_add((GSourceFunc)gps_parse_nmea_idle,
                                    g_strdup(buf));
                    }
                    else
                    {
                        /* There was a checksum, and it was bad. */
                        g_printerr("%s: Bad checksum in NMEA sentence:\n%s\n",
                                __PRETTY_FUNCTION__, buf);
                    }
                }

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
        }
    }

    /* Error, or we're done reading GPS. */

    /* Clean up. */
    if(iconn)
        gnome_vfs_inet_connection_free(iconn, NULL);

    if(is_context)
    {
        switch(gri->type)
        {
            case GPS_RCVR_BT:
                gpsbt_stop(&gps_context);
                break;

            case GPS_RCVR_FILE:
                gpsmgr_stop(&gps_context.mgr);
                break;

            default:
                ;
        }
    }

    gri_free(gri);

    printf("%s(): return\n", __PRETTY_FUNCTION__);
    return;
}

/**
 * Set the connection state.  This function controls all connection-related
 * banners.
 */
void
set_conn_state(ConnState new_conn_state)
{
    printf("%s(%d)\n", __PRETTY_FUNCTION__, new_conn_state);

    switch(_gps_state = new_conn_state)
    {
        case RCVR_OFF:
        case RCVR_FIXED:
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
            break;
        case RCVR_DOWN:
            if(_fix_banner)
            {
                gtk_widget_destroy(_fix_banner);
                _fix_banner = NULL;
            }
            if(!_connect_banner)
                _connect_banner = hildon_banner_show_animation(
                        _window, NULL, _("Searching for GPS receiver"));
            break;
        case RCVR_UP:
            if(_connect_banner)
            {
                gtk_widget_destroy(_connect_banner);
                _connect_banner = NULL;
            }
            if(!_fix_banner)
            {
                _fix_banner = hildon_banner_show_progress(
                        _window, NULL, _("Establishing GPS fix"));
            }
            break;
        default: ; /* to quell warning. */
    }
    map_force_redraw();

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

/**
 * Disconnect from the receiver.  This method cleans up any and everything
 * that might be associated with the receiver.
 */
void
rcvr_disconnect()
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    _gps_thread = NULL;

    if(_window)
        set_conn_state(RCVR_OFF);
    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

/**
 * Connect to the receiver.
 * This method assumes that _fd is -1 and _channel is NULL.  If unsure, call
 * rcvr_disconnect() first.
 * Since this is an idle function, this function returns whether or not it
 * should be called again, which is always FALSE.
 */
gboolean
rcvr_connect()
{
    printf("%s(%d)\n", __PRETTY_FUNCTION__, _gps_state);

    if(_enable_gps && _gps_state == RCVR_OFF)
    {
        set_conn_state(RCVR_DOWN);

        /* Lock/Unlock the mutex to ensure that the thread doesn't
         * start until _gps_thread is set. */
        g_mutex_lock(_gps_init_mutex);
        _gps_thread = g_thread_create((GThreadFunc)thread_read_nmea,
                gri_clone(&_gri), TRUE, NULL); /* Joinable. */
        g_mutex_unlock(_gps_init_mutex);
    }

    vprintf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
    return FALSE;
}

void
reset_bluetooth()
{
    printf("%s()\n", __PRETTY_FUNCTION__);
    if(system("/usr/bin/sudo -l | grep -q '/usr/sbin/hciconfig  *hci0  *reset'"
            " && sudo /usr/sbin/hciconfig hci0 reset"))
        popup_error(_window,
                _("An error occurred while trying to reset the bluetooth "
                "radio.\n\n"
                "Did you make sure to modify\nthe /etc/sudoers file?"));
    else if(_gps_state != RCVR_OFF)
    {
        rcvr_connect();
    }
    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

void
gps_init()
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    _gps_init_mutex = g_mutex_new();

    /* Fix a stupid PATH bug in libgps. */
    {
        gchar *path_env = getenv("PATH");
        gchar *new_path = g_strdup_printf("%s:%s", path_env, "/usr/sbin");
        setenv("PATH", new_path, 1);
    }

    /* set _gpsoffset */
    {   
        time_t time1;
        struct tm time2;
        time1 = time(NULL);
        localtime_r(&time1, &time2);
        _gmtoffset = time2.tm_gmtoff;
    }

    vprintf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
}

void
gps_destroy(gboolean last)
{
    static GThread* tmp = NULL;
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(!last)
    {
        if(_gps_thread)
        {
            tmp = (GThread*)_gps_thread;
            _gps_thread = NULL;
        }
    }
    else if(tmp)
        g_thread_join(tmp);

    vprintf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
}

