#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

#include "gpx-full.h"
#include "types.h"
#include "defines.h"

/* internal state */
static FILE* gpx_file = NULL;

/* This counter increased every data portion got. When it becomes divisible by 10 (usually every ten
   seconds) we write GPX finalization tags and flush data. On next data portion these tags will be
   overwriten by it's data. So, we have complete file to protect from maemo mapper crash. */
static int save_counter = 0;
static int flush_performed = 0;


static inline gboolean append_string (const char* str)
{
    int len;

    if (!gpx_file)
        return FALSE;

    if (!str)
        return TRUE;

    len = strlen (str);
    return fwrite (str, 1, len, gpx_file) == len;
}


static inline gboolean append_double (const char* format, gdouble val)
{
    static char buf[80];

    if (!gpx_file)
        return FALSE;

    g_ascii_formatd (buf, sizeof (buf), format, val);
    append_string (buf);
}


static void do_flush (gboolean seek)
{
    long pos;

    if (!gpx_file)
        return;

    if (seek)
        pos = ftell (gpx_file);
    append_string ("    </trkseg>\n"
                   "  </trk>\n"
                   "</gpx>\n");
    fflush (gpx_file);
    if (seek)
        fseek (gpx_file, pos, SEEK_SET);
    flush_performed = 1;
}


/* Initialize file with GPX data */
gboolean gpx_full_initialize (gboolean enabled, gchar* path)
{
    gchar* fname_buf;
    struct tm* tm;
    time_t t;
    int counter = 0;
    struct stat st;

    if (!enabled)
        return TRUE;

    if (!path)
        return FALSE;

    t = time (NULL);
    tm = localtime (&t);

    fname_buf = g_malloc (strlen (path) + 50);

    while (1) {
        sprintf (fname_buf, "%s/mapper_%04d%02d%02d_%04d.gpx", path, tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday, counter++);
        if (stat (fname_buf, &st) < 0)
            break;
    }

    /* file not exists, create one */
    gpx_file = fopen (fname_buf, "w+");

    if (!gpx_file)
        goto err;

    /* write file's header */
    append_string ("<?xml version=\"1.0\"?>\n"
                   "<gpx version=\"1.0\" creator=\"maemo-mapper\" "
                   "xmlns=\"http://www.topografix.com/GPX/1/0\">\n"
                   "  <trk>\n"
                   "    <trkseg>\n");
    return TRUE;
err:
    if (fname_buf)
        free (fname_buf);
    if (gpx_file)
        fclose (gpx_file);
    return FALSE;
}


/* Append GPX data portion to file */
gboolean gpx_full_append (GpsData* data, Point* pos)
{
    static char buf[80];

    flush_performed = 0;
    append_string ("      <trkpt lat=\"");
    append_double ("%.06f", data->lat);
    append_string ("\" lon=\"");
    append_double ("%.06f", data->lon);
    append_string ("\">\n");
    
    if (pos->altitude) {
        append_string ("        <ele>");
        append_double ("%.2f", pos->altitude);
        append_string ("</ele>\n");
    }

    if (pos->time) {
        append_string ("        <time>");
        strftime (buf, 80, XML_DATE_OUT_FORMAT, gmtime (&pos->time));
        append_string (buf);
        append_string ("</time>\n");
    }

    if (data->fix > 0 && data->fix <= 3) {
        switch (data->fix) {
        case 1:
            append_string ("        <fix>none</fix>\n");
            break;
        case 2:
            append_string ("        <fix>2d</fix>\n");
            break;
        case 3:
            append_string ("        <fix>3d</fix>\n");
            break;
        }
    }

    sprintf (buf, "        <sat>%d</sat>\n", data->satinuse);
    append_string (buf);

    append_string ("        <hdop>");
    append_double ("%.2f", data->hdop);
    append_string ("</hdop>\n");

    append_string ("        <vdop>");
    append_double ("%.2f", data->vdop);
    append_string ("</vdop>\n");

    append_string ("        <pdop>");
    append_double ("%.2f", data->pdop);
    append_string ("</pdop>\n");

    append_string("      </trkpt>\n");

    if ((++save_counter % 10) == 0) {
        do_flush (TRUE);
        save_counter = 0;
    }
}


/* Finalize file */
gboolean gpx_full_finalize ()
{
    if (!gpx_file)
        return TRUE;

    if (!flush_performed)
        do_flush (FALSE);
    fclose (gpx_file);
    gpx_file = NULL;
    flush_performed = save_counter = 0;
    return TRUE;
}

