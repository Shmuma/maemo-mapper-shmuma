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

#ifdef HAVE_CONFIG_H
#    include "config.h"
#endif

#define _GNU_SOURCE

#include <ctype.h>
#include <string.h>
#include <math.h>

#ifndef LEGACY
#    include <hildon/hildon-note.h>
#else
#    include <hildon-widgets/hildon-note.h>
#endif

#include "types.h"
#include "data.h"
#include "defines.h"

#include "gpx.h"
#include "util.h"

gboolean convert_iaru_loc_to_lat_lon(const gchar* txt_lon, gdouble* lat, gdouble* lon);

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
deg_format(gdouble coor, gchar *scoor, gchar neg_char, gchar pos_char)
{
    gdouble min;
    gdouble acoor = fabs(coor);
    printf("%s()\n", __PRETTY_FUNCTION__);

    switch(_degformat)
    {
	    case IARU_LOC:
    	case UK_OSGB:
    	case UK_NGR:
    	case UK_NGR6:
    		// These formats should not be formatted in the same way
    		// - they need to be converted first, therefore if we reach 
    		// this bit of code use the first available format - drop through.
        case DDPDDDDD:
            sprintf(scoor, "%.5f°", coor);
            break;

        case DDPDDDDD_NSEW:
            sprintf(scoor, "%.5f° %c", acoor,
                    coor < 0.0 ? neg_char : pos_char);
            break;
        case NSEW_DDPDDDDD:
            sprintf(scoor, "%c %.5f°",
                    coor < 0.0 ? neg_char : pos_char,
                    acoor);
            break;

        case DD_MMPMMM:
            sprintf(scoor, "%d°%06.3f'",
                    (int)coor, (acoor - (int)acoor)*60.0);
            break;
        case DD_MMPMMM_NSEW:
            sprintf(scoor, "%d°%06.3f' %c",
                    (int)acoor, (acoor - (int)acoor)*60.0,
                    coor < 0.0 ? neg_char : pos_char);
            break;
        case NSEW_DD_MMPMMM:
            sprintf(scoor, "%c %d° %06.3f'",
                    coor < 0.0 ? neg_char : pos_char,
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
                    coor < 0.0 ? neg_char : pos_char);
            break;
        case NSEW_DD_MM_SSPS:
            min = (acoor - (int)acoor)*60.0;
            sprintf(scoor, "%c %d° %02d' %04.1f\"",
                    coor < 0.0 ? neg_char : pos_char,
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
    buffer = g_strdup_printf(_route_dl_url_table[_route_dl_index].url, addr_escaped, addr_escaped);
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
 * in nautical miles and should be converted using UNITS_CONVERT[_units].
 */
gdouble
calculate_distance(gdouble lat1, gdouble lon1, gdouble lat2, gdouble lon2)
{
    gdouble dlat, dlon, slat, slon, a;

    /* Convert to radians. */
    lat1 = deg2rad(lat1);
    lon1 = deg2rad(lon1);
    lat2 = deg2rad(lat2);
    lon2 = deg2rad(lon2);

    dlat = lat2 - lat1;
    dlon = lon2 - lon1;

    slat = sin(dlat / 2.0);
    slon = sin(dlon / 2.0);
    a = (slat * slat) + (cos(lat1) * cos(lat2) * slon * slon);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return ((2.0 * atan2(sqrt(a), sqrt(1.0 - a))) * EARTH_RADIUS);
}

/**
 * Calculate the bearing between two lat/lon pairs.  The bearing is returned
 * as the angle from lat1/lon1 to lat2/lon2.
 */
gdouble
calculate_bearing(gdouble lat1, gdouble lon1, gdouble lat2, gdouble lon2)
{
    gdouble x, y;
    gdouble dlon = deg2rad(lon2 - lon1);
    lat1 = deg2rad(lat1);
    lat2 = deg2rad(lat2);

    y = sin(dlon) * cos(lat2);
    x = (cos(lat1) * sin(lat2)) - (sin(lat1) * cos(lat2) * cos(dlon));

    dlon = rad2deg(atan2(y, x));
    if(dlon < 0.0)
        dlon += 360.0;
    return dlon;
}



void
force_min_visible_bars(HildonControlbar *control_bar, gint num_bars)
{
#ifdef LEGACY
    GValue val;
    printf("%s()\n", __PRETTY_FUNCTION__);
    memset(&val, 0, sizeof(val));
    g_value_init(&val, G_TYPE_INT);
    g_value_set_int(&val, num_bars);
    g_object_set_property(G_OBJECT(control_bar), "minimum-visible-bars", &val);
    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
#endif
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

double marc(double bf0, double n, double phi0, double phi)
{
	return bf0 * (((1 + n + ((5 / 4) * (n * n)) + ((5 / 4) * (n * n * n))) * (phi - phi0))
	    - (((3 * n) + (3 * (n * n)) + ((21 / 8) * (n * n * n))) * (sin(phi - phi0)) * (cos(phi + phi0)))
	    + ((((15 / 8) * (n * n)) + ((15 / 8) * (n * n * n))) * (sin(2 * (phi - phi0))) * (cos(2 * (phi + phi0))))
	    - (((35 / 24) * (n * n * n)) * (sin(3 * (phi - phi0))) * (cos(3 * (phi + phi0)))));
}

gboolean os_grid_check_lat_lon(double lat, double lon)
{
	// TODO - Check exact OS Grid range
	if(lat < 50.0 || lat > 62 || lon < -7.5 || lon > 2.2 )
	{
		return FALSE;
	}
	else
	{
		return TRUE;
	}
}

gboolean coord_system_check_lat_lon (gdouble lat, gdouble lon, gint *fallback_deg_format)
{
	// Is the current coordinate system applicable to the provided lat and lon?
	gboolean valid = FALSE;
	
	switch(_degformat)
	{
	case UK_OSGB:
	case UK_NGR:
	case UK_NGR6:
		valid = os_grid_check_lat_lon(lat, lon);
		if(fallback_deg_format != NULL) *fallback_deg_format = DDPDDDDD; 
		break;
	default:
		valid = TRUE;
		break;
	}
	
	return valid;
}

gboolean convert_lat_long_to_os_grid(double lat, double lon, int *easting, int *northing)
{
	if(!os_grid_check_lat_lon(lat, lon))
	{
		return FALSE;
	}
	
	const double deg2rad_multi = (2 * PI / 360);
	
	const double phi = lat * deg2rad_multi;          // convert latitude to radians
	const double lam = lon * deg2rad_multi;          // convert longitude to radians
	const double a = 6377563.396;              // OSGB semi-major axis
	const double b = 6356256.91;               // OSGB semi-minor axis
	const double e0 = 400000;                  // easting of false origin
	const double n0 = -100000;                 // northing of false origin
	const double f0 = 0.9996012717;            // OSGB scale factor on central meridian
	const double e2 = 0.0066705397616;         // OSGB eccentricity squared
	const double lam0 = -0.034906585039886591; // OSGB false east
	const double phi0 = 0.85521133347722145;   // OSGB false north
	const double af0 = a * f0;
	const double bf0 = b * f0;

	// easting
	double slat2 = sin(phi) * sin(phi);
	double nu = af0 / (sqrt(1 - (e2 * (slat2))));
	double rho = (nu * (1 - e2)) / (1 - (e2 * slat2));
	double eta2 = (nu / rho) - 1;
	double p = lam - lam0;
	double IV = nu * cos(phi);
	double clat3 = pow(cos(phi), 3);
	double tlat2 = tan(phi) * tan(phi);
	double V = (nu / 6) * clat3 * ((nu / rho) - tlat2);
	double clat5 = pow(cos(phi), 5);
	double tlat4 = pow(tan(phi), 4);
	double VI = (nu / 120) * clat5 * ((5 - (18 * tlat2)) + tlat4 + (14 * eta2) - (58 * tlat2 * eta2));
	double east = e0 + (p * IV) + (pow(p, 3) * V) + (pow(p, 5) * VI);
		
	// northing
	double n = (af0 - bf0) / (af0 + bf0);
	double M = marc(bf0, n, phi0, phi);
	double I = M + (n0);
	double II = (nu / 2) * sin(phi) * cos(phi);
	double III = ((nu / 24) * sin(phi) * pow(cos(phi), 3)) * (5 - pow(tan(phi), 2) + (9 * eta2));
	double IIIA = ((nu / 720) * sin(phi) * clat5) * (61 - (58 * tlat2) + tlat4);
	double north = I + ((p * p) * II) + (pow(p, 4) * III) + (pow(p, 6) * IIIA);

	 // make whole number values
	 *easting = round(east);   // round to whole number
	 *northing = round(north); // round to whole number

	 return TRUE;
}

gboolean convert_os_grid_to_bng(gint easting, gint northing, gchar* bng)
{
	gdouble eX = (gdouble)easting / 500000.0;
	gdouble nX = (gdouble)northing / 500000.0;
	gdouble tmp = floor(eX) - 5.0 * floor(nX) + 17.0;
	gchar eing[12];
	gchar ning[12];
	
	nX = 5.0 * (nX - floor(nX));
	eX = 20.0 - 5.0 * floor(nX) + floor(5.0 * (eX - floor(eX)));
	if (eX > 7.5) eX = eX + 1;    // I is not used
	if (tmp > 7.5) tmp = tmp + 1; // I is not used
	
	snprintf(eing, 12, "%u", easting);
	snprintf(ning, 12, "%u", northing);
	
	snprintf(eing, (_degformat == UK_NGR ? 5 : 4), "%s", eing+1);
	snprintf(ning, (_degformat == UK_NGR ? 5 : 4), "%s", ning+1);
		
	sprintf(bng, "%c%c%s%s", 
			(char)(tmp + 65), 
			(char)(eX + 65),
			eing, ning
		);
 
		
	return TRUE;
}

gint convert_str_to_int(const gchar *str)
{
	gint i=0;
	gint result = 0;
	while(str[i] >= '0' && str[i] <= '9')
	{
		gint v = str[i] - '0';
		
		result = (result * 10) + v;
		
		i++;
	}
	
	return result;
}
gboolean convert_os_xy_to_latlon(const gchar *easting, const gchar *northing, gdouble *d_lat, gdouble *d_lon)
{
    const double deg2rad_multi = (2 * PI / 360);
    
	const gdouble N = (gdouble)convert_str_to_int(northing);
	const gdouble E = (gdouble)convert_str_to_int(easting);

	const gdouble a = 6377563.396, b = 6356256.910;              // Airy 1830 major & minor semi-axes
	const gdouble F0 = 0.9996012717;                             // NatGrid scale factor on central meridian
	const gdouble lat0 = 49*PI/180, lon0 = -2*PI/180;  // NatGrid true origin
	const gdouble N0 = -100000, E0 = 400000;                     // northing & easting of true origin, metres
	const gdouble e2 = 1 - (b*b)/(a*a);                          // eccentricity squared
	const gdouble n = (a-b)/(a+b), n2 = n*n, n3 = n*n*n;

	gdouble lat=lat0, M=0;
	do {
		lat = (N-N0-M)/(a*F0) + lat;

		const gdouble Ma = (1 + n + (5/4)*n2 + (5/4)*n3) * (lat-lat0);
		const gdouble Mb = (3*n + 3*n*n + (21/8)*n3) * sin(lat-lat0) * cos(lat+lat0);
		const gdouble Mc = ((15/8)*n2 + (15/8)*n3) * sin(2*(lat-lat0)) * cos(2*(lat+lat0));
		const gdouble Md = (35/24)*n3 * sin(3*(lat-lat0)) * cos(3*(lat+lat0));
	    M = b * F0 * (Ma - Mb + Mc - Md);                // meridional arc

	} while (N-N0-M >= 0.00001);  // ie until < 0.01mm

	const gdouble cosLat = cos(lat), sinLat = sin(lat);
	const gdouble nu = a*F0/sqrt(1-e2*sinLat*sinLat);              // transverse radius of curvature
	const gdouble rho = a*F0*(1-e2)/pow(1-e2*sinLat*sinLat, 1.5);  // meridional radius of curvature
	const gdouble eta2 = nu/rho-1;

	const gdouble tanLat = tan(lat);
	const gdouble tan2lat = tanLat*tanLat, tan4lat = tan2lat*tan2lat, tan6lat = tan4lat*tan2lat;
	const gdouble secLat = 1/cosLat;
	const gdouble nu3 = nu*nu*nu, nu5 = nu3*nu*nu, nu7 = nu5*nu*nu;
	const gdouble VII = tanLat/(2*rho*nu);
	const gdouble VIII = tanLat/(24*rho*nu3)*(5+3*tan2lat+eta2-9*tan2lat*eta2);
	const gdouble IX = tanLat/(720*rho*nu5)*(61+90*tan2lat+45*tan4lat);
	const gdouble X = secLat/nu;
	const gdouble XI = secLat/(6*nu3)*(nu/rho+2*tan2lat);
	const gdouble XII = secLat/(120*nu5)*(5+28*tan2lat+24*tan4lat);
	const gdouble XIIA = secLat/(5040*nu7)*(61+662*tan2lat+1320*tan4lat+720*tan6lat);

	const gdouble dE = (E-E0), dE2 = dE*dE, dE3 = dE2*dE, dE4 = dE2*dE2, dE5 = dE3*dE2;
	const gdouble dE6 = dE4*dE2, dE7 = dE5*dE2;
	lat = lat - VII*dE2 + VIII*dE4 - IX*dE6;
	const gdouble lon = lon0 + X*dE - XI*dE3 + XII*dE5 - XIIA*dE7;
	
	*d_lon = lon / deg2rad_multi;
	*d_lat = lat / deg2rad_multi;
	
	return TRUE;
}

gboolean convert_os_ngr_to_latlon(const gchar *text, gdouble *d_lat, gdouble *d_lon)
{
	// get numeric values of letter references, mapping A->0, B->1, C->2, etc:
	gint l1;
	gint l2;

	gchar easting[7], northing[7];
		
	if( ((gchar)text[0])>='a' && ((gchar)text[0]) <= 'z' ) 
		l1 = text[0] - (gint)'a'; // lower case
	else if( ((gchar)text[0])>='A' && ((gchar)text[0]) <= 'Z' )
		l1 = text[0] - (gint)'A'; // upper case
	else
		return FALSE; // Not a letter - invalid grid ref
	
	if( ((gchar)text[1])>='a' && ((gchar)text[1]) <= 'z' ) 
		l2 = text[1] - (gint)'a'; // lower case
	else if( ((gchar)text[1])>='A' && ((gchar)text[1]) <= 'Z' )
		l2 = text[1] - (gint)'A'; // upper case
	else
		return FALSE; // Not a letter - invalid grid ref

	
	// shuffle down letters after 'I' since 'I' is not used in grid:
	if (l1 > 7) l1--;
	if (l2 > 7) l2--;
	  
	// convert grid letters into 100km-square indexes from false origin (grid square SV):
	gdouble e = ((l1-2)%5)*5 + (l2%5);
	gdouble n = (19-floor(l1/5)*5) - floor(l2/5);

	// skip grid letters to get numeric part of ref, stripping any spaces:
	gchar *gridref = (gchar*)(text+2); 
	
	// user may have entered a space, so remove any spaces
	while(gridref[0] == ' ')
	{
		gridref = (gchar*)(text+1);
	}

	
	// floor the length incase a space has been added
	const gint len = (gint)floor((gdouble)strlen(gridref)/2.00); // normally this will be 4, often 3
	if(len>5 || len <3) return FALSE; 
	
	snprintf(easting, 2+len, "%u%s", (gint)e, gridref);
	snprintf(northing, 2+len, "%u%s", (gint)n, gridref+len);
		
	fprintf(stderr, "easting = %s, northing = %s\n", easting, northing);	
	
	switch (len) 
	{
    	case 3:
    		easting[4] = '0';
    		easting[5] = '0';
    		northing[4] = '0';
    		northing[5] = '0';
    		break;
    	case 4:
    		easting[5] = '0';
    		northing[5] = '0';
    		break;
    	// 10-digit refs are already 1m
	}
	
	easting[6] = '\0';
	northing[6] = '\0';
	

	convert_os_xy_to_latlon(easting, northing, d_lat, d_lon);
	
	return TRUE;
}


// Attempt to convert any user entered grid reference to a double lat/lon
// return TRUE on valid
gboolean parse_coords(const gchar* txt_lat, const gchar* txt_lon, gdouble* lat, gdouble* lon)
{
	gboolean valid = FALSE;
	
	// UK_NGR starts with two letters, and then all numbers - it may contain spaces - no lon will be entered
	if( _degformat == UK_NGR || _degformat == UK_NGR6 )
	{
		valid = convert_os_ngr_to_latlon(txt_lat, lat, lon);

        if(!valid || *lat < -90. || *lat > 90.) { valid = FALSE; }
        else   if(*lon < -180. || *lon > 180.)  { valid = FALSE; }
        
	}
	// UK_OSGB contains two 6 digit integers
	else if( _degformat == UK_OSGB)
	{
		valid = convert_os_xy_to_latlon(txt_lat, txt_lon, lat, lon);
		
        if(!valid || *lat < -90. || *lat > 90.) { valid = FALSE; }
        else   if(*lon < -180. || *lon > 180.)  { valid = FALSE; }

	}
	else if( _degformat == IARU_LOC)
	{
		valid = convert_iaru_loc_to_lat_lon(txt_lat, lat, lon);

        if(!valid || *lat < -90. || *lat > 90.) { valid = FALSE; }
        else   if(*lon < -180. || *lon > 180.)  { valid = FALSE; }
	}
	// It must either be invalid, or a lat/lon format
	else
	{
		gchar* error_check;
		*lat = strdmstod(txt_lat, &error_check);
		
        if(txt_lat == error_check || *lat < -90. || *lat > 90.) { valid = FALSE; }
        else { valid = TRUE; }

        if(valid == TRUE)
        {
        	*lon = strdmstod(txt_lon, &error_check);
        	
            if(txt_lon == error_check || *lon < -180. || *lon > 180.)  { valid = FALSE; }
        }
	}
	
	
	
	return valid;
}

gboolean convert_iaru_loc_to_lat_lon(const gchar* txt_lon, gdouble* lat, gdouble* lon)
{
	gint u_first = 0;
	gint u_second = 0;
	gint u_third = 0;
	gint u_fourth = 0;
	gint u_fifth = 0;
	gint u_sixth = 0;
	gint u_seventh = 0;
	gint u_eighth = 0;
	
	if(strlen(txt_lon) >= 1)
	{
		if( ((gchar)txt_lon[0])>='a' && ((gchar)txt_lon[0]) <= 'z' ) 
			u_first = txt_lon[0] - (gint)'a'; // lower case
		else if( ((gchar)txt_lon[0])>='A' && ((gchar)txt_lon[0]) <= 'Z' )
			u_first = txt_lon[0] - (gint)'A'; // upper case
	}
	
	if(strlen(txt_lon) >= 2)
	{
		if( ((gchar)txt_lon[1])>='a' && ((gchar)txt_lon[1]) <= 'z' ) 
			u_second = txt_lon[1] - (gint)'a'; // lower case
		else if( ((gchar)txt_lon[1])>='A' && ((gchar)txt_lon[1]) <= 'Z' )
			u_second = txt_lon[1] - (gint)'A'; // upper case
	}
	
	if(strlen(txt_lon) >= 3)
		u_third = txt_lon[2] - (gint)'0';
	
	if(strlen(txt_lon) >= 4)
		u_fourth = txt_lon[3] - (gint)'0';

	if(strlen(txt_lon) >= 5)
	{
		if( ((gchar)txt_lon[4])>='a' && ((gchar)txt_lon[4]) <= 'z' ) 
			u_fifth = txt_lon[4] - (gint)'a'; // lower case
		else if( ((gchar)txt_lon[4])>='A' && ((gchar)txt_lon[4]) <= 'Z' )
			u_fifth = txt_lon[4] - (gint)'A'; // upper case
	}

	if(strlen(txt_lon) >= 6)
	{
		if( ((gchar)txt_lon[5])>='a' && ((gchar)txt_lon[5]) <= 'z' ) 
			u_sixth = txt_lon[5] - (gint)'a'; // lower case
		else if( ((gchar)txt_lon[5])>='A' && ((gchar)txt_lon[5]) <= 'Z' )
			u_sixth = txt_lon[5] - (gint)'A'; // upper case
	}
	

	if(strlen(txt_lon) >= 7)
		u_seventh= txt_lon[6] - (gint)'0';

	if(strlen(txt_lon) >= 8)
		u_eighth = txt_lon[7] - (gint)'0';
	
	*lat = ((gdouble)u_first * 20.0) + ((gdouble)u_third * 2.0) + ((gdouble)u_fifth * (2.0/24.0)) + ((gdouble)u_seventh * (2.0/240.0)) - 90.0;
	*lon = ((gdouble)u_second * 10.0) + ((gdouble)u_fourth) + ((gdouble)u_sixth * (1.0/24.0)) + ((gdouble)u_eighth * (1.0/240.0)) - 180.0;
	
	return TRUE;
}

void convert_lat_lon_to_iaru_loc(gdouble d_lat, gdouble d_lon, gchar *loc)
{
	const gdouble d_a_lat = (d_lat+90.0);
	const gdouble d_a_lon = (d_lon+180.0);
		
	const gint i_first  = (gint)floor(d_a_lon/20.0);
	const gint i_second = (gint)floor(d_a_lat/10.0);
	const gint i_third  = (gint)floor((d_a_lon - (20.0*i_first))/2.0);
	const gint i_fourth = (gint)floor((d_a_lat - (10.0*i_second)));
	const gint i_fifth  = (gint)floor((d_a_lon - (20.0*i_first) - (2.0*i_third))/(5.0/60.0));
	const gint i_sixth  = (gint)floor((d_a_lat - (10.0*i_second) - (i_fourth))/(2.5/60.0));
	
	const gint i_seventh  = (gint)floor((d_a_lon - (20.0*i_first) - (2.0*i_third) 
			- ((2.0/24.0)*i_fifth))/(2.0/240.0)  );
	const gint i_eighth = (gint)floor((d_a_lat - (10.0*i_second) - (i_fourth) 
			- ((1.0/24.0)*i_sixth))/(1.0/240.0));
	
	
	sprintf(loc, "%c%c%u%u%c%c%u%u",
			'A'+i_first,
			'A'+i_second,
			i_third,
			i_fourth,
			'a' + i_fifth,
			'a' + i_sixth,
			i_seventh,
			i_eighth);
}

gboolean convert_lat_lon_to_bng(gdouble lat, gdouble lon, gchar* bng)
{
	gint easting, northing;

	if( convert_lat_long_to_os_grid(lat, lon, &easting, &northing) )
	{
		if( convert_os_grid_to_bng(easting, northing, bng) )
		{
			return TRUE;
		}
	}
	
	return FALSE;
}


void format_lat_lon(gdouble d_lat, gdouble d_lon, gchar* lat, gchar* lon)
{
	gint east = 0;
	gint north = 0;

	switch (_degformat)
	{
	case UK_OSGB:
		
		if(convert_lat_long_to_os_grid(d_lat, d_lon, &east, &north))
		{
			sprintf(lat, "%06d", east);
			sprintf(lon, "%06d", north);
		}
		else
		{
			// Failed (possibly out of range), so use defaults
			lat_format(d_lat, lat);
			lon_format(d_lon, lon);				
		}
		break;
	case UK_NGR:
	case UK_NGR6:
		
		if(convert_lat_lon_to_bng(d_lat, d_lon, lat))
		{
			lon[0] = 0;
		}
		else
		{
			// Failed (possibly out of range), so use defaults
			lat_format(d_lat, lat);
			lat_format(d_lon, lon);				
		}
		break;
		
	case IARU_LOC:
		convert_lat_lon_to_iaru_loc(d_lat, d_lon, lat);

		break;
		
	default:
		lat_format(d_lat, lat);
		lon_format(d_lon, lon);

		break;
	}
}

/* Custom version of g_ascii_strtoll, since Gregale does not support
 * GLIB 2.12. */
gint64
g_ascii_strtoll(const gchar *nptr, gchar **endptr, guint base)
{
    gchar *minus = g_strstr_len(nptr, -1, "");
    if(minus)
        return -g_ascii_strtoull(minus + 1, endptr, base);
    else
        return g_ascii_strtoull(nptr, endptr, base);
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
