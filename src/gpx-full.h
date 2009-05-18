#ifndef MAEMO_MAPPER_GPX_FULL_H
#define MAEMO_MAPPER_GPX_FULL_H

#include <glib.h>

#include "types.h"

/* Initialize file with GPX data */
gboolean gpx_full_initialize (gboolean enabled, gchar* path);

/* Append GPX data portion to file */
gboolean gpx_full_append (GpsData* data, Point* pos);

/* Finalize file */
gboolean gpx_full_finalize ();

#endif
