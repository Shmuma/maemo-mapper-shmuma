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

#include <ctype.h>
#include <string.h>
#include <math.h>
#include <hildon-widgets/hildon-note.h>

#include "types.h"
#include "data.h"
#include "defines.h"

#include "gpx.h"
#include "util.h"


/**
 * Pop up a modal dialog box with simple error information in it.
 */
void
popup_error(GtkWidget *window, const gchar *error)
{
    GtkWidget *dialog;
    printf("%s(\"%s\")\n", __PRETTY_FUNCTION__, error);

    dialog = hildon_note_new_information(GTK_WINDOW(window), error);

    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

void
deg_format(gfloat coor, gchar *scoor, gchar neg_char, gchar pos_char)
{
    gfloat min;
    gfloat acoor = fabs(coor);
    printf("%s()\n", __PRETTY_FUNCTION__);

    switch(_degformat)
    {
        case DDPDDDDD:
            sprintf(scoor, "%.5f°", coor);
            break;
        case DDPDDDDD_NSEW:
            sprintf(scoor, "%.5f° %c", acoor,
                    coor < 0.f ? neg_char : pos_char);
            break;
        case NSEW_DDPDDDDD:
            sprintf(scoor, "%c %.5f°",
                    coor < 0.f ? neg_char : pos_char,
                    acoor);
            break;

        case DD_MMPMMM:
            sprintf(scoor, "%d°%06.3f'",
                    (int)coor, (acoor - (int)acoor)*60.0);
            break;
        case DD_MMPMMM_NSEW:
            sprintf(scoor, "%d°%06.3f' %c",
                    (int)acoor, (acoor - (int)acoor)*60.0,
                    coor < 0.f ? neg_char : pos_char);
            break;
        case NSEW_DD_MMPMMM:
            sprintf(scoor, "%c %d° %06.3f'",
                    coor < 0.f ? neg_char : pos_char,
                    (int)acoor, (acoor - (int)acoor)*60.0);
            break;

        case DD_MM_SSPS:
            min = (acoor - (int)acoor)*60.0;
            sprintf(scoor, "%d°%02d'%04.1f\"", (int)coor, (int)min,
                    ((min - (int)min)*60.0));
            break;
        case DD_MM_SSPS_NSEW:
            min = (acoor - (int)acoor)*60.0;
            sprintf(scoor, "%d°%02d'%04.1f\" %c", (int)acoor, (int)min,
                    ((min - (int)min)*60.0),
                    coor < 0.f ? neg_char : pos_char);
            break;
        case NSEW_DD_MM_SSPS:
            min = (acoor - (int)acoor)*60.0;
            sprintf(scoor, "%c %d° %02d' %04.1f\"",
                    coor < 0.f ? neg_char : pos_char,
                    (int)acoor, (int)min,
                    ((min - (int)min)*60.0));
            break;
    }
    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

/** Return the location (in units) of the given string address.  This function
  * makes a call to the internet, so it may take some time. */
Point locate_address(GtkWidget *parent, const gchar *addr)
{
    Path temp;
    Point retval = _point_null;
    GnomeVFSResult vfs_result;
    gint size;
    gchar *bytes = NULL;
    gchar *addr_escaped;
    gchar *buffer;
    printf("%s(%s)\n", __PRETTY_FUNCTION__, addr);

    addr_escaped = gnome_vfs_escape_string(addr);
    buffer = g_strdup_printf(_route_dl_url, addr_escaped, addr_escaped);
    g_free(addr_escaped);

    /* Attempt to download the route from the server. */
    if(GNOME_VFS_OK != (vfs_result = gnome_vfs_read_entire_file(
                buffer, &size, &bytes)))
    {
        g_free(buffer);
        g_free(bytes);
        popup_error(parent, _("Failed to connect to GPX Directions server"));
        return _point_null;
    }

    g_free(buffer);

    MACRO_PATH_INIT(temp);

    if(strncmp(bytes, "<?xml", strlen("<?xml")))
    {
        /* Not an XML document - must be bad locations. */
        popup_error(parent, _("Invalid address."));
    }
    /* Else, if GPS is enabled, replace the route, otherwise append it. */
    else if(!gpx_path_parse(&temp, bytes, size, 0) || !temp.head[1].unity)
    {
        popup_error(parent, _("Unknown error while locating address."));
    }
    else
    {
        /* Save Destination in Route Locations list. */
        GtkTreeIter iter;
        if(!g_slist_find_custom(_loc_list, addr, (GCompareFunc)strcmp))
        {
            _loc_list = g_slist_prepend(_loc_list, g_strdup(addr));
            gtk_list_store_insert_with_values(_loc_model, &iter,
                    INT_MAX, 0, addr, -1);
        }

        retval = temp.head[1];
    }

    MACRO_PATH_FREE(temp);
    g_free(bytes);

    vprintf("%s(): return (%d, %d)\n", __PRETTY_FUNCTION__,
            retval.unitx, retval.unity);
    return retval;
}

/**
 * Calculate the distance between two lat/lon pairs.  The distance is returned
 * in kilometers and should be converted using UNITS_CONVERT[_units].
 */
gfloat
calculate_distance(gfloat lat1, gfloat lon1, gfloat lat2, gfloat lon2)
{
    gfloat dlat, dlon, slat, slon, a;

    /* Convert to radians. */
    lat1 = deg2rad(lat1);
    lon1 = deg2rad(lon1);
    lat2 = deg2rad(lat2);
    lon2 = deg2rad(lon2);

    dlat = lat2 - lat1;
    dlon = lon2 - lon1;

    slat = sinf(dlat / 2.f);
    slon = sinf(dlon / 2.f);
    a = (slat * slat) + (cosf(lat1) * cosf(lat2) * slon * slon);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return ((2.f * atan2f(sqrtf(a), sqrtf(1.f - a))) * EARTH_RADIUS);
}

/**
 * Calculate the bearing between two lat/lon pairs.  The bearing is returned
 * as the angle from lat1/lon1 to lat2/lon2.
 */
gfloat
calculate_bearing(gfloat lat1, gfloat lon1, gfloat lat2, gfloat lon2)
{
    gfloat x, y;
    gfloat dlon = deg2rad(lon2 - lon1);
    lat1 = deg2rad(lat1);
    lat2 = deg2rad(lat2);

    y = sinf(dlon) * cosf(lat2);
    x = (cosf(lat1) * sinf(lat2)) - (sinf(lat1) * cosf(lat2) * cosf(dlon));

    dlon = rad2deg(atan2f(y, x));
    if(dlon < 0.f)
        dlon += 360.f;
    return dlon;
}



void
force_min_visible_bars(HildonControlbar *control_bar, gint num_bars)
{
    GValue val;
    printf("%s()\n", __PRETTY_FUNCTION__);
    memset(&val, 0, sizeof(val));
    g_value_init(&val, G_TYPE_INT);
    g_value_set_int(&val, num_bars);
    g_object_set_property(G_OBJECT(control_bar), "minimum-visible-bars", &val);
    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

gboolean
banner_reset()
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    /* Re-enable any banners that might have been up. */
    {
        if(_connect_banner)
        {
            gtk_widget_hide(_connect_banner);
            gtk_widget_unrealize(_connect_banner);
            gtk_widget_realize(_connect_banner);
            gtk_widget_show(_connect_banner);
        }

        if(_fix_banner)
        {
            gtk_widget_hide(_fix_banner);
            gtk_widget_unrealize(_fix_banner);
            gtk_widget_realize(_fix_banner);
            gtk_widget_show(_fix_banner);
        }

        if(_waypoint_banner)
        {
            gtk_widget_hide(_waypoint_banner);
            gtk_widget_unrealize(_waypoint_banner);
            gtk_widget_realize(_waypoint_banner);
            gtk_widget_show(_waypoint_banner);
        }

        if(_download_banner)
        {
            gtk_widget_hide(_download_banner);
            gtk_widget_unrealize(_download_banner);
            gtk_widget_realize(_download_banner);
            gtk_widget_show(_download_banner);
        }

        /*
        ConnState old_state = _gps_state;
        set_conn_state(RCVR_OFF);
        set_conn_state(old_state);
        */
    }

    vprintf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
    return FALSE;
}
/*
 * Get one numeric token off the string, fractional part separator
 * is ',' or '.' and number may follow any token from sep.
 * Return value found is in d.
 *
 * If utf8_deg is set then accept also Unicode degree symbol U+00B0
 * encoded as UTF-8 octets 0xc2 0xb0.
 *
 * @returns
 *    0 : on error.
 *    1 : when nothing or just white space was seen.
 *    2 : when fractional number was seen.
 *    3 : when whole number was seen.
 *  In case 0, endptr points to the beginning of the input string nptr,
 *  d is undefined.
 *
 *  In case 1 endptr points past the whitespace, d is undefined.
 *
 *  In cases 2 and 3 the found number is stored in d and endptr points
 *  to first char not part of the number.
 *
 */
static gint
strdmstod_1(gdouble *d, gchar *nptr, gchar **endptr, gchar *sep, gint utf8_deg)
{
    guchar *p;
    gint v_flag, f_flag;
    gdouble v;

    p = nptr;

    while (*p && isspace(*p)) {
        p++;
    }

    v_flag = 0;
    f_flag = 0;

    /* whole part */
    v = 0.0;
    while (*p && isdigit(*p)) {
        v *= 10;
        v += *p++ - '0';
        v_flag = 1;
    }

    if (v_flag) {
        if (*p && (*p == '.' || *p == ',')) {
            gdouble f;
            gint n;

            p++;

            /* fractional part */
            f = 0.0;
            n = 1;
            while (*p && isdigit(*p)) {
                f *= 10.0;
                f += *p++ - '0';
                n *= 10;
            }

            if (n > 1) {
                f_flag = 1;
                v += f/n;
            }
        }

        /* allow for extra sep char at the end */
        if (*p && strchr(sep, *p)) {
            p++;
        } else if (utf8_deg && *p == 0xc2 && *(p+1) == 0xb0) {
            p += 2;
        }

        *d = v;
        if (endptr) *endptr = p;
        return f_flag ? 2 : 3;
    }

    if (endptr) *endptr = p;
    return *p == 0;
}

static gdouble
strdmstod_2(gchar *nptr, gchar **endptr)
{
    gint ret;
    guchar *p;
    gdouble d, m, s;

    p = nptr;

    /* degrees */
    ret = strdmstod_1(&d, p, endptr, "dD@", 1);
    switch (ret) {
        case 0: return 0.0;
        case 1:
                if (endptr) *endptr = (char *)nptr;
                return 0.0;
        case 2: return d;
    }

    /* minutes */
    p = *endptr;
    m = 0.0;
    ret = strdmstod_1(&m, p, endptr, "mM'", 0);
    switch (ret) {
        case 0: return 0.0;
        case 1: return d;
        case 2: return d + m/60.0;
    }

    /* seconds */
    p = *endptr;
    ret = strdmstod_1(&s, p, endptr, "sS\"", 0);
    switch (ret) {
        case 0: return 0.0;
        case 1: return d + m/60.0;
        case 2:
        case 3: return d + m/60.0 + s/3600.0;
    }

    /* can't be here */
    return 0.0;
}

/*
 * format: / \s* [+-NSWE]? \s* ( d | D \s+ m | D \s+ M \s+ s ) [NSWE]? /x
 * where D := / \d+[@d°]? /ix
 *       M := / \d+['m]? /ix
 *       d := / D | \d+[,.]\d+[@d°]? /ix
 *       m := / M | \d+[,.]\d+['m]? /ix
 *       s := / S | \d+[,.]\d+["s]? /ix
 *
 *   and N or W are treated as positive, S or E are treated as negative,
 *   they may occur only once.
 */
gdouble
strdmstod(const gchar *nptr, gchar **endptr)
{
    gint s;
    gchar *p, *end;
    gchar *sign = 0;
    gdouble d;

    p = (char *)nptr;

    while(*p && isspace(*p))
        p++;

    if(!*p) {
        if(endptr)
            *endptr = (char *)nptr;
        return 0.0;
    }

    if(strchr("nwseNWSE-+", *p)) {
        sign = p;
        p++;
    }

    d = strdmstod_2(p, &end);
    if(p == end && d == 0.0) {
        if(endptr) *endptr = end;
        return d;
    }

    p = end;
    while(*p && isspace(*p))
        p++;

    s = 1;
    if(sign == 0) {
        if(*p && strchr("nwseNWSE", *p)) {
            if(tolower(*p) == 's' || tolower(*p) == 'w')
                s = -1;
            p++;
        }
    } else {
        if(tolower(*sign) == 's' || tolower(*sign) == 'w' || *sign == '-')
            s = -1;
        printf("s: %d\n", s);
    }

    if(endptr) *endptr = p;
    return s * d;
}

#if 0
struct t_case {
    gchar *fmt;
    gdouble value;
} t_cases[] = {
    { "12°", 12 },
    { "+12d", 12 },
    { "-12.345d", -12.345 },
    { "12.345 E", -12.345 },
    { "12d34m", 12.5666667 },
    { "N 12 34", 12.5666667 },
    { "S 12d34.56m", -12.576 },
    { "W 12 34.56", 12.576 },
    { "E 12d34m56s", -12.582222 },
    { "12 34 56 S", -12.582222 },
    { "12 34 56", 12.582222 },
    { "12d34m56.7s E", -12.582417 },
    { "12 34 56.7 W", 12.582417 },
    { "12° 34 56.7 W", 12.582417 },

    { 0, 0 }
};

int
strdmstod_test()
{
    struct t_case *t;
    gint fail, ok;

    fail = ok = 0;

    for (t = t_cases; t->fmt; t++) {
        gdouble v;
        gchar *endp;

        v = strdmstod(t->fmt, &endp);
        if (endp == t->fmt || *endp != 0) {
            fprintf(stderr, "FAIL syntax %s\n", t->fmt);
            fail++;
        } else if (fabs(v - t->value) > 0.000001) {
            fprintf(stderr, "FAIL value %s -> %.10g (%.10g)\n",
                    t->fmt, v, t->value);
            fail++;
        } else {
            ok++;
        }
    }

    if (fail == 0) {
        fprintf(stderr, "ALL TESTS OK\n");
    } else {
        fprintf(stderr, "FAIL %d, OK %d\n", fail, ok);
    }
}
#endif
