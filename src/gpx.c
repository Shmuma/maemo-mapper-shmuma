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

#include <string.h>
#include <math.h>
#include <libxml/parser.h>

#include "types.h"
#include "data.h"
#include "defines.h"

#include "gpx.h"
#include "path.h"
#include "util.h"

/**
 * Handle a start tag in the parsing of a GPX file.
 */
#define MACRO_SET_UNKNOWN() { \
    data->sax_data.prev_state = data->sax_data.state; \
    data->sax_data.state = UNKNOWN; \
    data->sax_data.unknown_depth = 1; \
}

static gchar XML_TZONE[7];

/**
 * Handle char data in the parsing of a GPX file.
 */
static void
gpx_chars(SaxData *data, const xmlChar *ch, int len)
{
    gint i;
    vprintf("%s()\n", __PRETTY_FUNCTION__);

    switch(data->state)
    {
        case ERROR:
        case UNKNOWN:
            break;
        case INSIDE_WPT_NAME:
        case INSIDE_WPT_DESC:
        case INSIDE_PATH_POINT_ELE:
        case INSIDE_PATH_POINT_TIME:
        case INSIDE_PATH_POINT_DESC:
            for(i = 0; i < len; i++)
                data->chars = g_string_append_c(data->chars, ch[i]);
            vprintf("%s\n", data->chars->str);
            break;
        default:
            break;
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

/**
 * Handle an entity in the parsing of a GPX file.  We don't do anything
 * special here.
 */
static xmlEntityPtr
gpx_get_entity(SaxData *data, const xmlChar *name)
{
    vprintf("%s()\n", __PRETTY_FUNCTION__);
    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return xmlGetPredefinedEntity(name);
}

/**
 * Handle an error in the parsing of a GPX file.
 */
static void
gpx_error(SaxData *data, const gchar *msg, ...)
{
    vprintf("%s()\n", __PRETTY_FUNCTION__);
    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    data->state = ERROR;
}

static gboolean
gpx_write_string(GnomeVFSHandle *handle, const gchar *str)
{
    GnomeVFSResult vfs_result;
    GnomeVFSFileSize size;
    if(GNOME_VFS_OK != (vfs_result = gnome_vfs_write(
                    handle, str, strlen(str), &size)))
    {
        gchar buffer[BUFFER_SIZE];
        snprintf(buffer, sizeof(buffer),
                "%s:\n%s\n%s", _("Error while writing to file"),
                _("File is incomplete."),
                gnome_vfs_result_to_string(vfs_result));
        popup_error(_window, buffer);
        return FALSE;
    }
    return TRUE;
}

static gboolean
gpx_write_escaped(GnomeVFSHandle *handle, const gchar *str)
{
    const gchar *ptr = str;
    const gchar *nullchr = ptr + strlen(ptr);
    while(ptr < nullchr)
    {
        gchar *newptr = strpbrk(ptr, "&<>");
        if(newptr != NULL)
        {
            /* First, write out what we have so far. */
            const gchar *to_write;
            GnomeVFSResult vfs_result;
            GnomeVFSFileSize size;
            if(GNOME_VFS_OK != (vfs_result = gnome_vfs_write(
                            handle, ptr, newptr - ptr, &size)))
            {
                gchar buffer[BUFFER_SIZE];
                snprintf(buffer, sizeof(buffer),
                        "%s:\n%s\n%s", _("Error while writing to file"),
                        _("File is incomplete."),
                        gnome_vfs_result_to_string(vfs_result));
                popup_error(_window, buffer);
                return FALSE;
            }

            /* Now, write the XML entity. */
            switch(*newptr)
            {
                case '&':
                    to_write = "&amp;";
                    break;
                case '<':
                    to_write = "&lt;";
                    break;
                case '>':
                    to_write = "&gt;";
                    break;
            }
            gpx_write_string(handle, to_write);

            /* Advance the pointer to continue searching for entities. */
            ptr = newptr + 1;
        }
        else
        {
            /* No characters need escaping - write the whole thing. */
            gpx_write_string(handle, ptr);
            ptr = nullchr;
        }
    }
    return TRUE;
}

/****************************************************************************
 * BELOW: OPEN PATH *********************************************************
 ****************************************************************************/

static void
gpx_path_start_element(PathSaxData *data,
        const xmlChar *name, const xmlChar **attrs)
{
    vprintf("%s(%s)\n", __PRETTY_FUNCTION__, name);

    switch(data->sax_data.state)
    {
        case ERROR:
            printf("ERROR!\n");
            break;
        case START:
            if(!strcmp((gchar*)name, "gpx"))
                data->sax_data.state = INSIDE_GPX;
            else
                MACRO_SET_UNKNOWN();
            break;
        case INSIDE_GPX:
            if(!strcmp((gchar*)name, "trk"))
                data->sax_data.state = INSIDE_PATH;
            else
                MACRO_SET_UNKNOWN();
            break;
        case INSIDE_PATH:
            if(!strcmp((gchar*)name, "trkseg"))
            {
                data->sax_data.state = INSIDE_PATH_SEGMENT;
                data->sax_data.at_least_one_trkpt = FALSE;
            }
            else
                MACRO_SET_UNKNOWN();
            break;
        case INSIDE_PATH_SEGMENT:
            if(!strcmp((gchar*)name, "trkpt"))
            {
                const xmlChar **curr_attr;
                gchar *error_check;
                gdouble lat = 0.0, lon = 0.0;
                gboolean has_lat, has_lon;
                has_lat = FALSE;
                has_lon = FALSE;
                for(curr_attr = attrs; *curr_attr != NULL; )
                {
                    const gchar *attr_name = *curr_attr++;
                    const gchar *attr_val = *curr_attr++;
                    if(!strcmp(attr_name, "lat"))
                    {
                        lat = g_ascii_strtod(attr_val, &error_check);
                        if(error_check != attr_val)
                            has_lat = TRUE;
                    }
                    else if(!strcmp(attr_name, "lon"))
                    {
                        lon = g_ascii_strtod(attr_val, &error_check);
                        if(error_check != attr_val)
                            has_lon = TRUE;
                    }
                }
                if(has_lat && has_lon)
                {
                    MACRO_PATH_INCREMENT_TAIL(data->path);
                    latlon2unit(lat, lon,
                            data->path.tail->unitx,
                            data->path.tail->unity);
                    data->path.tail->time = 0;
                    data->path.tail->altitude = 0;
                    data->sax_data.state = INSIDE_PATH_POINT;
                }
                else
                    data->sax_data.state = ERROR;
            }
            else
                MACRO_SET_UNKNOWN();
            break;
        case INSIDE_PATH_POINT:
            if(!strcmp((gchar*)name, "time"))
                data->sax_data.state = INSIDE_PATH_POINT_TIME;
            else if(!strcmp((gchar*)name, "ele"))
                data->sax_data.state = INSIDE_PATH_POINT_ELE;
            else if(!strcmp((gchar*)name, "desc"))
                data->sax_data.state = INSIDE_PATH_POINT_DESC;

            else
                MACRO_SET_UNKNOWN();
            break;
        case UNKNOWN:
            data->sax_data.unknown_depth++;
            break;
        default:
            ;
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

/**
 * Handle an end tag in the parsing of a GPX file.
 */
static void
gpx_path_end_element(PathSaxData *data, const xmlChar *name)
{
    vprintf("%s(%s)\n", __PRETTY_FUNCTION__, name);

    switch(data->sax_data.state)
    {
        case ERROR:
            printf("ERROR!\n");
            break;
        case START:
            data->sax_data.state = ERROR;
            break;
        case INSIDE_GPX:
            if(!strcmp((gchar*)name, "gpx"))
                data->sax_data.state = FINISH;
            else
                data->sax_data.state = ERROR;
            break;
        case INSIDE_PATH:
            if(!strcmp((gchar*)name, "trk"))
                data->sax_data.state = INSIDE_GPX;
            else
                data->sax_data.state = ERROR;
            break;
        case INSIDE_PATH_SEGMENT:
            if(!strcmp((gchar*)name, "trkseg"))
            {
                if(data->sax_data.at_least_one_trkpt)
                {
                    MACRO_PATH_INCREMENT_TAIL(data->path);
                    *data->path.tail = _point_null;
                }
                data->sax_data.state = INSIDE_PATH;
            }
            else
                data->sax_data.state = ERROR;
            break;
        case INSIDE_PATH_POINT:
            if(!strcmp((gchar*)name, "trkpt"))
            {
                data->sax_data.state = INSIDE_PATH_SEGMENT;
                data->sax_data.at_least_one_trkpt = TRUE;
            }
            else
                data->sax_data.state = ERROR;
            break;
        case INSIDE_PATH_POINT_ELE:
            if(!strcmp((gchar*)name, "ele"))
            {
                gchar *error_check;
                data->path.tail->altitude
                    = g_ascii_strtod(data->sax_data.chars->str, &error_check);
                if(error_check == data->sax_data.chars->str)
                    data->path.tail->altitude = 0;
                data->sax_data.state = INSIDE_PATH_POINT;
                g_string_free(data->sax_data.chars, TRUE);
                data->sax_data.chars = g_string_new("");
            }
            else
                data->sax_data.state = ERROR;
            break;
        case INSIDE_PATH_POINT_TIME:
            if(!strcmp((gchar*)name, "time"))
            {
                struct tm time;
                gchar *ptr;

                if(NULL == (ptr = strptime(data->sax_data.chars->str,
                            XML_DATE_FORMAT, &time)))
                    /* Failed to parse dateTime format. */
                    data->sax_data.state = ERROR;
                else
                {
                    /* Parse was successful. Now we have to parse timezone.
                     * From here on, if there is an error, I just assume local
                     * timezone.  Yes, this is not proper XML, but I don't
                     * care. */
                    gchar *error_check;

                    /* First, set time in "local" time zone. */
                    data->path.tail->time = (mktime(&time));

                    /* Now, skip inconsequential characters */
                    while(*ptr && *ptr != 'Z' && *ptr != '-' && *ptr != '+')
                        ptr++;

                    /* Check if we ran to the end of the string. */
                    if(*ptr)
                    {
                        /* Next character is either 'Z', '-', or '+' */
                        if(*ptr == 'Z')
                            /* Zulu (UTC) time. Undo the local time zone's
                             * offset. */
                            data->path.tail->time += time.tm_gmtoff;
                        else
                        {
                            /* Not Zulu (UTC). Must parse hours and minutes. */
                            gint offhours = strtol(ptr, &error_check, 10);
                            if(error_check != ptr
                                    && *(ptr = error_check) == ':')
                            {
                                /* Parse of hours worked. Check minutes. */
                                gint offmins = strtol(ptr + 1,
                                        &error_check, 10);
                                if(error_check != (ptr + 1))
                                {
                                    /* Parse of minutes worked. Calculate. */
                                    data->path.tail->time
                                        += (time.tm_gmtoff
                                                - (offhours * 60 * 60
                                                    + offmins * 60));
                                }
                            }
                        }
                    }
                    /* Successfully parsed dateTime. */
                    data->sax_data.state = INSIDE_PATH_POINT;
                }

                g_string_free(data->sax_data.chars, TRUE);
                data->sax_data.chars = g_string_new("");
            }
            else
                data->sax_data.state = ERROR;
            break;
        case INSIDE_PATH_POINT_DESC:
            /* only parse description for routes */
            if(!strcmp((gchar*)name, "desc"))
            {
                MACRO_PATH_INCREMENT_WTAIL(data->path);
                data->path.wtail->point = data->path.tail;
                data->path.wtail->desc
                    = g_string_free(data->sax_data.chars, FALSE);
                data->sax_data.chars = g_string_new("");
                data->sax_data.state = INSIDE_PATH_POINT;
            }
            else
                data->sax_data.state = ERROR;
            break;
        case UNKNOWN:
            if(!--data->sax_data.unknown_depth)
                data->sax_data.state = data->sax_data.prev_state;
            else
                data->sax_data.state = ERROR;
            break;
        default:
            ;
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

gboolean
gpx_path_parse(Path *to_replace, gchar *buffer, gint size, gint policy_old)
{
    PathSaxData data;
    xmlSAXHandler sax_handler;
    printf("%s()\n", __PRETTY_FUNCTION__);

    MACRO_PATH_INIT(data.path);
    data.sax_data.state = START;
    data.sax_data.chars = g_string_new("");

    memset(&sax_handler, 0, sizeof(sax_handler));
    sax_handler.characters = (charactersSAXFunc)gpx_chars;
    sax_handler.startElement = (startElementSAXFunc)gpx_path_start_element;
    sax_handler.endElement = (endElementSAXFunc)gpx_path_end_element;
    sax_handler.entityDecl = (entityDeclSAXFunc)gpx_get_entity;
    sax_handler.warning = (warningSAXFunc)gpx_error;
    sax_handler.error = (errorSAXFunc)gpx_error;
    sax_handler.fatalError = (fatalErrorSAXFunc)gpx_error;

    xmlSAXUserParseMemory(&sax_handler, &data, buffer, size);
    g_string_free(data.sax_data.chars, TRUE);

    if(data.sax_data.state != FINISH)
    {
        vprintf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
        return FALSE;
    }

    if(policy_old && to_replace->head != to_replace->tail)
    {
        Point *src_first;
        Path *src, *dest;

        if(policy_old > 0)
        {
            /* Append to current path. Make sure last path point is zero. */
            if(to_replace->tail->unity != 0)
            {
                MACRO_PATH_INCREMENT_TAIL((*to_replace));
                *to_replace->tail = _point_null;
            }
            src = &data.path;
            dest = to_replace;
        }
        else
        {
            /* Prepend to current route. */
            src = to_replace;
            dest = &data.path;
        }

        /* Find src_first non-zero point. */
        for(src_first = src->head - 1; src_first++ != src->tail; )
            if(src_first->unity)
                break;

        /* Append route points from src to dest. */
        if(src->tail >= src_first)
        {
            WayPoint *curr;
            gint num_dest_points = dest->tail - dest->head + 1;
            gint num_src_points = src->tail - src_first + 1;

            /* Adjust dest->tail to be able to fit src route data
             * plus room for more route data. */
            path_resize(dest,
                    num_dest_points + num_src_points + ARRAY_CHUNK_SIZE);

            memcpy(dest->tail + 1, src_first,
                    num_src_points * sizeof(Point));

            dest->tail += num_src_points;

            /* Append waypoints from src to dest->. */
            path_wresize(dest, (dest->wtail - dest->whead)
                    + (src->wtail - src->whead) + 2 + ARRAY_CHUNK_SIZE);
            for(curr = src->whead - 1; curr++ != src->wtail; )
            {
                (++(dest->wtail))->point = dest->head + num_dest_points
                    + (curr->point - src_first);
                dest->wtail->desc = curr->desc;
            }

        }

        /* Kill old route - don't use MACRO_PATH_FREE(), because that
         * would free the string desc's that we just moved to data.route. */
        g_free(src->head);
        g_free(src->whead);
        if(policy_old < 0)
            (*to_replace) = *dest;
    }
    else
    {
        MACRO_PATH_FREE((*to_replace));
        /* Overwrite with data.route. */
        (*to_replace) = data.path;
        path_resize(to_replace,
                to_replace->tail - to_replace->head + 1 + ARRAY_CHUNK_SIZE);
        path_wresize(to_replace,
                to_replace->wtail - to_replace->whead + 1 + ARRAY_CHUNK_SIZE);
    }

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

/****************************************************************************
 * ABOVE: OPEN PATH *********************************************************
 ****************************************************************************/

/****************************************************************************
 * BELOW: SAVE PATH *********************************************************
 ****************************************************************************/

gboolean
gpx_path_write(Path *path, GnomeVFSHandle *handle)
{
    Point *curr = NULL;
    WayPoint *wcurr = NULL;
    gboolean trkseg_break = FALSE;
    printf("%s()\n", __PRETTY_FUNCTION__);

    /* Find first non-zero point. */
    for(curr = path->head - 1, wcurr = path->whead; curr++ != path->tail; )
    {
        if(curr->unity)
            break;
        else if(wcurr <= path->wtail && curr == wcurr->point)
            wcurr++;
    }

    /* Write the header. */
    gpx_write_string(handle,
            "<?xml version=\"1.0\"?>\n"
            "<gpx version=\"1.0\" creator=\"maemo-mapper\" "
            "xmlns=\"http://www.topografix.com/GPX/1/0\">\n"
            "  <trk>\n"
            "    <trkseg>\n");

    /* Curr points to first non-zero point. */
    for(curr--; curr++ != path->tail; )
    {
        gdouble lat, lon;
        if(curr->unity)
        {
            gchar buffer[80];
            gboolean first_sub = TRUE;
            if(trkseg_break)
            {
                /* First trkpt of the segment - write trkseg header. */
                gpx_write_string(handle, "    </trkseg>\n"
                             "    <trkseg>\n");
                trkseg_break = FALSE;
            }
            unit2latlon(curr->unitx, curr->unity, lat, lon);
            gpx_write_string(handle, "      <trkpt lat=\"");
            g_ascii_formatd(buffer, sizeof(buffer), "%.06f", lat);
            gpx_write_string(handle, buffer);
            gpx_write_string(handle, "\" lon=\"");
            g_ascii_formatd(buffer, sizeof(buffer), "%.06f", lon);
            gpx_write_string(handle, buffer);
            gpx_write_string(handle, "\"");

            /* write the elevation */
            if(curr->altitude != 0)
            {
                if(first_sub)
                {
                    gpx_write_string(handle, ">\n");
                    first_sub = FALSE;
                }
                gpx_write_string(handle, "        <ele>");
                {
                    g_ascii_formatd(buffer, 80, "%.2f", curr->altitude);
                    gpx_write_string(handle, buffer);
                }
                gpx_write_string(handle, "</ele>\n");
            }

            /* write the time */
            if(curr->time)
            {
                if(first_sub)
                {
                    gpx_write_string(handle, ">\n");
                    first_sub = FALSE;
                }
                gpx_write_string(handle, "        <time>");
                strftime(buffer, 80, XML_DATE_FORMAT, localtime(&curr->time));
                gpx_write_string(handle, buffer);
                gpx_write_string(handle, XML_TZONE);
                gpx_write_string(handle, "</time>\n");
            }

            if(wcurr && curr == wcurr->point)
            {
                if(first_sub)
                {
                    gpx_write_string(handle, ">\n");
                    first_sub = FALSE;
                }
                gpx_write_string(handle, "        <desc>");
                gpx_write_escaped(handle, wcurr->desc);
                gpx_write_string(handle, "</desc>\n");
                wcurr++;
            }
            if(first_sub)
            {
                gpx_write_string(handle, "/>\n");
            }
            else
            {
                gpx_write_string(handle, "      </trkpt>\n");
            }
        }
        else
            trkseg_break = TRUE;
    }

    /* Write the footer. */
    gpx_write_string(handle,
            "    </trkseg>\n"
            "  </trk>\n"
            "</gpx>\n");


    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

/****************************************************************************
 * ABOVE: SAVE PATH *********************************************************
 ****************************************************************************/

/****************************************************************************
 * BELOW: OPEN POI **********************************************************
 ****************************************************************************/

static void
gpx_poi_start_element(PoiSaxData *data,
        const xmlChar *name, const xmlChar **attrs)
{
    vprintf("%s(%s)\n", __PRETTY_FUNCTION__, name);

    switch(data->sax_data.state)
    {
        case ERROR:
            printf("ERROR!\n");
            break;
        case START:
            if(!strcmp((gchar*)name, "gpx"))
                data->sax_data.state = INSIDE_GPX;
            else
                MACRO_SET_UNKNOWN();
            break;
        case INSIDE_GPX:
            if(!strcmp((gchar*)name, "wpt"))
            {
                const xmlChar **curr_attr;
                gchar *error_check;
                gdouble lat = 0.0, lon = 0.0;
                gboolean has_lat, has_lon;
                has_lat = FALSE;
                has_lon = FALSE;

                /* Parse the attributes - there should be lat and lon. */
                for(curr_attr = attrs; *curr_attr != NULL; )
                {
                    const gchar *attr_name = *curr_attr++;
                    const gchar *attr_val = *curr_attr++;
                    if(!strcmp(attr_name, "lat"))
                    {
                        lat = g_ascii_strtod(attr_val, &error_check);
                        if(error_check != attr_val)
                            has_lat = TRUE;
                    }
                    else if(!strcmp(attr_name, "lon"))
                    {
                        lon = g_ascii_strtod(attr_val, &error_check);
                        if(error_check != attr_val)
                            has_lon = TRUE;
                    }
                }
                if(has_lat && has_lon)
                {
                    data->sax_data.state = INSIDE_WPT;
                    data->curr_poi = g_slice_new0(PoiInfo);
                    data->curr_poi->lat = lat;
                    data->curr_poi->lon = lon;
                    data->poi_list = g_list_append(
                            data->poi_list, data->curr_poi);
                }
                else
                    data->sax_data.state = ERROR;
            }
            else
                MACRO_SET_UNKNOWN();
            break;
        case INSIDE_WPT:
            if(!strcmp((gchar*)name, "name"))
                data->sax_data.state = INSIDE_WPT_NAME;
            else if(!strcmp((gchar*)name, "desc"))
                data->sax_data.state = INSIDE_WPT_DESC;
            else
                MACRO_SET_UNKNOWN();
            break;
        case UNKNOWN:
            printf("UNKNOWN!\n");
            data->sax_data.unknown_depth++;
            break;
        default:
            ;
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

/**
 * Handle an end tag in the parsing of a GPX file.
 */
static void
gpx_poi_end_element(PoiSaxData *data, const xmlChar *name)
{
    vprintf("%s(%s)\n", __PRETTY_FUNCTION__, name);

    switch(data->sax_data.state)
    {
        case ERROR:
            printf("ERROR!\n");
            break;
        case START:
            data->sax_data.state = ERROR;
            break;
        case INSIDE_GPX:
            if(!strcmp((gchar*)name, "gpx"))
            {
                data->sax_data.state = FINISH;
            }
            else
                data->sax_data.state = ERROR;
            break;
        case INSIDE_WPT:
            if(!strcmp((gchar*)name, "wpt"))
                data->sax_data.state = INSIDE_GPX;
            else
                data->sax_data.state = ERROR;
            break;
        case INSIDE_WPT_NAME:
            if(!strcmp((gchar*)name, "name"))
            {
                data->curr_poi->label
                    = g_string_free(data->sax_data.chars, FALSE);
                data->sax_data.chars = g_string_new("");
                data->sax_data.state = INSIDE_WPT;
            }
            else
                data->sax_data.state = ERROR;
            break;
        case INSIDE_WPT_DESC:
            if(!strcmp((gchar*)name, "desc"))
            {
                data->curr_poi->desc
                    = g_string_free(data->sax_data.chars, FALSE);
                data->sax_data.chars = g_string_new("");
                data->sax_data.state = INSIDE_WPT;
            }
            else
                data->sax_data.state = ERROR;
            break;
        case UNKNOWN:
            printf("UNKNOWN!\n");
            if(!--data->sax_data.unknown_depth)
                data->sax_data.state = data->sax_data.prev_state;
            else
                data->sax_data.state = ERROR;
            break;
        default:
            ;
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

gboolean
gpx_poi_parse(gchar *buffer, gint size, GList **poi_list)
{
    PoiSaxData data;
    xmlSAXHandler sax_handler;
    printf("%s()\n", __PRETTY_FUNCTION__);

    data.poi_list = *poi_list;
    data.sax_data.state = START;
    data.sax_data.chars = g_string_new("");

    memset(&sax_handler, 0, sizeof(sax_handler));
    sax_handler.characters = (charactersSAXFunc)gpx_chars;
    sax_handler.startElement = (startElementSAXFunc)gpx_poi_start_element;
    sax_handler.endElement = (endElementSAXFunc)gpx_poi_end_element;
    sax_handler.entityDecl = (entityDeclSAXFunc)gpx_get_entity;
    sax_handler.warning = (warningSAXFunc)gpx_error;
    sax_handler.error = (errorSAXFunc)gpx_error;
    sax_handler.fatalError = (fatalErrorSAXFunc)gpx_error;

    xmlSAXUserParseMemory(&sax_handler, &data, buffer, size);
    g_string_free(data.sax_data.chars, TRUE);
    *poi_list = data.poi_list;

    if(data.sax_data.state != FINISH)
    {
        vprintf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
        return FALSE;
    }

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

/****************************************************************************
 * ABOVE: OPEN POI **********************************************************
 ****************************************************************************/

/****************************************************************************
 * BELOW: SAVE POI **********************************************************
 ****************************************************************************/

gint
gpx_poi_write(GtkTreeModel *model, GnomeVFSHandle *handle)
{
    gint num_written = 0;
    GtkTreeIter iter;
    printf("%s()\n", __PRETTY_FUNCTION__);

    /* Write the header. */
    gpx_write_string(handle,
            "<?xml version=\"1.0\"?>\n"
            "<gpx version=\"1.0\" creator=\"maemo-mapper\" "
            "xmlns=\"http://www.topografix.com/GPX/1/0\">\n");

    /* Iterate through the data model and import as desired. */
    if(gtk_tree_model_get_iter_first(model, &iter)) do
    {   
        PoiInfo poi;
        gboolean selected;
        memset(&poi, 0, sizeof(poi));

        gtk_tree_model_get(model, &iter,
                POI_SELECTED, &selected,
                POI_POIID, &(poi.poi_id),
                POI_CATID, &(poi.cat_id),
                POI_LAT, &(poi.lat),
                POI_LON, &(poi.lon),
                POI_LABEL, &(poi.label),
                POI_DESC, &(poi.desc),
                POI_CLABEL, &(poi.clabel),
                -1);

        if(selected)
        {
            gchar buffer[80];

            gpx_write_string(handle, "  <wpt lat=\"");
            g_ascii_formatd(buffer, sizeof(buffer), "%.06f", poi.lat);
            gpx_write_string(handle, buffer);
            gpx_write_string(handle, "\" lon=\"");
            g_ascii_formatd(buffer, sizeof(buffer), "%.06f", poi.lon);
            gpx_write_string(handle, buffer);
            gpx_write_string(handle, "\">\n");

            if(poi.label && *poi.label)
            {
                gpx_write_string(handle, "    <name>");
                gpx_write_escaped(handle, poi.label);
                gpx_write_string(handle, "</name>\n");
            }

            if(poi.desc && *poi.desc)
            {
                gpx_write_string(handle, "    <desc>");
                gpx_write_escaped(handle, poi.desc);
                gpx_write_string(handle, "</desc>\n");
            }
            gpx_write_string(handle, "  </wpt>\n");
            ++ num_written;
        }
    } while(gtk_tree_model_iter_next(model, &iter));

    /* Write the footer. */
    gpx_write_string(handle, "</gpx>\n");

    vprintf("%s(): return %d\n", __PRETTY_FUNCTION__, num_written);
    return num_written;
}

/****************************************************************************
 * ABOVE: SAVE POI **********************************************************
 ****************************************************************************/

void
gpx_init()
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    /* set XML_TZONE */
    {   
        time_t time1;
        struct tm time2;
        time1 = time(NULL);
        localtime_r(&time1, &time2);
        snprintf(XML_TZONE, sizeof(XML_TZONE), "%+03ld:%02ld",
                (time2.tm_gmtoff / 60 / 60), (time2.tm_gmtoff / 60) % 60);
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}
