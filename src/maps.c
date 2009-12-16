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

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <glib/gstdio.h>
#include <fcntl.h>
#include <locale.h>
#include <time.h>

#ifndef LEGACY
#    include <hildon/hildon-note.h>
#    include <hildon/hildon-file-chooser-dialog.h>
#    include <hildon/hildon-number-editor.h>
#    include <hildon/hildon-banner.h>
#else
#    include <osso-helplib.h>
#    include <hildon-widgets/hildon-note.h>
#    include <hildon-widgets/hildon-file-chooser-dialog.h>
#    include <hildon-widgets/hildon-number-editor.h>
#    include <hildon-widgets/hildon-banner.h>
#    include <hildon-widgets/hildon-input-mode-hint.h>
#endif


#include "types.h"
#include "data.h"
#include "defines.h"

#include "display.h"
#include "main.h"
#include "maps.h"
#include "menu.h"
#include "settings.h"
#include "util.h"

typedef struct
{
    MapUpdateCb callback;
    gpointer user_data;
} MapUpdateCbData;

typedef struct
{
    MapTileSpec tile;
    gint priority;
    gint8 update_type;
    gint8 layer_level;
    guint downloading : 1;
    /* list of MapUpdateCbData */
    GSList *callbacks;

    /* Output values */
    GdkPixbuf *pixbuf;
    GError *error;
} MapUpdateTask;

typedef struct _RepoManInfo RepoManInfo;
struct _RepoManInfo {
    GtkWidget *dialog;
    GtkWidget *notebook;
    GtkWidget *cmb_repos;
    GList *repo_edits;
};

typedef struct _RepoEditInfo RepoEditInfo;
struct _RepoEditInfo {
    gchar *name;
    gboolean is_sqlite;
    GtkWidget *txt_url;
    GtkWidget *txt_db_filename;
    GtkWidget *num_dl_zoom_steps;
    GtkWidget *num_view_zoom_steps;
    GtkWidget *chk_double_size;
    GtkWidget *chk_nextable;
    GtkWidget *btn_browse;
    GtkWidget *btn_compact;
    GtkWidget *num_min_zoom;
    GtkWidget *num_max_zoom;
    BrowseInfo browse_info;
    RepoData *repo;
};


typedef struct _RepoLayersInfo RepoLayersInfo;
struct _RepoLayersInfo {
    GtkWidget *dialog;
    GtkWidget *notebook;
    GtkListStore *layers_store;
    GtkWidget *layers_list;
    GList *layer_edits;
};


typedef struct _LayerEditInfo LayerEditInfo;
struct _LayerEditInfo {
    RepoLayersInfo *rli;

    GtkWidget *txt_name;
    GtkWidget *txt_url;
    GtkWidget *txt_db;
    GtkWidget *num_autofetch;
    GtkWidget *chk_visible;
    GtkWidget *vbox;
};



typedef struct _MapmanInfo MapmanInfo;
struct _MapmanInfo {
    GtkWidget *dialog;
    GtkWidget *notebook;
    GtkWidget *tbl_area;

    /* The "Setup" tab. */
    GtkWidget *rad_download;
    GtkWidget *rad_delete;
    GtkWidget *chk_overwrite;
    GtkWidget *rad_by_area;
    GtkWidget *rad_by_route;
    GtkWidget *num_route_radius;

    /* The "Area" tab. */
    GtkWidget *txt_topleft_lat;
    GtkWidget *txt_topleft_lon;
    GtkWidget *txt_botright_lat;
    GtkWidget *txt_botright_lon;

    /* The "Zoom" tab. */
    GtkWidget *chk_zoom_levels[MAX_ZOOM + 1];
};


typedef struct _CompactInfo CompactInfo;
struct _CompactInfo {
    GtkWidget *dialog;
    GtkWidget *txt;
    GtkWidget *banner;
    const gchar *db_filename;
    gboolean is_sqlite;
    gchar *status_msg;
};

typedef struct _MapCacheKey MapCacheKey;
struct _MapCacheKey {
    RepoData      *repo;
    gint           zoom;
    gint           tilex;
    gint           tiley;
};

typedef struct _MapCacheEntry MapCacheEntry;
struct _MapCacheEntry {
    MapCacheKey    key;
    int            list;
    guint          size;
    guint          data_sz;
    gchar         *data;
    GdkPixbuf     *pixbuf;
    MapCacheEntry *next;
    MapCacheEntry *prev;
};

typedef struct _MapCacheList MapCacheList;
struct _MapCacheList {
    MapCacheEntry *head;
    MapCacheEntry *tail;
    size_t         size;
    size_t         data_sz;
};

typedef struct _MapCache MapCache;
struct _MapCache {
    MapCacheList  lists[4];
    size_t        cache_size;
    size_t        p;
    size_t        thits;
    size_t        bhits;
    size_t        misses;
    GHashTable   *entries;
};

const gchar* layer_timestamp_key = "tEXt::mm_ts";

static void
build_tile_path(gchar *buffer, gsize size,
                RepoData *repo, gint zoom, gint tilex, gint tiley)
{
    g_snprintf(buffer, size,
               "/home/user/MyDocs/.maps/%s/%d/%d/",
               repo->name, zoom, tilex);
}

static void
build_tile_filename(gchar *buffer, gsize size,
                    RepoData *repo, gint zoom, gint tilex, gint tiley)
{
    g_snprintf(buffer, size,
               "/home/user/MyDocs/.maps/%s/%d/%d/%d.png",
               repo->name, zoom, tilex, tiley);
}

gboolean
mapdb_exists(RepoData *repo, gint zoom, gint tilex, gint tiley)
{
    gchar filename[200];
    gboolean exists;
    vprintf("%s(%s, %d, %d, %d)\n", __PRETTY_FUNCTION__,
            repo->name, zoom, tilex, tiley);

    build_tile_filename(filename, sizeof(filename), repo, zoom, tilex, tiley);
    exists = g_file_test(filename, G_FILE_TEST_EXISTS);

    g_debug("%s(): return %d", __PRETTY_FUNCTION__, exists);
    return exists;
}

GdkPixbuf*
mapdb_get(RepoData *repo, gint zoom, gint tilex, gint tiley)
{
    gchar filename[200];
    GdkPixbuf *pixbuf;
    vprintf("%s(%s, %d, %d, %d)\n", __PRETTY_FUNCTION__,
            repo->name, zoom, tilex, tiley);

    build_tile_filename(filename, sizeof(filename), repo, zoom, tilex, tiley);
    pixbuf = gdk_pixbuf_new_from_file(filename, NULL);

    g_debug("%s(%s, %d, %d, %d): return %p", __PRETTY_FUNCTION__,
           repo->name, zoom, tilex, tiley, pixbuf);
    return pixbuf;
}

static gboolean
mapdb_update(RepoData *repo, gint zoom, gint tilex, gint tiley,
        void *bytes, gint size)
{
    gint success = TRUE;
    gchar filename[200], path[200];
    vprintf("%s(%s, %d, %d, %d)\n", __PRETTY_FUNCTION__,
            repo->name, zoom, tilex, tiley);

    build_tile_path(path, sizeof(path), repo, zoom, tilex, tiley);
    g_mkdir_with_parents(path, 0766);
    build_tile_filename(filename, sizeof(filename), repo, zoom, tilex, tiley);
    success = g_file_set_contents(filename, bytes, size, NULL);

    g_debug("%s(): return %d", __PRETTY_FUNCTION__, success);
    return success;
}

static gboolean
mapdb_delete(RepoData *repo, gint zoom, gint tilex, gint tiley)
{
    gchar filename[200];
    gint success = FALSE;
    vprintf("%s(%s, %d, %d, %d)\n", __PRETTY_FUNCTION__,
            repo->name, zoom, tilex, tiley);

    build_tile_filename(filename, sizeof(filename), repo, zoom, tilex, tiley);
    g_remove(filename);

    vprintf("%s(): return %d\n", __PRETTY_FUNCTION__, success);
    return success;
}

void
set_repo_type(RepoData *repo)
{
    printf("%s(%s)\n", __PRETTY_FUNCTION__, repo->url);

    if(repo->url && *repo->url)
    {
        gchar *url = g_utf8_strdown(repo->url, -1);

        /* Determine type of repository. */
        if(strstr(url, "service=wms"))
            repo->type = REPOTYPE_WMS;
        else if(strstr(url, "%s"))
            repo->type = REPOTYPE_QUAD_QRST;
        else if(strstr(url, "%0d"))
            repo->type = REPOTYPE_XYZ_INV;
        else if(strstr(url, "%-d"))
            repo->type = REPOTYPE_XYZ_SIGNED;
        else if(strstr(url, "%0s"))
            repo->type = REPOTYPE_QUAD_ZERO;
        else
            repo->type = REPOTYPE_XYZ;

        g_free(url);
    }
    else
        repo->type = REPOTYPE_NONE;

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

gboolean
repo_set_curr(RepoData *rd)
{
    /* Set the current repository */
    _curr_repo = rd;
    return TRUE;
}


/**
 * Returns true if:
 * 1. base == layer, or
 * 2. layer is sublayer of base
 */
gboolean repo_is_layer (RepoData* base, RepoData* layer)
{
    while (base) {
        if (base == layer)
            return TRUE;
        base = base->layers;
    }

    return FALSE;
}


/**
 * Given a wms uri pattern, compute the coordinate transformation and
 * trimming.
 * 'proj' is used for the conversion
 */
static gchar*
map_convert_wms_to_wms(gint tilex, gint tiley, gint zoomlevel, gchar* uri)
{
    gint system_retcode;
    gchar cmd[BUFFER_SIZE], srs[BUFFER_SIZE];
    gchar *ret = NULL;
    FILE* in;
    gdouble lon1, lat1, lon2, lat2;

    gchar *widthstr   = strcasestr(uri,"WIDTH=");
    gchar *heightstr  = strcasestr(uri,"HEIGHT=");
    gchar *srsstr     = strcasestr(uri,"SRS=EPSG");
    gchar *srsstre    = strchr(srsstr,'&');
    vprintf("%s()\n", __PRETTY_FUNCTION__);

    /* missing: test if found */
    strcpy(srs,"epsg");
    strncpy(srs+4,srsstr+8,256);
    /* missing: test srsstre-srsstr < 526 */
    srs[srsstre-srsstr-4] = 0;
    /* convert to lower, as WMC is EPSG and cs2cs is epsg */

    gint dwidth  = widthstr ? atoi(widthstr+6) - TILE_SIZE_PIXELS : 0;
    gint dheight = heightstr ? atoi(heightstr+7) - TILE_SIZE_PIXELS : 0;

    unit2latlon(tile2zunit(tilex,zoomlevel)
            - pixel2zunit(dwidth/2,zoomlevel),
            tile2zunit(tiley+1,zoomlevel)
            + pixel2zunit((dheight+1)/2,zoomlevel),
            lat1, lon1);

    unit2latlon(tile2zunit(tilex+1,zoomlevel)
            + pixel2zunit((dwidth+1)/2,zoomlevel),
            tile2zunit(tiley,zoomlevel)
            - pixel2zunit(dheight/2,zoomlevel),
            lat2, lon2);

    setlocale(LC_NUMERIC, "C");

    snprintf(cmd, sizeof(cmd),
            "(echo \"%.6f %.6f\"; echo \"%.6f %.6f\") | "
            "/usr/bin/cs2cs +proj=longlat +datum=WGS84 +to +init=%s -f %%.6f "
            " > /tmp/tmpcs2cs ",
            lon1, lat1, lon2, lat2, srs);
    vprintf("Running command: %s\n", cmd);
    system_retcode = system(cmd);

    if(system_retcode)
        g_printerr("cs2cs returned error code %d\n",
                WEXITSTATUS(system_retcode));
    else if(!(in = g_fopen("/tmp/tmpcs2cs","r")))
        g_printerr("Cannot open results of conversion\n");
    else if(5 != fscanf(in,"%lf %lf %s %lf %lf",
                &lon1, &lat1, cmd, &lon2, &lat2))
    {
        g_printerr("Wrong conversion\n");
        fclose(in);
    }
    else
    {
        fclose(in);
        ret = g_strdup_printf(uri, lon1, lat1, lon2, lat2);
    }

    setlocale(LC_NUMERIC, "");

    vprintf("%s(): return %s\n", __PRETTY_FUNCTION__, ret);
    return ret;
}


/**
 * Given the xyz coordinates of our map coordinate system, write the qrst
 * quadtree coordinates to buffer.
 */
static void
map_convert_coords_to_quadtree_string(gint x, gint y, gint zoomlevel,
                                      gchar *buffer, const gchar initial,
                                      const gchar *const quadrant)
{
    gchar *ptr = buffer;
    gint n;
    vprintf("%s()\n", __PRETTY_FUNCTION__);

    if (initial)
        *ptr++ = initial;

    for(n = MAX_ZOOM - zoomlevel; n >= 0; n--)
    {
        gint xbit = (x >> n) & 1;
        gint ybit = (y >> n) & 1;
        *ptr++ = quadrant[xbit + 2 * ybit];
    }
    *ptr++ = '\0';
    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

/**
 * Construct the URL that we should fetch, based on the current URI format.
 * This method works differently depending on if a "%s" string is present in
 * the URI format, since that would indicate a quadtree-based map coordinate
 * system.
 */
static gchar*
map_construct_url(RepoData *repo, gint zoom, gint tilex, gint tiley)
{
    gchar *retval;
    vprintf("%s(%p, %d, %d, %d)\n", __PRETTY_FUNCTION__,
            repo, zoom, tilex, tiley);
    switch(repo->type)
    {
        case REPOTYPE_XYZ:
            retval = g_strdup_printf(repo->url,
                    tilex, tiley,  zoom - (MAX_ZOOM - 16));
            break;

        case REPOTYPE_XYZ_INV:
            retval = g_strdup_printf(repo->url,
                    MAX_ZOOM + 1 - zoom, tilex, tiley);
            break;

        case REPOTYPE_XYZ_SIGNED:
            retval = g_strdup_printf(repo->url,
                    tilex,
                    (1 << (MAX_ZOOM - zoom)) - tiley - 1,
                    zoom - (MAX_ZOOM - 17));
            break;

        case REPOTYPE_QUAD_QRST:
        {
            gchar location[MAX_ZOOM + 2];
            map_convert_coords_to_quadtree_string(
                    tilex, tiley, zoom, location, 't', "qrts");
            retval = g_strdup_printf(repo->url, location);
            break;
        }

        case REPOTYPE_QUAD_ZERO:
        {
            /* This is a zero-based quadtree URI. */
            gchar location[MAX_ZOOM + 2];
            map_convert_coords_to_quadtree_string(
                    tilex, tiley, zoom, location, '\0', "0123");
            retval = g_strdup_printf(repo->url, location);
            break;
        }

        case REPOTYPE_WMS:
            retval = map_convert_wms_to_wms(tilex, tiley, zoom, repo->url);
            break;

        default:
            retval = g_strdup(repo->url);
            break;
    }
    vprintf("%s(): return \"%s\"\n", __PRETTY_FUNCTION__, retval);
    return retval;
}

static gboolean
mapdb_initiate_update_banner_idle()
{
    if (!_download_banner)
    {
        _download_banner = hildon_banner_show_progress(
                _window, NULL, _("Processing Maps"));
        /* If we're not connected, then hide the banner immediately.  It will
         * be unhidden if/when we're connected. */
        if(!_conic_is_connected)
            gtk_widget_hide(_download_banner);
    }
    return FALSE;
}

static void
map_update_tile_int(MapTileSpec *tile, gint priority, MapUpdateType update_type,
                    MapUpdateCb callback, gpointer user_data)
{
    MapUpdateTask *mut;
    MapUpdateTask *old_mut;
    MapUpdateCbData *cb_data = NULL;

    g_debug("%s(%s, %d, %d, %d, %d)", G_STRFUNC,
            tile->repo->name, tile->zoom, tile->tilex, tile->tiley, update_type);

    mut = g_slice_new0(MapUpdateTask);
    if (!mut)
    {
        /* Could not allocate memory. */
        g_error("Out of memory in allocation of update task");
        return;
    }
    memcpy(&mut->tile, tile, sizeof(MapTileSpec));
    mut->layer_level = tile->repo->layer_level;
    mut->priority = priority;
    mut->update_type = update_type;

    if (callback)
    {
        cb_data = g_slice_new(MapUpdateCbData);
        cb_data->callback = callback;
        cb_data->user_data = user_data;
    }

    g_mutex_lock(_mut_priority_mutex);
    old_mut = g_hash_table_lookup(_mut_exists_table, mut);
    if (old_mut)
    {
        g_debug("Task already queued, adding listener");

        if (cb_data)
            old_mut->callbacks = g_slist_prepend(old_mut->callbacks, cb_data);

        /* Check if the priority of the new task is higher */
        if (old_mut->priority > mut->priority && !old_mut->downloading)
        {
            g_debug("re-insert, old priority = %d", old_mut->priority);
            /* It is, so remove the task from the tree, update its priority and
             * re-insert it with the new one */
            g_tree_remove(_mut_priority_tree, old_mut);
            old_mut->priority = mut->priority;
            g_tree_insert(_mut_priority_tree, old_mut, old_mut);
        }

        /* free the newly allocated task */
        g_slice_free(MapUpdateTask, mut);
        g_mutex_unlock(_mut_priority_mutex);
        return;
    }

    if (cb_data)
        mut->callbacks = g_slist_prepend(mut->callbacks, cb_data);

    g_hash_table_insert(_mut_exists_table, mut, mut);
    g_tree_insert(_mut_priority_tree, mut, mut);

    g_mutex_unlock(_mut_priority_mutex);

    /* Increment download count and (possibly) display banner. */
    if (g_hash_table_size(_mut_exists_table) >= 20 && !_download_banner)
        g_idle_add((GSourceFunc)mapdb_initiate_update_banner_idle, NULL);

    /* This doesn't need to be thread-safe.  Extras in the pool don't
     * really make a difference. */
    if (g_thread_pool_get_num_threads(_mut_thread_pool)
        < g_thread_pool_get_max_threads(_mut_thread_pool))
        g_thread_pool_push(_mut_thread_pool, (gpointer)1, NULL);
}

void
map_download_tile(MapTileSpec *tile, gint priority,
                  MapUpdateCb callback, gpointer user_data)
{
    map_update_tile_int(tile, priority, MAP_UPDATE_AUTO, callback, user_data);
}

/**
 * Initiate a download of the given xyz coordinates using the given buffer
 * as the URL.  If the map already exists on disk, or if we are already
 * downloading the map, then this method does nothing.
 */
gboolean
mapdb_initiate_update(RepoData *repo, gint zoom, gint tilex, gint tiley,
        gint update_type, gint batch_id, gint priority,
        ThreadLatch *refresh_latch)
{
    MapTileSpec tile;

    tile.repo = repo;
    tile.zoom = zoom;
    tile.tilex = tilex;
    tile.tiley = tiley;
    map_update_tile_int(&tile, priority, update_type,
                        map_download_refresh_idle,
                        GINT_TO_POINTER(update_type));
    return FALSE;
}

static gboolean
get_next_mut(gpointer key, gpointer value, MapUpdateTask **data)
{
    *data = key;
    return TRUE;
}

static gboolean
map_handle_error(gchar *error)
{
    MACRO_BANNER_SHOW_INFO(_window, error);
    return FALSE;
}

static gboolean
map_update_task_completed(MapUpdateTask *mut)
{
    MapTileSpec *tile = &mut->tile;

    g_debug("%s(%s, %d, %d, %d)", G_STRFUNC,
            tile->repo->name, tile->zoom, tile->tilex, tile->tiley);

    g_mutex_lock(_mut_priority_mutex);
    g_hash_table_remove(_mut_exists_table, mut);

    /* notify all listeners */
    mut->callbacks = g_slist_reverse(mut->callbacks);
    while (mut->callbacks != NULL)
    {
        MapUpdateCbData *cb_data = mut->callbacks->data;

        (cb_data->callback)(tile, mut->pixbuf, mut->error, cb_data->user_data);

        g_slice_free(MapUpdateCbData, cb_data);
        mut->callbacks = g_slist_delete_link(mut->callbacks, mut->callbacks);
    }
    g_mutex_unlock(_mut_priority_mutex);

    if (g_hash_table_size(_mut_exists_table) == 0)
    {
        g_thread_pool_stop_unused_threads();

        if (_curr_repo->gdbm_db && !_curr_repo->is_sqlite)
            gdbm_sync(_curr_repo->gdbm_db);

        if (_download_banner)
        {
            gtk_widget_destroy(_download_banner);
            _download_banner = NULL;
        }
    }

    /* clean up the task memory */
    if (mut->pixbuf)
        g_object_unref(mut->pixbuf);
    if (mut->error)
        g_error_free(mut->error);
    g_slice_free(MapUpdateTask, mut);

    /* don't call again */
    return FALSE;
}

static void
download_tile(MapTileSpec *tile, gchar **bytes, gint *size,
              GdkPixbuf **pixbuf, GError **error)
{
    gchar *src_url;
    GnomeVFSResult vfs_result;
    GdkPixbufLoader *loader = NULL;

    g_debug("%s (%s, %d, %d, %d)", G_STRFUNC,
            tile->repo->name, tile->zoom, tile->tilex, tile->tiley);

    /* First, construct the URL from which we will get the data. */
    src_url = map_construct_url(tile->repo, tile->zoom,
                                tile->tilex, tile->tiley);

    /* Now, attempt to read the entire contents of the URL. */
    vfs_result = gnome_vfs_read_entire_file(src_url, size, bytes);
    g_free(src_url);

    if (vfs_result != GNOME_VFS_OK || *bytes == NULL)
    {
        /* it might not be very proper, but let's use GFile error codes */
        g_set_error(error, g_file_error_quark(), G_FILE_ERROR_FAULT,
                    "%s", gnome_vfs_result_to_string(vfs_result));
        goto l_error;
    }

    /* Attempt to parse the bytes into a pixbuf. */
    loader = gdk_pixbuf_loader_new();
    gdk_pixbuf_loader_write(loader, (guchar *)(*bytes), *size, NULL);
    gdk_pixbuf_loader_close(loader, error);
    if (*error) goto l_error;

    *pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
    if (!*pixbuf)
    {
        g_set_error(error, g_file_error_quark(), G_FILE_ERROR_INVAL,
                    "Creation of pixbuf failed");
        goto l_error;
    }
    g_object_ref(*pixbuf);
    g_object_unref(loader);
    return;

l_error:
    if (loader)
        g_object_unref(loader);
    g_free(*bytes);
    *bytes = NULL;
}

static void
map_update_task_remove_all(const GError *error)
{
    g_debug("%s", G_STRFUNC);

    while (1)
    {
        MapUpdateTask *mut = NULL;

        g_mutex_lock(_mut_priority_mutex);
        g_tree_foreach(_mut_priority_tree, (GTraverseFunc)get_next_mut, &mut);
        if (!mut)
        {
            g_mutex_unlock(_mut_priority_mutex);
            break;
        }

        /* Mark this MUT as "in-progress". */
        mut->downloading = TRUE;
        g_tree_remove(_mut_priority_tree, mut);
        g_mutex_unlock(_mut_priority_mutex);

        mut->pixbuf = NULL;
        mut->error = g_error_copy(error);
        g_idle_add((GSourceFunc)map_update_task_completed, mut);
    }
}

gboolean
thread_proc_mut()
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    /* Make sure things are inititalized. */
    gnome_vfs_init();

    while(conic_ensure_connected())
    {
        gint retries;
        gboolean notification_sent = FALSE, layer_tile;
        MapUpdateTask *mut = NULL;
        MapTileSpec *tile;

        /* Get the next MUT from the mut tree. */
        g_mutex_lock(_mut_priority_mutex);
        g_tree_foreach(_mut_priority_tree, (GTraverseFunc)get_next_mut, &mut);
        if (!mut)
        {
            /* No more MUTs to process.  Return. */
            g_mutex_unlock(_mut_priority_mutex);
            return FALSE;
        }

        tile = &mut->tile;
        /* Mark this MUT as "in-progress". */
        mut->downloading = TRUE;
        g_tree_remove(_mut_priority_tree, mut);
        g_mutex_unlock(_mut_priority_mutex);

        g_debug("%s %p (%s, %d, %d, %d)", G_STRFUNC, mut,
                tile->repo->name, tile->zoom, tile->tilex, tile->tiley);

        layer_tile = tile->repo != _curr_repo &&
            repo_is_layer(_curr_repo, tile->repo);

        mut->pixbuf = NULL;
        mut->error = NULL;

        if (tile->repo != _curr_repo && !layer_tile)
        {
            /* Do nothing, except report that there is no error. */
        }
        else if (mut->update_type == MAP_UPDATE_DELETE)
        {
            /* Easy - just delete the entry from the database.  We don't care
             * about failures (sorry). */
            if (MAPDB_EXISTS(tile->repo))
                mapdb_delete(tile->repo, tile->zoom, tile->tilex, tile->tiley);
        }
        else
        {
            gboolean download_needed = TRUE;

            /* First check for existence. */
            if (mut->update_type == MAP_UPDATE_ADD)
            {
                /* We don't want to overwrite, so check for existence. */
                /* Map already exists, and we're not going to overwrite. */
                if (mapdb_exists(tile->repo, tile->zoom, tile->tilex, tile->tiley))
                {
                    download_needed = FALSE;
                }
            }

            if (download_needed)
            {
                RepoData *repo;
                gint zoom, tilex, tiley;
                gchar *bytes = NULL;
                gint size;

                for (retries = tile->repo->layer_level
                     ? 1 : INITIAL_DOWNLOAD_RETRIES; retries > 0; --retries)
                {
                    g_clear_error(&mut->error);
                    download_tile(tile, &bytes, &size,
                                  &mut->pixbuf, &mut->error);
                    if (mut->pixbuf) break;

                    g_debug("Download failed, retrying");
                }

                /* Copy database-relevant mut data before we release it. */
                repo = tile->repo;
                zoom = tile->zoom;
                tilex = tile->tilex;
                tiley = tile->tiley;

                g_debug("%s(%s, %d, %d, %d): %s", G_STRFUNC,
                        tile->repo->name, tile->zoom, tile->tilex, tile->tiley,
                        mut->pixbuf ? "Success" : "Failed");
                g_idle_add_full(G_PRIORITY_HIGH_IDLE,
                                (GSourceFunc)map_update_task_completed, mut,
                                NULL);
                notification_sent = TRUE;

                /* DO NOT USE mut FROM THIS POINT ON. */

                /* Also attempt to add to the database. */
                if (bytes &&
                    !mapdb_update(repo, zoom, tilex, tiley, bytes, size)) {
                    g_idle_add((GSourceFunc)map_handle_error,
                               _("Error saving map to disk - disk full?"));
                }

                /* Success! */
                g_free(bytes);
            }
        }

        if (!notification_sent)
            g_idle_add_full(G_PRIORITY_HIGH_IDLE,
                            (GSourceFunc)map_update_task_completed, mut, NULL);
    }

    if (!_conic_is_connected)
    {
        /* borrow error from glib fileutils */
        GError error = { G_FILE_ERROR, G_FILE_ERROR_FAULT, "Disconnected" };

        /* Abort all tasks */
        map_update_task_remove_all(&error);
        return FALSE;
    }

    return FALSE;
}

guint
mut_exists_hashfunc(gconstpointer a)
{
    const MapUpdateTask *t = a;
    const MapTileSpec *r = &t->tile;
    return r->zoom + r->tilex + r->tiley + t->update_type + t->layer_level;
}

gboolean
mut_exists_equalfunc(gconstpointer a, gconstpointer b)
{
    const MapUpdateTask *t1 = a;
    const MapUpdateTask *t2 = b;
    return (t1->tile.tilex == t2->tile.tilex
            && t1->tile.tiley == t2->tile.tiley
            && t1->tile.zoom == t2->tile.zoom
            && t1->update_type == t2->update_type
            && t1->layer_level == t2->layer_level);
}

gint
mut_priority_comparefunc(gconstpointer _a, gconstpointer _b)
{
    const MapUpdateTask *a = _a;
    const MapUpdateTask *b = _b;
    /* The update_type enum is sorted in order of ascending priority. */
    gint diff = (b->update_type - a->update_type);
    if(diff)
        return diff;
    diff = (a->priority - b->priority); /* Lower priority numbers first. */
    if(diff)
        return diff;
    diff = (a->layer_level - b->layer_level); /* Lower layers first. */
    if(diff)
        return diff;

    /* At this point, we don't care, so just pick arbitrarily. */
    diff = (a->tile.tilex - b->tile.tilex);
    if(diff)
        return diff;
    diff = (a->tile.tiley - b->tile.tiley);
    if(diff)
        return diff;
    return (a->tile.zoom - b->tile.zoom);
}

static gboolean
repoman_dialog_select(GtkWidget *widget, RepoManInfo *rmi)
{
    printf("%s()\n", __PRETTY_FUNCTION__);
    gint curr_index = gtk_combo_box_get_active(GTK_COMBO_BOX(rmi->cmb_repos));
    gtk_notebook_set_current_page(GTK_NOTEBOOK(rmi->notebook), curr_index);
    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
repoman_dialog_browse(GtkWidget *widget, BrowseInfo *browse_info)
{
    GtkWidget *dialog;
    gchar *basename;
    printf("%s()\n", __PRETTY_FUNCTION__);

    dialog = GTK_WIDGET(
            hildon_file_chooser_dialog_new(GTK_WINDOW(browse_info->dialog),
            GTK_FILE_CHOOSER_ACTION_SAVE));

    gtk_file_chooser_set_uri(GTK_FILE_CHOOSER(dialog),
            gtk_entry_get_text(GTK_ENTRY(browse_info->txt)));

    /* Work around a bug in HildonFileChooserDialog. */
    basename = g_path_get_basename(
            gtk_entry_get_text(GTK_ENTRY(browse_info->txt)));
    g_object_set(G_OBJECT(dialog), "autonaming", FALSE, NULL);
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), basename);

    if(GTK_RESPONSE_OK == gtk_dialog_run(GTK_DIALOG(dialog)))
    {
        gchar *filename = gtk_file_chooser_get_filename(
                GTK_FILE_CHOOSER(dialog));
        gtk_entry_set_text(GTK_ENTRY(browse_info->txt), filename);
        g_free(filename);
    }

    gtk_widget_destroy(dialog);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
repoman_compact_complete_idle(CompactInfo *ci)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    gtk_widget_destroy(GTK_WIDGET(ci->banner));
    popup_error(ci->dialog, ci->status_msg);
    gtk_widget_destroy(ci->dialog);
    g_free(ci);

    vprintf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
    return FALSE;
}

static void
thread_repoman_compact(CompactInfo *ci)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(ci->is_sqlite)
    {
        sqlite3 *db;
        if(SQLITE_OK != (sqlite3_open(ci->db_filename, &db)))
            ci->status_msg = _("Failed to open map database for compacting.");
        else
        {
            if(SQLITE_OK != sqlite3_exec(db, "vacuum;", NULL, NULL, NULL))
                ci->status_msg = _("An error occurred while trying to "
                            "compact the database.");
            else
                ci->status_msg = _("Successfully compacted database.");
            sqlite3_close(db);
        }
    }
    else
    {
        GDBM_FILE db;
        if(!(db = gdbm_open((gchar*)ci->db_filename, 0, GDBM_WRITER | GDBM_FAST,
                        0644, NULL)))
            ci->status_msg = _("Failed to open map database for compacting.");
        else
        {
            if(gdbm_reorganize(db))
                ci->status_msg = _("An error occurred while trying to "
                            "compact the database.");
            else
                ci->status_msg = _("Successfully compacted database.");
            gdbm_close(db);
        }
    }

    g_idle_add((GSourceFunc)repoman_compact_complete_idle, ci);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

static void
repoman_dialog_compact(GtkWidget *widget, RepoEditInfo *rei)
{
    CompactInfo *ci;
    GtkWidget *sw;
    printf("%s()\n", __PRETTY_FUNCTION__);

    ci = g_new0(CompactInfo, 1);

    ci->dialog = gtk_dialog_new_with_buttons(_("Compact Database"),
            GTK_WINDOW(rei->browse_info.dialog), GTK_DIALOG_MODAL,
            GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
            GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
            NULL);

    sw = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw),
            GTK_SHADOW_ETCHED_IN);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
            GTK_POLICY_NEVER,
            GTK_POLICY_ALWAYS);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(ci->dialog)->vbox),
            sw, TRUE, TRUE, 0);

    gtk_container_add(GTK_CONTAINER(sw), ci->txt = gtk_text_view_new());
    gtk_text_view_set_editable(GTK_TEXT_VIEW(ci->txt), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(ci->txt), FALSE);
    gtk_text_buffer_set_text(
            gtk_text_view_get_buffer(GTK_TEXT_VIEW(ci->txt)),
            _("Generally, deleted maps create an empty space in the "
                "database that is later reused when downloading new maps.  "
                "Compacting the database reorganizes it such that all "
                "that blank space is eliminated.  This is the only way "
                "that the size of the database can decrease.\n"
                "This reorganization requires creating a new file and "
                "inserting all the maps in the old database file into the "
                "new file. The new file is then renamed to the same name "
                "as the old file and dbf is updated to contain all the "
                "correct information about the new file.  Note that this "
                "can require free space on disk of an amount up to the size "
                "of the map database.\n"
                "This process may take several minutes, especially if "
                "your map database is large.  As a rough estimate, you can "
                "expect to wait approximately 2-5 seconds per megabyte of "
                "map data (34-85 minutes per gigabyte).  There is no progress "
                "indicator, although you can watch the new file grow in any "
                "file manager.  Do not attempt to close Maemo Mapper while "
                "the compacting operation is in progress."),
            -1);
    {
        GtkTextIter iter;
        gtk_text_buffer_get_iter_at_offset(
                gtk_text_view_get_buffer(GTK_TEXT_VIEW(ci->txt)),
                &iter, 0);
        gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(ci->txt),
                &iter, 0.0, FALSE, 0, 0);
    }

    gtk_widget_set_size_request(GTK_WIDGET(sw), 600, 200);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(ci->txt), GTK_WRAP_WORD);

    gtk_widget_show_all(ci->dialog);

    if(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(ci->dialog)))
    {
        gtk_widget_set_sensitive(GTK_DIALOG(ci->dialog)->action_area, FALSE);
        ci->db_filename = gtk_entry_get_text(GTK_ENTRY(rei->txt_db_filename));
        ci->is_sqlite = rei->is_sqlite;
        ci->banner = hildon_banner_show_animation(ci->dialog, NULL,
                _("Compacting database..."));

        g_thread_create((GThreadFunc)thread_repoman_compact, ci, FALSE, NULL);
    }
    else
    {
        gtk_widget_destroy(ci->dialog);
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

static gboolean
repoman_dialog_rename(GtkWidget *widget, RepoManInfo *rmi)
{
    static GtkWidget *hbox = NULL;
    static GtkWidget *label = NULL;
    static GtkWidget *txt_name = NULL;
    static GtkWidget *dialog = NULL;
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(dialog == NULL)
    {
        dialog = gtk_dialog_new_with_buttons(_("New Name"),
                GTK_WINDOW(rmi->dialog), GTK_DIALOG_MODAL,
                GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                NULL);

        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
                hbox = gtk_hbox_new(FALSE, 4), FALSE, FALSE, 4);

        gtk_box_pack_start(GTK_BOX(hbox),
                label = gtk_label_new(_("Name")),
                FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox),
                txt_name = gtk_entry_new(),
                TRUE, TRUE, 0);
    }

    {
        gint active = gtk_combo_box_get_active(GTK_COMBO_BOX(rmi->cmb_repos));
        RepoEditInfo *rei = g_list_nth_data(rmi->repo_edits, active);
        gtk_entry_set_text(GTK_ENTRY(txt_name), rei->name);
    }

    gtk_widget_show_all(dialog);

    while(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(dialog)))
    {
        gint active = gtk_combo_box_get_active(GTK_COMBO_BOX(rmi->cmb_repos));
        RepoEditInfo *rei = g_list_nth_data(rmi->repo_edits, active);
        g_free(rei->name);
        rei->name = g_strdup(gtk_entry_get_text(GTK_ENTRY(txt_name)));
        gtk_combo_box_insert_text(GTK_COMBO_BOX(rmi->cmb_repos),
                active, g_strdup(rei->name));
        gtk_combo_box_set_active(GTK_COMBO_BOX(rmi->cmb_repos), active);
        gtk_combo_box_remove_text(GTK_COMBO_BOX(rmi->cmb_repos), active + 1);
        break;
    }

    gtk_widget_hide(dialog);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static void
repoman_delete(RepoManInfo *rmi, gint index)
{
    gtk_combo_box_remove_text(GTK_COMBO_BOX(rmi->cmb_repos), index);
    gtk_notebook_remove_page(GTK_NOTEBOOK(rmi->notebook), index);
    rmi->repo_edits = g_list_remove_link(
            rmi->repo_edits,
            g_list_nth(rmi->repo_edits, index));
}

static gboolean
repoman_dialog_delete(GtkWidget *widget, RepoManInfo *rmi, gint index)
{
    gchar buffer[100];
    GtkWidget *confirm;
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(gtk_tree_model_iter_n_children(GTK_TREE_MODEL(
                    gtk_combo_box_get_model(GTK_COMBO_BOX(rmi->cmb_repos))),
                                NULL) <= 1)
    {
        popup_error(rmi->dialog,
                _("Cannot delete the last repository - there must be at"
                " lease one repository."));
        vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
        return TRUE;
    }

    snprintf(buffer, sizeof(buffer), "%s:\n%s\n",
            _("Confirm delete of repository"),
            gtk_combo_box_get_active_text(GTK_COMBO_BOX(rmi->cmb_repos)));

    confirm = hildon_note_new_confirmation(GTK_WINDOW(rmi->dialog),buffer);

    if(GTK_RESPONSE_OK == gtk_dialog_run(GTK_DIALOG(confirm)))
    {
        gint active = gtk_combo_box_get_active(GTK_COMBO_BOX(rmi->cmb_repos));
        repoman_delete(rmi, active);
        gtk_combo_box_set_active(GTK_COMBO_BOX(rmi->cmb_repos),
                MAX(0, index - 1));
    }

    gtk_widget_destroy(confirm);

    return TRUE;
}

static RepoEditInfo*
repoman_dialog_add_repo(RepoManInfo *rmi, gchar *name, gboolean is_sqlite)
{
    GtkWidget *vbox;
    GtkWidget *table;
    GtkWidget *label;
    GtkWidget *hbox;
    RepoEditInfo *rei = g_new0(RepoEditInfo, 1);
    printf("%s(%s, %d)\n", __PRETTY_FUNCTION__, name, is_sqlite);

    rei->name = name;
    rei->is_sqlite = is_sqlite;

    /* Maps page. */
    gtk_notebook_append_page(GTK_NOTEBOOK(rmi->notebook),
            vbox = gtk_vbox_new(FALSE, 4),
            gtk_label_new(name));

    /* Prevent destruction of notebook page, because the destruction causes
     * a seg fault (!?!?) */
    gtk_object_ref(GTK_OBJECT(vbox));

    gtk_box_pack_start(GTK_BOX(vbox),
            table = gtk_table_new(2, 2, FALSE),
            FALSE, FALSE, 0);
    /* Map download URI. */
    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new(_("URL Format")),
            0, 1, 0, 1, GTK_FILL, 0, 2, 0);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(table),
            rei->txt_url = gtk_entry_new(),
            1, 2, 0, 1, GTK_EXPAND | GTK_FILL, 0, 2, 0);

    /* Map Directory. */
    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new(_("Cache DB")),
            0, 1, 1, 2, GTK_FILL, 0, 2, 0);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(table),
            hbox = gtk_hbox_new(FALSE, 4),
            1, 2, 1, 2, GTK_EXPAND | GTK_FILL, 0, 2, 0);
    gtk_box_pack_start(GTK_BOX(hbox),
            rei->txt_db_filename = gtk_entry_new(),
            TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox),
            rei->btn_browse = gtk_button_new_with_label(_("Browse...")),
            FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox),
            rei->btn_compact = gtk_button_new_with_label(_("Compact...")),
            FALSE, FALSE, 0);

    /* Initialize cache dir */
    {
        gchar buffer[BUFFER_SIZE];
        snprintf(buffer, sizeof(buffer), "%s.%s", name,
                is_sqlite ? "sqlite" : "gdbm");
        gchar *db_base = gnome_vfs_expand_initial_tilde(
                REPO_DEFAULT_CACHE_BASE);
        gchar *db_filename = gnome_vfs_uri_make_full_from_relative(
                db_base, buffer);
        gtk_entry_set_text(GTK_ENTRY(rei->txt_db_filename), db_filename);
        g_free(db_filename);
        g_free(db_base);
    }

    gtk_box_pack_start(GTK_BOX(vbox),
            table = gtk_table_new(3, 2, FALSE),
            FALSE, FALSE, 0);

    /* Download Zoom Steps. */
    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new(_("Download Zoom Steps")),
            0, 1, 0, 1, GTK_FILL, 0, 2, 0);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(table),
            label = gtk_alignment_new(0.f, 0.5f, 0.f, 0.f),
            1, 2, 0, 1, GTK_FILL, 0, 2, 0);
    gtk_container_add(GTK_CONTAINER(label),
            rei->num_dl_zoom_steps = hildon_controlbar_new());
    hildon_controlbar_set_range(
            HILDON_CONTROLBAR(rei->num_dl_zoom_steps), 1, 4);
    hildon_controlbar_set_value(HILDON_CONTROLBAR(rei->num_dl_zoom_steps),
            REPO_DEFAULT_DL_ZOOM_STEPS);
    force_min_visible_bars(HILDON_CONTROLBAR(rei->num_dl_zoom_steps), 1);

    /* Download Zoom Steps. */
    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new(_("View Zoom Steps")),
            0, 1, 1, 2, GTK_FILL, 0, 2, 0);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(table),
            label = gtk_alignment_new(0.f, 0.5f, 0.f, 0.f),
            1, 2, 1, 2, GTK_FILL, 0, 2, 0);
    gtk_container_add(GTK_CONTAINER(label),
            rei->num_view_zoom_steps = hildon_controlbar_new());
    hildon_controlbar_set_range(
            HILDON_CONTROLBAR(rei->num_view_zoom_steps), 1, 4);
    hildon_controlbar_set_value(HILDON_CONTROLBAR(rei->num_view_zoom_steps),
            REPO_DEFAULT_VIEW_ZOOM_STEPS);
    force_min_visible_bars(HILDON_CONTROLBAR(rei->num_view_zoom_steps), 1);

    gtk_table_attach(GTK_TABLE(table),
            label = gtk_vseparator_new(),
            2, 3, 0, 2, GTK_FILL, GTK_FILL, 4, 0);

    /* Double-size. */
    gtk_table_attach(GTK_TABLE(table),
            rei->chk_double_size = gtk_check_button_new_with_label(
                _("Double Pixels")),
            3, 4, 0, 1, GTK_FILL, GTK_FILL, 0, 0);
    gtk_toggle_button_set_active(
            GTK_TOGGLE_BUTTON(rei->chk_double_size), FALSE);

    /* Next-able */
    gtk_table_attach(GTK_TABLE(table),
            rei->chk_nextable = gtk_check_button_new_with_label(
                _("Next-able")),
            3, 4, 1, 2, GTK_FILL, GTK_FILL, 0, 0);
    gtk_toggle_button_set_active(
            GTK_TOGGLE_BUTTON(rei->chk_nextable), TRUE);

    /* Downloadable Zoom Levels. */
    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new(_("Downloadable Zooms:")),
            0, 1, 2, 3, GTK_FILL, 0, 2, 0);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(table),
            label = gtk_alignment_new(0.f, 0.5f, 0.f, 0.f),
            1, 4, 2, 3, GTK_FILL, 0, 2, 0);
    gtk_container_add(GTK_CONTAINER(label),
            hbox = gtk_hbox_new(FALSE, 4));
    gtk_box_pack_start(GTK_BOX(hbox),
            label = gtk_label_new(_("Min.")),
            TRUE, TRUE, 0);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_box_pack_start(GTK_BOX(hbox),
            rei->num_min_zoom = hildon_number_editor_new(MIN_ZOOM, MAX_ZOOM),
            FALSE, FALSE, 0);
    hildon_number_editor_set_value(HILDON_NUMBER_EDITOR(rei->num_min_zoom), 4);
    gtk_box_pack_start(GTK_BOX(hbox),
            label = gtk_label_new(""),
            TRUE, TRUE, 4);
    gtk_box_pack_start(GTK_BOX(hbox),
            label = gtk_label_new(_("Max.")),
            TRUE, TRUE, 0);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_box_pack_start(GTK_BOX(hbox),
            rei->num_max_zoom = hildon_number_editor_new(MIN_ZOOM, MAX_ZOOM),
            FALSE, FALSE, 0);
    hildon_number_editor_set_value(HILDON_NUMBER_EDITOR(rei->num_max_zoom),20);

    rmi->repo_edits = g_list_append(rmi->repo_edits, rei);

    /* Connect signals. */
    rei->browse_info.dialog = rmi->dialog;
    rei->browse_info.txt = rei->txt_db_filename;
    g_signal_connect(G_OBJECT(rei->btn_browse), "clicked",
                      G_CALLBACK(repoman_dialog_browse),
                      &rei->browse_info);
    g_signal_connect(G_OBJECT(rei->btn_compact), "clicked",
                      G_CALLBACK(repoman_dialog_compact),
                      rei);

    gtk_widget_show_all(vbox);

    gtk_combo_box_append_text(GTK_COMBO_BOX(rmi->cmb_repos), name);
    gtk_combo_box_set_active(GTK_COMBO_BOX(rmi->cmb_repos),
            gtk_tree_model_iter_n_children(GTK_TREE_MODEL(
                    gtk_combo_box_get_model(GTK_COMBO_BOX(rmi->cmb_repos))),
                NULL) - 1);

    /* newly created repos keep this NULL in rei, indicating
       that layes cannot be added so far */
    rei->repo = NULL;

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return rei;
}

static gboolean
repoman_dialog_new(GtkWidget *widget, RepoManInfo *rmi)
{
    static GtkWidget *table = NULL;
    static GtkWidget *label = NULL;
    static GtkWidget *txt_name = NULL;
    static GtkWidget *cmb_type = NULL;
    static GtkWidget *dialog = NULL;
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(dialog == NULL)
    {
        dialog = gtk_dialog_new_with_buttons(_("New Repository"),
                GTK_WINDOW(rmi->dialog), GTK_DIALOG_MODAL,
                GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                NULL);

        /* Enable the help button. */
#ifndef LEGACY
#else
        ossohelp_dialog_help_enable(
                GTK_DIALOG(dialog), HELP_ID_NEWREPO, _osso);
#endif

        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
                table = gtk_table_new(2, 2, FALSE),
                FALSE, FALSE, 0);

        /* Download Zoom Steps. */
        gtk_table_attach(GTK_TABLE(table),
                label = gtk_label_new(_("Name")),
                0, 1, 0, 1, GTK_FILL, 2, 4, 2);
        gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);

        gtk_table_attach(GTK_TABLE(table),
                txt_name = gtk_entry_new(),
                1, 2, 0, 1, GTK_FILL, 2, 4, 2);

        gtk_table_attach(GTK_TABLE(table),
                label = gtk_label_new(_("Type")),
                0, 1, 1, 2, GTK_FILL, 2, 4, 2);
        gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);

        gtk_table_attach(GTK_TABLE(table),
                cmb_type = gtk_combo_box_new_text(),
                1, 2, 1, 2, GTK_FILL, 2, 4, 2);

        gtk_combo_box_append_text(GTK_COMBO_BOX(cmb_type),
                _("SQLite 3 (default)"));
        gtk_combo_box_append_text(GTK_COMBO_BOX(cmb_type),
                _("GDBM (legacy)"));
        gtk_combo_box_set_active(GTK_COMBO_BOX(cmb_type), 0);
    }

    gtk_entry_set_text(GTK_ENTRY(txt_name), "");

    gtk_widget_show_all(dialog);

    while(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(dialog)))
    {
        repoman_dialog_add_repo(rmi,
                g_strdup(gtk_entry_get_text(GTK_ENTRY(txt_name))),
                gtk_combo_box_get_active(GTK_COMBO_BOX(cmb_type)) == 0);
        break;
    }

    gtk_widget_hide(dialog);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
repoman_reset(GtkWidget *widget, RepoManInfo *rmi)
{
    GtkWidget *confirm;
    printf("%s()\n", __PRETTY_FUNCTION__);

    confirm = hildon_note_new_confirmation(GTK_WINDOW(rmi->dialog),
            _("Replace all repositories with the default repository?"));

    if(GTK_RESPONSE_OK == gtk_dialog_run(GTK_DIALOG(confirm)))
    {
        /* First, delete all existing repositories. */
        while(rmi->repo_edits)
            repoman_delete(rmi, 0);

        /* Now, add the default repository. */
        repoman_dialog_add_repo(rmi, REPO_DEFAULT_NAME, TRUE);
        gtk_entry_set_text(
                GTK_ENTRY(((RepoEditInfo*)rmi->repo_edits->data)->txt_url),
                REPO_DEFAULT_MAP_URI);

        gtk_combo_box_set_active(GTK_COMBO_BOX(rmi->cmb_repos), 0);
    }
    gtk_widget_destroy(confirm);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

gboolean
repoman_download()
{
    GtkWidget *confirm;
    printf("%s()\n", __PRETTY_FUNCTION__);

    confirm = hildon_note_new_confirmation(GTK_WINDOW(_window),
            _("Maemo Mapper will now download and add a list of "
                "possibly-duplicate repositories from the internet.  "
                "Continue?"));

    if(GTK_RESPONSE_OK == gtk_dialog_run(GTK_DIALOG(confirm)))
    {
        gchar *bytes;
        gchar *head;
        gchar *tail;
        gint size;
        GnomeVFSResult vfs_result;
        printf("%s()\n", __PRETTY_FUNCTION__);

        /* Get repo config file from www.gnuite.com. */
        if(GNOME_VFS_OK != (vfs_result = gnome_vfs_read_entire_file(
                    "http://www.gnuite.com/nokia770/maemo-mapper/"
                    "repos-with-layers.txt",
                    &size, &bytes)))
        {
            popup_error(_window,
                    _("An error occurred while retrieving the repositories.  "
                        "The web service may be temporarily down."));
            g_printerr("Error while download repositories: %s\n",
                    gnome_vfs_result_to_string(vfs_result));
        }
        /* Parse each line as a reposotory. */
        else
        {
            RepoData *prev_repo = NULL;
            menu_maps_remove_repos();
            for(head = bytes; head && *head; head = tail)
            {
                RepoData *rd;
                tail = strchr(head, '\n');
                *tail++ = '\0';

                rd = settings_parse_repo(head);
                if (rd->layer_level == 0) {
                    _repo_list = g_list_append(_repo_list, rd);
                }
                else
                    prev_repo->layers = rd;

                prev_repo = rd;
            }
            g_free(bytes);
            menu_maps_add_repos();
            settings_save();
        }
    }
    gtk_widget_destroy(confirm);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}


static gint
layer_get_page_index (RepoLayersInfo *rli, GtkTreeIter list_it)
{
    GtkTreePath *p1, *p2;
    GtkTreeIter p;
    gint index = 0;

    gtk_tree_model_get_iter_first (GTK_TREE_MODEL (rli->layers_store), &p);

    p1 = gtk_tree_model_get_path (GTK_TREE_MODEL (rli->layers_store), &list_it);
    p2 = gtk_tree_model_get_path (GTK_TREE_MODEL (rli->layers_store), &p);

    while (gtk_tree_path_compare (p1, p2) != 0) {
        gtk_tree_path_next (p2);
        index++;
    }

    gtk_tree_path_free (p1);
    gtk_tree_path_free (p2);

    return index;
}


static gboolean
layer_name_changed (GtkWidget *entry, LayerEditInfo *lei)
{
    const gchar* name;
    GtkTreeSelection *selection;
    GtkTreeIter iter;

    printf("%s()\n", __PRETTY_FUNCTION__);

    /* take new name  */
    name = gtk_entry_get_text (GTK_ENTRY (entry));

    /* find selected entry in list view */
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (lei->rli->layers_list));

    if (!gtk_tree_selection_get_selected (selection, NULL, &iter)) {
        vprintf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
        return FALSE;
    }

    gtk_list_store_set (lei->rli->layers_store, &iter, 0, name, -1);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}


static gboolean
layer_dialog_browse (GtkWidget *widget, LayerEditInfo *lei)
{
    GtkWidget *dialog;
    gchar *basename;
    printf("%s()\n", __PRETTY_FUNCTION__);

    dialog = GTK_WIDGET(
            hildon_file_chooser_dialog_new(GTK_WINDOW(lei->rli->dialog),
            GTK_FILE_CHOOSER_ACTION_SAVE));

    gtk_file_chooser_set_uri(GTK_FILE_CHOOSER(dialog),
            gtk_entry_get_text(GTK_ENTRY(lei->txt_db)));

    /* Work around a bug in HildonFileChooserDialog. */
    basename = g_path_get_basename(
            gtk_entry_get_text(GTK_ENTRY(lei->txt_db)));
    g_object_set(G_OBJECT(dialog), "autonaming", FALSE, NULL);
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), basename);

    if(GTK_RESPONSE_OK == gtk_dialog_run(GTK_DIALOG(dialog)))
    {
        gchar *filename = gtk_file_chooser_get_filename(
                GTK_FILE_CHOOSER(dialog));
        gtk_entry_set_text(GTK_ENTRY(lei->txt_db), filename);
        g_free(filename);
    }

    gtk_widget_destroy(dialog);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}



static LayerEditInfo*
repoman_layers_add_layer (RepoLayersInfo *rli, gchar* name)
{
    LayerEditInfo *lei = g_new (LayerEditInfo, 1);
    GtkWidget *vbox;
    GtkWidget *hbox2;
    GtkWidget *table;
    GtkWidget *label;
    GtkWidget *btn_browse;
    GtkTreeIter layers_iter;
    
    printf("%s(%s)\n", __PRETTY_FUNCTION__, name);

    lei->rli = rli;

    rli->layer_edits = g_list_append (rli->layer_edits, lei);

    gtk_notebook_append_page (GTK_NOTEBOOK (rli->notebook), vbox = gtk_vbox_new (FALSE, 4),
                              gtk_label_new (name));

    gtk_box_pack_start (GTK_BOX (vbox), table = gtk_table_new (4, 2, FALSE),
                        FALSE, FALSE, 0);

    /* Layer name */
    gtk_table_attach (GTK_TABLE (table), label = gtk_label_new (_("Name")),
                      0, 1, 0, 1, GTK_FILL, 0, 2, 0);
    gtk_misc_set_alignment (GTK_MISC (label), 1.f, 0.5f);
    gtk_table_attach (GTK_TABLE (table), lei->txt_name = gtk_entry_new (),
                      1, 2, 0, 1, GTK_EXPAND | GTK_FILL, 0, 2, 0);
    gtk_entry_set_text (GTK_ENTRY (lei->txt_name), name);

    /* signals */
    g_signal_connect(G_OBJECT(lei->txt_name), "changed", G_CALLBACK(layer_name_changed), lei);

    /* URL format */
    gtk_table_attach (GTK_TABLE (table), label = gtk_label_new (_("URL")),
                      0, 1, 1, 2, GTK_FILL, 0, 2, 0);
    gtk_misc_set_alignment (GTK_MISC (label), 1.f, 0.5f);
    gtk_table_attach (GTK_TABLE (table), lei->txt_url = gtk_entry_new (),
                      1, 2, 1, 2, GTK_EXPAND | GTK_FILL, 0, 2, 0);

    /* Map directory */
    gtk_table_attach (GTK_TABLE (table), label = gtk_label_new (_("Cache DB")),
                      0, 1, 2, 3, GTK_FILL, 0, 2, 0);
    gtk_misc_set_alignment (GTK_MISC (label), 1.f, 0.5f);
    gtk_table_attach (GTK_TABLE (table), hbox2 = gtk_hbox_new (FALSE, 4),
                      1, 2, 2, 3, GTK_EXPAND | GTK_FILL, 0, 2, 0);
    gtk_box_pack_start (GTK_BOX (hbox2), lei->txt_db = gtk_entry_new (),
                        TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (hbox2), btn_browse = gtk_button_new_with_label (_("Browse...")),
                        FALSE, FALSE, 0);

    g_signal_connect(G_OBJECT(btn_browse), "clicked", G_CALLBACK(layer_dialog_browse), lei);

    /* Autorefresh */
    gtk_table_attach (GTK_TABLE (table), label = gtk_label_new (_("Autofetch")),
                      0, 1, 3, 4, GTK_FILL, 0, 2, 0);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_table_attach (GTK_TABLE (table), hbox2 = gtk_hbox_new (FALSE, 4),
                      1, 2, 3, 4, GTK_EXPAND | GTK_FILL, 0, 2, 0);
    gtk_box_pack_start (GTK_BOX (hbox2), lei->num_autofetch = hildon_number_editor_new (0, 120),
                        FALSE, FALSE, 4);
    gtk_box_pack_start (GTK_BOX (hbox2), label = gtk_label_new (_("min.")), FALSE, FALSE, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);

    /* Visible */
    gtk_box_pack_start (GTK_BOX (vbox), lei->chk_visible = gtk_check_button_new_with_label (_("Layer is visible")),
                        FALSE, FALSE, 4);

    gtk_widget_show_all (vbox);

    /* Side list view with layers */
    gtk_list_store_append (rli->layers_store, &layers_iter);
    gtk_list_store_set (rli->layers_store, &layers_iter, 0, name, -1);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);

    return lei;
}



static gboolean
repoman_layers_new (GtkWidget *widget, RepoLayersInfo *rli)
{
    static GtkWidget *hbox = NULL;
    static GtkWidget *label = NULL;
    static GtkWidget *txt_name = NULL;
    static GtkWidget *dialog = NULL;
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(dialog == NULL)
    {
        dialog = gtk_dialog_new_with_buttons(_("New Layer"),
                GTK_WINDOW(rli->dialog), GTK_DIALOG_MODAL,
                GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                NULL);

        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
                hbox = gtk_hbox_new(FALSE, 4), FALSE, FALSE, 4);

        gtk_box_pack_start(GTK_BOX(hbox),
                label = gtk_label_new(_("Name")),
                FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox),
                txt_name = gtk_entry_new(),
                TRUE, TRUE, 0);
    }

    gtk_entry_set_text(GTK_ENTRY(txt_name), "");

    gtk_widget_show_all(dialog);

    while(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(dialog)))
    {
        repoman_layers_add_layer(rli,
                g_strdup(gtk_entry_get_text(GTK_ENTRY(txt_name))));
        break;
    }

    gtk_widget_hide(dialog);
    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}


static gboolean
repoman_layers_del (GtkWidget *widget, RepoLayersInfo *rli)
{
    GtkTreeIter iter;
    GtkTreeSelection *selection;
    gint index;

    printf("%s()\n", __PRETTY_FUNCTION__);

    /* delete list item */
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (rli->layers_list));

    if (!gtk_tree_selection_get_selected (selection, NULL, &iter)) {
        vprintf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
        return FALSE;
    }

    index = layer_get_page_index (rli, iter);
    gtk_list_store_remove (rli->layers_store, &iter);

    rli->layer_edits = g_list_remove_link (rli->layer_edits, g_list_nth (rli->layer_edits, index));

    /* delete notebook page */
    gtk_notebook_remove_page (GTK_NOTEBOOK (rli->notebook), index);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}


static gboolean
repoman_layers_up (GtkWidget *widget, RepoLayersInfo *rli)
{
    GtkTreeSelection *selection;
    GtkTreeIter iter, iter2;
    GtkTreePath *path;
    gint page;
    LayerEditInfo *lei;
    GList *list_elem;

    printf("%s()\n", __PRETTY_FUNCTION__);

    /* find selected entry in list view */
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (rli->layers_list));

    if (!gtk_tree_selection_get_selected (selection, NULL, &iter)) {
        vprintf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
        return FALSE;
    }

    iter2 = iter;
    path = gtk_tree_model_get_path (GTK_TREE_MODEL (rli->layers_store), &iter);
    if (!gtk_tree_path_prev (path) || !gtk_tree_model_get_iter (GTK_TREE_MODEL (rli->layers_store), &iter, path)) {
        vprintf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
        return FALSE;
    }

    gtk_tree_path_free (path);

    /* move it up */
    gtk_list_store_move_before (rli->layers_store, &iter2, &iter);

    /* reorder notebook tabs */
    page = gtk_notebook_get_current_page (GTK_NOTEBOOK (rli->notebook));
    gtk_notebook_reorder_child (GTK_NOTEBOOK (rli->notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (rli->notebook), page), page-1);

    /* reorder layer edits */
    list_elem = g_list_nth (rli->layer_edits, page);
    lei = list_elem->data;
    rli->layer_edits = g_list_remove_link (rli->layer_edits, list_elem);
    rli->layer_edits = g_list_insert (rli->layer_edits, lei, page-1);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}


static gboolean
repoman_layers_dn (GtkWidget *widget, RepoLayersInfo *rli)
{
    GtkTreeSelection *selection;
    GtkTreeIter iter, iter2;
    gint page;
    LayerEditInfo *lei;
    GList *list_elem;

    printf("%s()\n", __PRETTY_FUNCTION__);

    /* find selected entry in list view */
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (rli->layers_list));

    if (!gtk_tree_selection_get_selected (selection, NULL, &iter)) {
        vprintf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
        return FALSE;
    }

    iter2 = iter;
    if (!gtk_tree_model_iter_next (GTK_TREE_MODEL (rli->layers_store), &iter)) {
        vprintf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
        return FALSE;
    }

    /* move it down */
    gtk_list_store_move_after (rli->layers_store, &iter2, &iter);

    /* reorder notebook tabs */
    page = gtk_notebook_get_current_page (GTK_NOTEBOOK (rli->notebook));
    gtk_notebook_reorder_child (GTK_NOTEBOOK (rli->notebook), gtk_notebook_get_nth_page (GTK_NOTEBOOK (rli->notebook), page), page+1);

    /* reorder layer edits */
    list_elem = g_list_nth (rli->layer_edits, page);
    lei = list_elem->data;
    rli->layer_edits = g_list_remove_link (rli->layer_edits, list_elem);
    rli->layer_edits = g_list_insert (rli->layer_edits, lei, page+1);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}


static gboolean
repoman_layer_selected (GtkTreeSelection *selection, RepoLayersInfo *rli)
{
    GtkTreeIter cur;

    printf("%s()\n", __PRETTY_FUNCTION__);

    if (!gtk_tree_selection_get_selected (selection, NULL, &cur)) {
        vprintf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
        return FALSE;
    }

    gtk_notebook_set_current_page (GTK_NOTEBOOK (rli->notebook), layer_get_page_index (rli, cur));

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}


static gboolean
repoman_layers(GtkWidget *widget, RepoManInfo *rmi)
{
    GtkWidget *hbox = NULL;
    GtkWidget *layers_vbox = NULL;
    GtkWidget *buttons_hbox = NULL;
    GtkWidget *frame;
    GtkCellRenderer *layers_rendeder = NULL;
    GtkTreeViewColumn *layers_column = NULL;
    GtkTreeSelection *selection;

    /* layers buttons */
    GtkWidget *btn_new = NULL;
    GtkWidget *btn_del = NULL;
    GtkWidget *btn_up = NULL;
    GtkWidget *btn_dn = NULL;

    const char* t_header = _("Manage layers [%s]");
    char* header = NULL;
    RepoEditInfo* rei = NULL;
    RepoLayersInfo rli;
    gint curr_repo_index = gtk_combo_box_get_active (GTK_COMBO_BOX (rmi->cmb_repos));
    RepoData *rd;

    printf("%s()\n", __PRETTY_FUNCTION__);

    if (curr_repo_index < 0) {
        vprintf("%s(): return FALSE (1)\n", __PRETTY_FUNCTION__);
        return FALSE;
    }

    rei = g_list_nth_data (rmi->repo_edits, curr_repo_index);

    if (!rei) {
        vprintf("%s(): return FALSE (2)\n", __PRETTY_FUNCTION__);
        return FALSE;
    }

    /* check that rei have repo data structure. If it haven't, it means that repository have just
       added, so report about this */
    if (!rei->repo) {
        GtkWidget *msg = hildon_note_new_information ( GTK_WINDOW (rmi->dialog),
                           _("You cannot add layers to not saved repository,\nsorry. So, press ok in repository manager\n"
                             "and open this dialog again."));

        gtk_dialog_run (GTK_DIALOG (msg));
        gtk_widget_destroy (msg);

        vprintf("%s(): return FALSE (3)\n", __PRETTY_FUNCTION__);
        return FALSE;
    }

    header = g_malloc (strlen (t_header) + strlen (rei->name));
    sprintf (header, t_header, rei->name);

    printf ("Creating dialog with header: %s\n", header);

    rli.layer_edits = NULL;
    rli.dialog = gtk_dialog_new_with_buttons (header, GTK_WINDOW (rmi->dialog), GTK_DIALOG_MODAL, 
                                              GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                                              GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, NULL);

    rli.layers_store = gtk_list_store_new (1, G_TYPE_STRING);
    rli.layers_list = gtk_tree_view_new_with_model (GTK_TREE_MODEL (rli.layers_store));
    layers_rendeder = gtk_cell_renderer_text_new ();
    layers_column = gtk_tree_view_column_new_with_attributes ("Column", layers_rendeder, "text", 0, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (rli.layers_list), layers_column);

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (rli.layers_list));

    frame = gtk_frame_new (NULL);
    gtk_container_add (GTK_CONTAINER (frame), rli.layers_list);
    gtk_widget_set_size_request (frame, -1, 100);

    /* beside layers list with have buttons on bottom */
    layers_vbox = gtk_vbox_new (FALSE, 4);
    gtk_box_pack_start (GTK_BOX (layers_vbox), frame, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (layers_vbox), buttons_hbox = gtk_hbox_new (FALSE, 4), FALSE, FALSE, 0);

    /* buttons */
    gtk_box_pack_start (GTK_BOX (buttons_hbox), btn_new = gtk_button_new_with_label (_("New")), FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (buttons_hbox), btn_del = gtk_button_new_with_label (_("Del")), FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (buttons_hbox), btn_up  = gtk_button_new_with_label (_("Up")), FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (buttons_hbox), btn_dn  = gtk_button_new_with_label (_("Dn")), FALSE, FALSE, 0);

    /* signals */
    g_signal_connect(G_OBJECT(selection), "changed", G_CALLBACK(repoman_layer_selected), &rli);
    g_signal_connect(G_OBJECT(btn_new), "clicked", G_CALLBACK(repoman_layers_new), &rli);
    g_signal_connect(G_OBJECT(btn_del), "clicked", G_CALLBACK(repoman_layers_del), &rli);
    g_signal_connect(G_OBJECT(btn_up),  "clicked", G_CALLBACK(repoman_layers_up), &rli);
    g_signal_connect(G_OBJECT(btn_dn),  "clicked", G_CALLBACK(repoman_layers_dn), &rli);

    /* notebook with layers' attributes */
    rli.notebook = gtk_notebook_new ();

    gtk_notebook_set_show_tabs(GTK_NOTEBOOK(rli.notebook), FALSE);
    gtk_notebook_set_show_border(GTK_NOTEBOOK(rli.notebook), FALSE);

    /* walk through all layers and add notebook pages */
    rd = rei->repo->layers;
    while (rd) {
        LayerEditInfo *lei = repoman_layers_add_layer (&rli, rd->name);

        gtk_entry_set_text (GTK_ENTRY (lei->txt_url),  rd->url);
        gtk_entry_set_text (GTK_ENTRY (lei->txt_db),   rd->db_filename);
        hildon_number_editor_set_value (HILDON_NUMBER_EDITOR (lei->num_autofetch), rd->layer_refresh_interval);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (lei->chk_visible), rd->layer_enabled);

        rd = rd->layers;
    }

    /* pack all widgets together */
    hbox = gtk_hbox_new (FALSE, 4);

    gtk_box_pack_start (GTK_BOX (hbox), layers_vbox, TRUE, TRUE, 4);
    gtk_box_pack_start (GTK_BOX (hbox), rli.notebook, TRUE, TRUE, 4);

    gtk_box_pack_start (GTK_BOX (GTK_DIALOG (rli.dialog)->vbox), hbox, FALSE, FALSE, 4);

    gtk_widget_show_all (rli.dialog);

    while (GTK_RESPONSE_ACCEPT == gtk_dialog_run (GTK_DIALOG (rli.dialog)))
    {
        RepoData **rdp;
        gint i;
        GList *curr;

        menu_layers_remove_repos ();

        /* iterate over notebook's pages and build layers */
        /* keep list in memory in case downloads use it (TODO: reference counting) */
        rdp = &rei->repo->layers;
        *rdp = NULL;

        for (i = 0, curr = rli.layer_edits; curr; curr = curr->next, i++)  {
            LayerEditInfo *lei = curr->data;

            rd = g_new0 (RepoData, 1);
            *rdp = rd;

            rd->name = g_strdup (gtk_entry_get_text (GTK_ENTRY (lei->txt_name)));
            rd->is_sqlite = rei->repo->is_sqlite;
            rd->url = g_strdup (gtk_entry_get_text (GTK_ENTRY (lei->txt_url)));
            rd->db_filename = g_strdup (gtk_entry_get_text (GTK_ENTRY (lei->txt_db)));
            rd->layer_enabled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (lei->chk_visible));
            rd->layer_refresh_interval = hildon_number_editor_get_value (HILDON_NUMBER_EDITOR (lei->num_autofetch));
            rd->layer_refresh_countdown = rd->layer_refresh_interval;
            rd->layer_level = i+1;

            rd->dl_zoom_steps = rei->repo->dl_zoom_steps;
            rd->view_zoom_steps = rei->repo->view_zoom_steps;
            rd->double_size = rei->repo->double_size;
            rd->nextable = rei->repo->nextable;
            rd->min_zoom = rei->repo->min_zoom;
            rd->max_zoom = rei->repo->max_zoom;

            set_repo_type (rd);
            rdp = &rd->layers;
        }

        menu_layers_add_repos ();
        repo_set_curr(_curr_repo);
        settings_save ();
        map_refresh_mark (TRUE);
        break;
    }

    gtk_widget_destroy (rli.dialog);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}


gboolean
repoman_dialog()
{
    static RepoManInfo rmi;
    static GtkWidget *dialog = NULL;
    static GtkWidget *hbox = NULL;
    static GtkWidget *btn_rename = NULL;
    static GtkWidget *btn_delete = NULL;
    static GtkWidget *btn_new = NULL;
    static GtkWidget *btn_reset = NULL;
    static GtkWidget *btn_layers = NULL;
    gint i, curr_repo_index = 0;
    GList *curr;
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(dialog == NULL)
    {
        rmi.dialog = dialog = gtk_dialog_new_with_buttons(
                _("Manage Repositories"),
                GTK_WINDOW(_window), GTK_DIALOG_MODAL,
                GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                NULL);

        /* Enable the help button. */
#ifndef LEGACY
#else
        ossohelp_dialog_help_enable(
                GTK_DIALOG(dialog), HELP_ID_REPOMAN, _osso);
#endif

        /* Reset button. */
        gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area),
                btn_reset = gtk_button_new_with_label(_("Reset...")));
        g_signal_connect(G_OBJECT(btn_reset), "clicked",
                          G_CALLBACK(repoman_reset), &rmi);

        /* Layers button. */
        gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area),
                btn_layers = gtk_button_new_with_label(_("Layers...")));
        g_signal_connect(G_OBJECT(btn_layers), "clicked",
                          G_CALLBACK(repoman_layers), &rmi);

        /* Cancel button. */
        gtk_dialog_add_button(GTK_DIALOG(dialog),
                GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT);

        hbox = gtk_hbox_new(FALSE, 4);

        gtk_box_pack_start(GTK_BOX(hbox),
                rmi.cmb_repos = gtk_combo_box_new_text(), TRUE, TRUE, 4);

        gtk_box_pack_start(GTK_BOX(hbox),
                gtk_vseparator_new(), FALSE, FALSE, 4);
        gtk_box_pack_start(GTK_BOX(hbox),
                btn_rename = gtk_button_new_with_label(_("Rename...")),
                FALSE, FALSE, 4);
        gtk_box_pack_start(GTK_BOX(hbox),
                btn_delete = gtk_button_new_with_label(_("Delete...")),
                FALSE, FALSE, 4);
        gtk_box_pack_start(GTK_BOX(hbox),
                btn_new = gtk_button_new_with_label(_("New...")),
                FALSE, FALSE, 4);

        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
                hbox, FALSE, FALSE, 4);

        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
                gtk_hseparator_new(), TRUE, TRUE, 4);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
                rmi.notebook = gtk_notebook_new(), TRUE, TRUE, 4);

        gtk_notebook_set_show_tabs(GTK_NOTEBOOK(rmi.notebook), FALSE);
        gtk_notebook_set_show_border(GTK_NOTEBOOK(rmi.notebook), FALSE);

        rmi.repo_edits = NULL;

        /* Connect signals. */
        g_signal_connect(G_OBJECT(btn_rename), "clicked",
                G_CALLBACK(repoman_dialog_rename), &rmi);
        g_signal_connect(G_OBJECT(btn_delete), "clicked",
                G_CALLBACK(repoman_dialog_delete), &rmi);
        g_signal_connect(G_OBJECT(btn_new), "clicked",
                G_CALLBACK(repoman_dialog_new), &rmi);
        g_signal_connect(G_OBJECT(rmi.cmb_repos), "changed",
                G_CALLBACK(repoman_dialog_select), &rmi);
    }

    /* Populate combo box and pages in notebook. */
    for(i = 0, curr = _repo_list; curr; curr = curr->next, i++)
    {
        RepoData *rd = (RepoData*)curr->data;
        RepoEditInfo *rei = repoman_dialog_add_repo(&rmi, g_strdup(rd->name),
                rd->is_sqlite);

        /* store this to be able to walk through layers attached to repo */
        rei->repo = rd;

        /* Initialize fields with data from the RepoData object. */
        gtk_entry_set_text(GTK_ENTRY(rei->txt_url), rd->url);
        gtk_entry_set_text(GTK_ENTRY(rei->txt_db_filename),
                rd->db_filename);
        hildon_controlbar_set_value(
                HILDON_CONTROLBAR(rei->num_dl_zoom_steps),
                rd->dl_zoom_steps);
        hildon_controlbar_set_value(
                HILDON_CONTROLBAR(rei->num_view_zoom_steps),
                rd->view_zoom_steps);
        gtk_toggle_button_set_active(
                GTK_TOGGLE_BUTTON(rei->chk_double_size),
                rd->double_size);
        gtk_toggle_button_set_active(
                GTK_TOGGLE_BUTTON(rei->chk_nextable),
                rd->nextable);
        hildon_number_editor_set_value(
                HILDON_NUMBER_EDITOR(rei->num_min_zoom),
                rd->min_zoom);
        hildon_number_editor_set_value(
                HILDON_NUMBER_EDITOR(rei->num_max_zoom),
                rd->max_zoom);
        if(rd == _curr_repo)
            curr_repo_index = i;
    }

    gtk_combo_box_set_active(GTK_COMBO_BOX(rmi.cmb_repos), curr_repo_index);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(rmi.notebook), curr_repo_index);

    gtk_widget_show_all(dialog);

    while(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(dialog)))
    {
        /* Iterate through repos and verify each. */
        gboolean verified = TRUE;
        gint i;
        GList *curr;
        gchar *old_curr_repo_name = _curr_repo->name;

        for(i = 0, curr = rmi.repo_edits; curr; curr = curr->next, i++)
        {
            /* Check the ranges for the min and max zoom levels. */
            RepoEditInfo *rei = curr->data;
            if(hildon_number_editor_get_value(
                        HILDON_NUMBER_EDITOR(rei->num_max_zoom))
                 < hildon_number_editor_get_value(
                        HILDON_NUMBER_EDITOR(rei->num_min_zoom)))
            {
                verified = FALSE;
                break;
            }
        }
        if(!verified)
        {
            gtk_combo_box_set_active(GTK_COMBO_BOX(rmi.cmb_repos), i);
            popup_error(dialog,
                    _("Minimum Downloadable Zoom must be less than "
                        "Maximum Downloadable Zoom."));
            continue;
        }

        /* We're good to replace.  Remove old _repo_list menu items. */
        menu_maps_remove_repos();
        /* But keep the repo list in memory, in case downloads are using it. */
        _repo_list = NULL;

        /* Write new _repo_list. */
        curr_repo_index = gtk_combo_box_get_active(
                GTK_COMBO_BOX(rmi.cmb_repos));
        _curr_repo = NULL;
        for(i = 0, curr = rmi.repo_edits; curr; curr = curr->next, i++)
        {
            RepoEditInfo *rei = curr->data;
            RepoData *rd = g_new0(RepoData, 1);
            RepoData *rd0, **rd1;
            rd->name = g_strdup(rei->name);
            rd->is_sqlite = rei->is_sqlite;
            rd->url = g_strdup(gtk_entry_get_text(GTK_ENTRY(rei->txt_url)));
            rd->db_filename = gnome_vfs_expand_initial_tilde(
                    gtk_entry_get_text(GTK_ENTRY(rei->txt_db_filename)));
            rd->dl_zoom_steps = hildon_controlbar_get_value(
                    HILDON_CONTROLBAR(rei->num_dl_zoom_steps));
            rd->view_zoom_steps = hildon_controlbar_get_value(
                    HILDON_CONTROLBAR(rei->num_view_zoom_steps));
            rd->double_size = gtk_toggle_button_get_active(
                    GTK_TOGGLE_BUTTON(rei->chk_double_size));
            rd->nextable = gtk_toggle_button_get_active(
                    GTK_TOGGLE_BUTTON(rei->chk_nextable));
            rd->min_zoom = hildon_number_editor_get_value(
                    HILDON_NUMBER_EDITOR(rei->num_min_zoom));
            rd->max_zoom = hildon_number_editor_get_value(
                    HILDON_NUMBER_EDITOR(rei->num_max_zoom));

            if (rei->repo) {
                /* clone layers */
                rd0 = rei->repo->layers;
                rd1 = &rd->layers;

                while (rd0) {
                    *rd1 = g_new0 (RepoData, 1);
                    (*rd1)->name = rd0->name;
                    (*rd1)->is_sqlite = rd0->is_sqlite;
                    (*rd1)->url = rd0->url;
                    (*rd1)->db_filename = rd0->db_filename;
                    (*rd1)->layer_enabled = rd0->layer_enabled;
                    (*rd1)->layer_refresh_interval = rd0->layer_refresh_interval;
                    (*rd1)->layer_refresh_countdown = rd0->layer_refresh_countdown;
                    (*rd1)->layer_level = rd0->layer_level;

                    (*rd1)->dl_zoom_steps = rd0->dl_zoom_steps;
                    (*rd1)->view_zoom_steps = rd0->view_zoom_steps;
                    (*rd1)->double_size = rd0->double_size;
                    (*rd1)->nextable = rd0->nextable;
                    (*rd1)->min_zoom = rd0->min_zoom;
                    (*rd1)->max_zoom = rd0->max_zoom;

                    set_repo_type (*rd1);

                    rd0 = rd0->layers;
                    rd1 = &(*rd1)->layers;
                }
                *rd1 = NULL;
            }
            else
                rd->layers = NULL;

            rd->layer_level = 0;
            set_repo_type(rd);

            _repo_list = g_list_append(_repo_list, rd);

            if(!_curr_repo && !strcmp(old_curr_repo_name, rd->name))
                repo_set_curr(rd);
            else if(i == curr_repo_index)
                repo_set_curr(rd);
        }
        if(!_curr_repo)
            repo_set_curr((RepoData*)g_list_first(_repo_list)->data);
        menu_maps_add_repos();

        settings_save();
        break;
    }

    gtk_widget_hide(dialog);

    /* Clear out the notebook entries. */
    while(rmi.repo_edits)
        repoman_delete(&rmi, 0);

    map_set_zoom(_zoom); /* make sure we're at an appropriate zoom level. */
    map_refresh_mark (TRUE);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
mapman_by_area(gdouble start_lat, gdouble start_lon,
        gdouble end_lat, gdouble end_lon, MapmanInfo *mapman_info,
        MapUpdateType update_type,
        gint download_batch_id)
{
    gint start_unitx, start_unity, end_unitx, end_unity;
    gint num_maps = 0;
    gint z;
    gchar buffer[80];
    GtkWidget *confirm;
    printf("%s(%f, %f, %f, %f)\n", __PRETTY_FUNCTION__, start_lat, start_lon,
            end_lat, end_lon);

    latlon2unit(start_lat, start_lon, start_unitx, start_unity);
    latlon2unit(end_lat, end_lon, end_unitx, end_unity);

    /* Swap if they specified flipped lats or lons. */
    if(start_unitx > end_unitx)
    {
        gint swap = start_unitx;
        start_unitx = end_unitx;
        end_unitx = swap;
    }
    if(start_unity > end_unity)
    {
        gint swap = start_unity;
        start_unity = end_unity;
        end_unity = swap;
    }

    /* First, get the number of maps to download. */
    for(z = 0; z <= MAX_ZOOM; ++z)
    {
        if(gtk_toggle_button_get_active(
                    GTK_TOGGLE_BUTTON(mapman_info->chk_zoom_levels[z])))
        {
            gint start_tilex, start_tiley, end_tilex, end_tiley;
            start_tilex = unit2ztile(start_unitx, z);
            start_tiley = unit2ztile(start_unity, z);
            end_tilex = unit2ztile(end_unitx, z);
            end_tiley = unit2ztile(end_unity, z);
            num_maps += (end_tilex - start_tilex + 1)
                * (end_tiley - start_tiley + 1);
        }
    }

    if(update_type == MAP_UPDATE_DELETE)
    {
        snprintf(buffer, sizeof(buffer), "%s %d %s", _("Confirm DELETION of"),
                num_maps, _("maps "));
    }
    else
    {
        snprintf(buffer, sizeof(buffer),
                "%s %d %s\n(%s %.2f MB)\n", _("Confirm download of"),
                num_maps, _("maps"), _("up to about"),
                num_maps * (strstr(_curr_repo->url, "%s") ? 18e-3 : 6e-3));
    }
    confirm = hildon_note_new_confirmation(
            GTK_WINDOW(mapman_info->dialog), buffer);

    if(GTK_RESPONSE_OK != gtk_dialog_run(GTK_DIALOG(confirm)))
    {
        gtk_widget_destroy(confirm);
        vprintf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
        return FALSE;
    }

    g_mutex_lock(_mut_priority_mutex);
    for(z = 0; z <= MAX_ZOOM; ++z)
    {
        if(gtk_toggle_button_get_active(
                    GTK_TOGGLE_BUTTON(mapman_info->chk_zoom_levels[z])))
        {
            gint start_tilex, start_tiley, end_tilex, end_tiley;
            gint tilex, tiley;
            start_tilex = unit2ztile(start_unitx, z);
            start_tiley = unit2ztile(start_unity, z);
            end_tilex = unit2ztile(end_unitx, z);
            end_tiley = unit2ztile(end_unity, z);
            for(tiley = start_tiley; tiley <= end_tiley; tiley++)
            {
                for(tilex = start_tilex; tilex <= end_tilex; tilex++)
                {
                    /* Make sure this tile is even possible. */
                    if((unsigned)tilex < unit2ztile(WORLD_SIZE_UNITS, z)
                      && (unsigned)tiley < unit2ztile(WORLD_SIZE_UNITS, z))
                    {
                        RepoData* rd = _curr_repo;

                        while (rd) {
                            if (rd == _curr_repo
                                    || (rd->layer_enabled && MAPDB_EXISTS(rd)))
                                mapdb_initiate_update(rd, z, tilex, tiley,
                                  update_type, download_batch_id,
                                  (abs(tilex - unit2tile(_next_center.unitx))
                                   +abs(tiley - unit2tile(_next_center.unity))),
                                  NULL);
                            rd = rd->layers;
                        }
                    }
                }
            }
        }
    }
    g_mutex_unlock(_mut_priority_mutex);

    gtk_widget_destroy(confirm);
    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
mapman_by_route(MapmanInfo *mapman_info, MapUpdateType update_type,
        gint download_batch_id)
{
    GtkWidget *confirm;
    gint prev_tilex, prev_tiley, num_maps = 0, z;
    Point *curr;
    gchar buffer[80];
    gint radius = hildon_number_editor_get_value(
            HILDON_NUMBER_EDITOR(mapman_info->num_route_radius));
    printf("%s()\n", __PRETTY_FUNCTION__);

    /* First, get the number of maps to download. */
    for(z = 0; z <= MAX_ZOOM; ++z)
    {
        if(gtk_toggle_button_get_active(
                    GTK_TOGGLE_BUTTON(mapman_info->chk_zoom_levels[z])))
        {
            prev_tilex = 0;
            prev_tiley = 0;
            for(curr = _route.head - 1; curr++ != _route.tail; )
            {
                if(curr->unity)
                {
                    gint tilex = unit2ztile(curr->unitx, z);
                    gint tiley = unit2ztile(curr->unity, z);
                    if(tilex != prev_tilex || tiley != prev_tiley)
                    {
                        if(prev_tiley)
                            num_maps += (abs((gint)tilex - prev_tilex) + 1)
                                * (abs((gint)tiley - prev_tiley) + 1) - 1;
                        prev_tilex = tilex;
                        prev_tiley = tiley;
                    }
                }
            }
        }
    }
    num_maps *= 0.625 * pow(radius + 1, 1.85);

    if(update_type == MAP_UPDATE_DELETE)
    {
        snprintf(buffer, sizeof(buffer), "%s %s %d %s",
                _("Confirm DELETION of"), _("about"),
                num_maps, _("maps "));
    }
    else
    {
        snprintf(buffer, sizeof(buffer),
                "%s %s %d %s\n(%s %.2f MB)\n", _("Confirm download of"),
                _("about"),
                num_maps, _("maps"), _("up to about"),
                num_maps * (strstr(_curr_repo->url, "%s") ? 18e-3 : 6e-3));
    }
    confirm = hildon_note_new_confirmation(
            GTK_WINDOW(mapman_info->dialog), buffer);

    if(GTK_RESPONSE_OK != gtk_dialog_run(GTK_DIALOG(confirm)))
    {
        gtk_widget_destroy(confirm);
        vprintf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
        return FALSE;
    }

    /* Now, do the actual download. */
    g_mutex_lock(_mut_priority_mutex);
    for(z = 0; z <= MAX_ZOOM; ++z)
    {
        if(gtk_toggle_button_get_active(
                    GTK_TOGGLE_BUTTON(mapman_info->chk_zoom_levels[z])))
        {
            prev_tilex = 0;
            prev_tiley = 0;
            for(curr = _route.head - 1; curr++ != _route.tail; )
            {
                if(curr->unity)
                {
                    gint tilex = unit2ztile(curr->unitx, z);
                    gint tiley = unit2ztile(curr->unity, z);
                    if(tilex != prev_tilex || tiley != prev_tiley)
                    {
                        gint minx, miny, maxx, maxy, x, y;
                        if(prev_tiley != 0)
                        {
                            minx = MIN(tilex, prev_tilex) - radius;
                            miny = MIN(tiley, prev_tiley) - radius;
                            maxx = MAX(tilex, prev_tilex) + radius;
                            maxy = MAX(tiley, prev_tiley) + radius;
                        }
                        else
                        {
                            minx = tilex - radius;
                            miny = tiley - radius;
                            maxx = tilex + radius;
                            maxy = tiley + radius;
                        }
                        for(x = minx; x <= maxx; x++)
                        {
                            for(y = miny; y <= maxy; y++)
                            {
                                /* Make sure this tile is even possible. */
                                if((unsigned)tilex
                                        < unit2ztile(WORLD_SIZE_UNITS, z)
                                  && (unsigned)tiley
                                        < unit2ztile(WORLD_SIZE_UNITS, z))
                                {
                                    mapdb_initiate_update(_curr_repo, z, x, y,
                                        update_type, download_batch_id,
                                        (abs(tilex - unit2tile(
                                                 _next_center.unitx))
                                         + abs(tiley - unit2tile(
                                                 _next_center.unity))),
                                        NULL);
                                }
                            }
                        }
                        prev_tilex = tilex;
                        prev_tiley = tiley;
                    }
                }
            }
        }
    }
    g_mutex_unlock(_mut_priority_mutex);
    _route_dl_radius = radius;
    gtk_widget_destroy(confirm);
    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static void
mapman_clear(GtkWidget *widget, MapmanInfo *mapman_info)
{
    gint z;
    printf("%s()\n", __PRETTY_FUNCTION__);
    if(gtk_notebook_get_current_page(GTK_NOTEBOOK(mapman_info->notebook)))
        /* This is the second page (the "Zoom" page) - clear the checks. */
        for(z = 0; z <= MAX_ZOOM; ++z)
            gtk_toggle_button_set_active(
                    GTK_TOGGLE_BUTTON(mapman_info->chk_zoom_levels[z]), FALSE);
    else
    {
        /* This is the first page (the "Area" page) - clear the text fields. */
        gtk_entry_set_text(GTK_ENTRY(mapman_info->txt_topleft_lat), "");
        gtk_entry_set_text(GTK_ENTRY(mapman_info->txt_topleft_lon), "");
        gtk_entry_set_text(GTK_ENTRY(mapman_info->txt_botright_lat), "");
        gtk_entry_set_text(GTK_ENTRY(mapman_info->txt_botright_lon), "");
    }
    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

void mapman_update_state(GtkWidget *widget, MapmanInfo *mapman_info)
{
    printf("%s()\n", __PRETTY_FUNCTION__);
    gtk_widget_set_sensitive( mapman_info->chk_overwrite,
            gtk_toggle_button_get_active(
                GTK_TOGGLE_BUTTON(mapman_info->rad_download)));

    if(gtk_toggle_button_get_active(
                GTK_TOGGLE_BUTTON(mapman_info->rad_by_area)))
        gtk_widget_show(mapman_info->tbl_area);
    else if(gtk_notebook_get_n_pages(GTK_NOTEBOOK(mapman_info->notebook)) == 3)
        gtk_widget_hide(mapman_info->tbl_area);

    gtk_widget_set_sensitive(mapman_info->num_route_radius,
            gtk_toggle_button_get_active(
                GTK_TOGGLE_BUTTON(mapman_info->rad_by_route)));
    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

gboolean
mapman_dialog()
{
    static GtkWidget *dialog = NULL;
    static GtkWidget *vbox = NULL;
    static GtkWidget *hbox = NULL;
    static GtkWidget *table = NULL;
    static GtkWidget *label = NULL;
    static GtkWidget *button = NULL;
    static GtkWidget *lbl_gps_lat = NULL;
    static GtkWidget *lbl_gps_lon = NULL;
    static GtkWidget *lbl_center_lat = NULL;
    static GtkWidget *lbl_center_lon = NULL;
    static MapmanInfo mapman_info;
    static gint last_deg_format = 0;
    
    gchar buffer[80];
    gdouble lat, lon;
    gint z;
    gint prev_degformat = _degformat;
    gint fallback_deg_format = _degformat;
    gdouble top_left_lat, top_left_lon, bottom_right_lat, bottom_right_lon;

    
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(!MAPDB_EXISTS(_curr_repo))
    {
        popup_error(_window, "To manage maps, you must set a valid repository "
                "database filename in the \"Manage Repositories\" dialog.");
        vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
        return TRUE;
    }

    // - If the coord system has changed then we need to update certain values
    
    /* Initialize to the bounds of the screen. */
    unit2latlon(
            _center.unitx - pixel2unit(MAX(_view_width_pixels,
                    _view_height_pixels) / 2),
            _center.unity - pixel2unit(MAX(_view_width_pixels,
                    _view_height_pixels) / 2), top_left_lat, top_left_lon);
    
    BOUND(top_left_lat, -90.f, 90.f);
    BOUND(top_left_lon, -180.f, 180.f);

        
    unit2latlon(
            _center.unitx + pixel2unit(MAX(_view_width_pixels,
                    _view_height_pixels) / 2),
            _center.unity + pixel2unit(MAX(_view_width_pixels,
                    _view_height_pixels) / 2), bottom_right_lat, bottom_right_lon);
    BOUND(bottom_right_lat, -90.f, 90.f);
    BOUND(bottom_right_lon, -180.f, 180.f);
    
    

    
    if(!coord_system_check_lat_lon (top_left_lat, top_left_lon, &fallback_deg_format))
    {
    	_degformat = fallback_deg_format;
    }
    else
    {
    	// top left is valid, also check bottom right
        if(!coord_system_check_lat_lon (bottom_right_lat, bottom_right_lon, &fallback_deg_format))
        {
        	_degformat = fallback_deg_format;
        }
    }
    
    
    if(_degformat != last_deg_format)
    {
    	last_deg_format = _degformat;
    	
		if(dialog != NULL) gtk_widget_destroy(dialog);
    	dialog = NULL;
    }

    if(dialog == NULL)
    {
        mapman_info.dialog = dialog = gtk_dialog_new_with_buttons(
                _("Manage Maps"),
                GTK_WINDOW(_window), GTK_DIALOG_MODAL,
                GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                NULL);

        /* Enable the help button. */
#ifndef LEGACY
#else
        ossohelp_dialog_help_enable(
                GTK_DIALOG(mapman_info.dialog), HELP_ID_MAPMAN, _osso);
#endif

        /* Clear button. */
        gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area),
                button = gtk_button_new_with_label(_("Clear")));
        g_signal_connect(G_OBJECT(button), "clicked",
                          G_CALLBACK(mapman_clear), &mapman_info);

        /* Cancel button. */
        gtk_dialog_add_button(GTK_DIALOG(dialog),
                GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT);

        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
                mapman_info.notebook = gtk_notebook_new(), TRUE, TRUE, 0);

        /* Setup page. */
        gtk_notebook_append_page(GTK_NOTEBOOK(mapman_info.notebook),
                vbox = gtk_vbox_new(FALSE, 2),
                label = gtk_label_new(_("Setup")));
        gtk_notebook_set_tab_label_packing(
                GTK_NOTEBOOK(mapman_info.notebook), vbox,
                FALSE, FALSE, GTK_PACK_START);

        gtk_box_pack_start(GTK_BOX(vbox),
                hbox = gtk_hbox_new(FALSE, 4),
                FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox),
                mapman_info.rad_download = gtk_radio_button_new_with_label(
                    NULL,_("Download Maps")),
                FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox),
                label = gtk_alignment_new(0.f, 0.5f, 0.f, 0.f),
                FALSE, FALSE, 0);
        gtk_container_add(GTK_CONTAINER(label),
                mapman_info.chk_overwrite
                        = gtk_check_button_new_with_label(_("Overwrite"))),

        gtk_box_pack_start(GTK_BOX(vbox),
                mapman_info.rad_delete
                        = gtk_radio_button_new_with_label_from_widget(
                            GTK_RADIO_BUTTON(mapman_info.rad_download),
                            _("Delete Maps")),
                FALSE, FALSE, 0);

        gtk_box_pack_start(GTK_BOX(vbox),
                gtk_hseparator_new(),
                FALSE, FALSE, 0);

        gtk_box_pack_start(GTK_BOX(vbox),
                mapman_info.rad_by_area
                        = gtk_radio_button_new_with_label(NULL,
                            _("By Area (see tab)")),
                FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox),
                hbox = gtk_hbox_new(FALSE, 4),
                FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox),
                mapman_info.rad_by_route
                        = gtk_radio_button_new_with_label_from_widget(
                            GTK_RADIO_BUTTON(mapman_info.rad_by_area),
                            _("Along Route - Radius (tiles):")),
                FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox),
                mapman_info.num_route_radius = hildon_number_editor_new(0,100),
                FALSE, FALSE, 0);
        hildon_number_editor_set_value(
                HILDON_NUMBER_EDITOR(mapman_info.num_route_radius),
                _route_dl_radius);


        /* Zoom page. */
        gtk_notebook_append_page(GTK_NOTEBOOK(mapman_info.notebook),
                table = gtk_table_new(5, 5, FALSE),
                label = gtk_label_new(_("Zoom")));
        gtk_notebook_set_tab_label_packing(
                GTK_NOTEBOOK(mapman_info.notebook), table,
                FALSE, FALSE, GTK_PACK_START);
        gtk_table_attach(GTK_TABLE(table),
                label = gtk_label_new(
                    _("Zoom Levels to Download: (0 = most detail)")),
                0, 4, 0, 1, GTK_FILL, 0, 4, 0);
        gtk_misc_set_alignment(GTK_MISC(label), 0.f, 0.5f);
        snprintf(buffer, sizeof(buffer), "%d", 0);
        gtk_table_attach(GTK_TABLE(table),
                mapman_info.chk_zoom_levels[0]
                        = gtk_check_button_new_with_label(buffer),
                4, 5 , 0, 1, GTK_FILL, 0, 0, 0);
        for(z = 0; z < MAX_ZOOM; ++z)
        {
            snprintf(buffer, sizeof(buffer), "%d", z + 1);
            gtk_table_attach(GTK_TABLE(table),
                    mapman_info.chk_zoom_levels[z + 1]
                            = gtk_check_button_new_with_label(buffer),
                    z / 4, z / 4 + 1, z % 4 + 1, z % 4 + 2,
                    GTK_FILL, 0, 0, 0);
        }

        /* Area page. */
        gtk_notebook_append_page(GTK_NOTEBOOK(mapman_info.notebook),
            mapman_info.tbl_area = gtk_table_new(5, 3, FALSE),
            label = gtk_label_new(_("Area")));

        /* Label Columns. */
        gtk_table_attach(GTK_TABLE(mapman_info.tbl_area),
        		label = gtk_label_new(DEG_FORMAT_ENUM_TEXT[_degformat].long_field_1),
                1, 2, 0, 1, GTK_FILL, 0, 4, 0);
        gtk_misc_set_alignment(GTK_MISC(label), 0.5f, 0.5f);
        
        if(DEG_FORMAT_ENUM_TEXT[_degformat].field_2_in_use)
        {
	        gtk_table_attach(GTK_TABLE(mapman_info.tbl_area),
	        		label = gtk_label_new(DEG_FORMAT_ENUM_TEXT[_degformat].long_field_2),
	                2, 3, 0, 1, GTK_FILL, 0, 4, 0);
	        gtk_misc_set_alignment(GTK_MISC(label), 0.5f, 0.5f);
        }
        
        /* GPS. */
        gtk_table_attach(GTK_TABLE(mapman_info.tbl_area),
                label = gtk_label_new(_("GPS Location")),
                0, 1, 1, 2, GTK_FILL, 0, 4, 0);
        gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
        gtk_table_attach(GTK_TABLE(mapman_info.tbl_area),
                lbl_gps_lat = gtk_label_new(""),
                1, 2, 1, 2, GTK_FILL, 0, 4, 0);
        gtk_label_set_selectable(GTK_LABEL(lbl_gps_lat), TRUE);
        gtk_misc_set_alignment(GTK_MISC(lbl_gps_lat), 1.f, 0.5f);
        
        if(DEG_FORMAT_ENUM_TEXT[_degformat].field_2_in_use)
        {
	        gtk_table_attach(GTK_TABLE(mapman_info.tbl_area),
	                lbl_gps_lon = gtk_label_new(""),
	                2, 3, 1, 2, GTK_FILL, 0, 4, 0);
	        gtk_label_set_selectable(GTK_LABEL(lbl_gps_lon), TRUE);
	        gtk_misc_set_alignment(GTK_MISC(lbl_gps_lon), 1.f, 0.5f);
        }
        
        /* Center. */
        gtk_table_attach(GTK_TABLE(mapman_info.tbl_area),
                label = gtk_label_new(_("View Center")),
                0, 1, 2, 3, GTK_FILL, 0, 4, 0);
        gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
        gtk_table_attach(GTK_TABLE(mapman_info.tbl_area),
                lbl_center_lat = gtk_label_new(""),
                1, 2, 2, 3, GTK_FILL, 0, 4, 0);
        gtk_label_set_selectable(GTK_LABEL(lbl_center_lat), TRUE);
        gtk_misc_set_alignment(GTK_MISC(lbl_center_lat), 1.f, 0.5f);
        
    
        if(DEG_FORMAT_ENUM_TEXT[_degformat].field_2_in_use)
        {
	        gtk_table_attach(GTK_TABLE(mapman_info.tbl_area),
	                lbl_center_lon = gtk_label_new(""),
	                2, 3, 2, 3, GTK_FILL, 0, 4, 0);
	        gtk_label_set_selectable(GTK_LABEL(lbl_center_lon), TRUE);
	        gtk_misc_set_alignment(GTK_MISC(lbl_center_lon), 1.f, 0.5f);
        }

        /* default values for Top Left and Bottom Right are defined by the
         * rectangle of the current and the previous Center */

        /* Top Left. */
        gtk_table_attach(GTK_TABLE(mapman_info.tbl_area),
                label = gtk_label_new(_("Top-Left")),
                0, 1, 3, 4, GTK_FILL, 0, 4, 0);
        gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
        gtk_table_attach(GTK_TABLE(mapman_info.tbl_area),
                mapman_info.txt_topleft_lat = gtk_entry_new(),
                1, 2, 3, 4, GTK_FILL, 0, 4, 0);
        gtk_entry_set_width_chars(GTK_ENTRY(mapman_info.txt_topleft_lat), 12);
        gtk_entry_set_alignment(GTK_ENTRY(mapman_info.txt_topleft_lat), 1.f);
#ifdef MAEMO_CHANGES
        g_object_set(G_OBJECT(mapman_info.txt_topleft_lat),
#ifndef LEGACY
                "hildon-input-mode",
                HILDON_GTK_INPUT_MODE_FULL, NULL);
#else
                HILDON_INPUT_MODE_HINT,
                HILDON_INPUT_MODE_HINT_ALPHANUMERICSPECIAL, NULL);
        g_object_set(G_OBJECT(mapman_info.txt_topleft_lat),
                HILDON_AUTOCAP,
                FALSE, NULL);
#endif
#endif
        
        if(DEG_FORMAT_ENUM_TEXT[_degformat].field_2_in_use)
        {
	        gtk_table_attach(GTK_TABLE(mapman_info.tbl_area),
	                mapman_info.txt_topleft_lon = gtk_entry_new(),
	                2, 3, 3, 4, GTK_FILL, 0, 4, 0);
	        gtk_entry_set_width_chars(GTK_ENTRY(mapman_info.txt_topleft_lon), 12);
	        gtk_entry_set_alignment(GTK_ENTRY(mapman_info.txt_topleft_lon), 1.f);
#ifdef MAEMO_CHANGES
		    g_object_set(G_OBJECT(mapman_info.txt_topleft_lon),
#ifndef LEGACY
	                "hildon-input-mode",
	                HILDON_GTK_INPUT_MODE_FULL, NULL);
#else
	                HILDON_INPUT_MODE_HINT,
	                HILDON_INPUT_MODE_HINT_ALPHANUMERICSPECIAL, NULL);
	        g_object_set(G_OBJECT(mapman_info.txt_topleft_lon),
	                HILDON_AUTOCAP,
	                FALSE, NULL);
#endif
#endif

        }
        
        /* Bottom Right. */
        gtk_table_attach(GTK_TABLE(mapman_info.tbl_area),
                label = gtk_label_new(_("Bottom-Right")),
                0, 1, 4, 5, GTK_FILL, 0, 4, 0);
        gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
        gtk_table_attach(GTK_TABLE(mapman_info.tbl_area),
                mapman_info.txt_botright_lat = gtk_entry_new(),
                1, 2, 4, 5, GTK_FILL, 0, 4, 0);
        gtk_entry_set_width_chars(GTK_ENTRY(mapman_info.txt_botright_lat), 12);
        gtk_entry_set_alignment(GTK_ENTRY(mapman_info.txt_botright_lat), 1.f);
#ifdef MAEMO_CHANGES
        g_object_set(G_OBJECT(mapman_info.txt_botright_lat),
#ifndef LEGACY
                "hildon-input-mode",
                HILDON_GTK_INPUT_MODE_FULL, NULL);
#else
                HILDON_INPUT_MODE_HINT,
                HILDON_INPUT_MODE_HINT_ALPHANUMERICSPECIAL, NULL);
        g_object_set(G_OBJECT(mapman_info.txt_botright_lat),
                HILDON_AUTOCAP,
                FALSE, NULL);
#endif
#endif
        
        if(DEG_FORMAT_ENUM_TEXT[_degformat].field_2_in_use)
        {
	        gtk_table_attach(GTK_TABLE(mapman_info.tbl_area),
	                mapman_info.txt_botright_lon = gtk_entry_new(),
	                2, 3, 4, 5, GTK_FILL, 0, 4, 0);
	        gtk_entry_set_width_chars(GTK_ENTRY(mapman_info.txt_botright_lon), 12);
	        gtk_entry_set_alignment(GTK_ENTRY(mapman_info.txt_botright_lon), 1.f);
#ifdef MAEMO_CHANGES
	        g_object_set(G_OBJECT(mapman_info.txt_botright_lon),
#ifndef LEGACY
	                "hildon-input-mode",
	                HILDON_GTK_INPUT_MODE_FULL, NULL);
#else
	            	HILDON_INPUT_MODE_HINT,
	                HILDON_INPUT_MODE_HINT_ALPHANUMERICSPECIAL, NULL);
	        g_object_set(G_OBJECT(mapman_info.txt_botright_lon),
	                HILDON_AUTOCAP,
	                FALSE, NULL);
#endif
#endif

        }
        
        /* Default action is to download by area. */
        gtk_toggle_button_set_active(
                GTK_TOGGLE_BUTTON(mapman_info.rad_by_area), TRUE);

        g_signal_connect(G_OBJECT(mapman_info.rad_download), "clicked",
                          G_CALLBACK(mapman_update_state), &mapman_info);
        g_signal_connect(G_OBJECT(mapman_info.rad_delete), "clicked",
                          G_CALLBACK(mapman_update_state), &mapman_info);
        g_signal_connect(G_OBJECT(mapman_info.rad_by_area), "clicked",
                          G_CALLBACK(mapman_update_state), &mapman_info);
        g_signal_connect(G_OBJECT(mapman_info.rad_by_route), "clicked",
                          G_CALLBACK(mapman_update_state), &mapman_info);
    }

    /* Initialize fields.  Do no use g_ascii_formatd; these strings will be
     * output (and parsed) as locale-dependent. */

    gtk_widget_set_sensitive(mapman_info.rad_by_route,
            _route.head != _route.tail);

    
    gchar buffer1[15];
    gchar buffer2[15];
    format_lat_lon(_gps.lat, _gps.lon, buffer1, buffer2);
    //lat_format(_gps.lat, buffer);
    gtk_label_set_text(GTK_LABEL(lbl_gps_lat), buffer1);
    //lon_format(_gps.lon, buffer);
    if(DEG_FORMAT_ENUM_TEXT[_degformat].field_2_in_use)
    	gtk_label_set_text(GTK_LABEL(lbl_gps_lon), buffer2);
    
    unit2latlon(_center.unitx, _center.unity, lat, lon);
    
    format_lat_lon(lat, lon, buffer1, buffer2);
    //lat_format(lat, buffer);
    gtk_label_set_text(GTK_LABEL(lbl_center_lat), buffer1);
    //lon_format(lon, buffer);
    if(DEG_FORMAT_ENUM_TEXT[_degformat].field_2_in_use)
    	gtk_label_set_text(GTK_LABEL(lbl_center_lon), buffer2);

    format_lat_lon(top_left_lat, top_left_lon, buffer1, buffer2);
    
    gtk_entry_set_text(GTK_ENTRY(mapman_info.txt_topleft_lat), buffer1);
    
    if(DEG_FORMAT_ENUM_TEXT[_degformat].field_2_in_use)
    	gtk_entry_set_text(GTK_ENTRY(mapman_info.txt_topleft_lon), buffer2);
    
    format_lat_lon(bottom_right_lat, bottom_right_lon, buffer1, buffer2);
    //lat_format(lat, buffer);
    gtk_entry_set_text(GTK_ENTRY(mapman_info.txt_botright_lat), buffer1);
    //lon_format(lon, buffer);
    if(DEG_FORMAT_ENUM_TEXT[_degformat].field_2_in_use)
    	gtk_entry_set_text(GTK_ENTRY(mapman_info.txt_botright_lon), buffer2);

    /* Initialize zoom levels. */
    {
        gint i;
        for(i = 0; i <= MAX_ZOOM; i++)
        {
            gtk_toggle_button_set_active(
                    GTK_TOGGLE_BUTTON(mapman_info.chk_zoom_levels[i]), FALSE);
        }
    }
    gtk_toggle_button_set_active(
            GTK_TOGGLE_BUTTON(mapman_info.chk_zoom_levels[
                _zoom + (_curr_repo->double_size ? 1 : 0)]), TRUE);

    gtk_widget_show_all(dialog);

    mapman_update_state(NULL, &mapman_info);

    if(_curr_repo->type != REPOTYPE_NONE)
    {
        gtk_widget_set_sensitive(mapman_info.rad_download, TRUE);
    }
    else
    {
        gtk_widget_set_sensitive(mapman_info.rad_download, FALSE);
        popup_error(dialog,
                _("NOTE: You must set a Map URI in the current repository in "
                    "order to download maps."));
    }

    while(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(dialog)))
    {
        MapUpdateType update_type;
        static gint8 download_batch_id = INT8_MIN;

        if(gtk_toggle_button_get_active(
                    GTK_TOGGLE_BUTTON(mapman_info.rad_delete)))
            update_type = MAP_UPDATE_DELETE;
        else if(gtk_toggle_button_get_active(
                GTK_TOGGLE_BUTTON(mapman_info.chk_overwrite)))
            update_type = MAP_UPDATE_OVERWRITE;
        else
            update_type = MAP_UPDATE_ADD;

        ++download_batch_id;
        if(gtk_toggle_button_get_active(
                    GTK_TOGGLE_BUTTON(mapman_info.rad_by_route)))
        {
            if(mapman_by_route(&mapman_info, update_type, download_batch_id))
                break;
        }
        else
        {
            const gchar *text_lon, *text_lat;
            //gchar *error_check;
            gdouble start_lat, start_lon, end_lat, end_lon;

            text_lat = gtk_entry_get_text(GTK_ENTRY(mapman_info.txt_topleft_lat));
            text_lon = gtk_entry_get_text(GTK_ENTRY(mapman_info.txt_topleft_lon));
            
            if(!parse_coords(text_lat, text_lon, &start_lat, &start_lon))
            {
            	popup_error(dialog, _("Invalid Top-Left coordinate specified"));
            	continue;
            }
            
            text_lat = gtk_entry_get_text(GTK_ENTRY(mapman_info.txt_botright_lat));
            text_lon = gtk_entry_get_text(GTK_ENTRY(mapman_info.txt_botright_lon));

            if(!parse_coords(text_lat, text_lon, &end_lat, &end_lon))
            {
            	popup_error(dialog, _("Invalid Bottom-Right coordinate specified"));
            	continue;
            }

  

            if(mapman_by_area(start_lat, start_lon, end_lat, end_lon,
                        &mapman_info, update_type, download_batch_id))
                break;
        }
    }

    gtk_widget_hide(dialog);
    
    _degformat = prev_degformat;

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}


/* changes visibility of current repo's layers to it's previous state */
void maps_toggle_visible_layers ()
{
    RepoData *rd = _curr_repo;
    gboolean changed = FALSE;

    printf("%s()\n", __PRETTY_FUNCTION__);

    if (!rd) {
        vprintf("%s(): return\n", __PRETTY_FUNCTION__);
        return;
    }

    rd = rd->layers;

    while (rd) {
        if (rd->layer_enabled) {
            changed = TRUE;
            rd->layer_was_enabled = rd->layer_enabled;
            rd->layer_enabled = FALSE;
        }
        else {
            rd->layer_enabled = rd->layer_was_enabled;
            if (rd->layer_was_enabled)
                changed = TRUE;
        }

        rd = rd->layers;
    }

    /* redraw map */
    if (changed) {
        menu_layers_remove_repos ();
        menu_layers_add_repos ();
        map_refresh_mark (TRUE);
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

RepoData*
create_default_repo()
{
    /* We have no repositories - create a default one. */
    RepoData *repo = g_new0(RepoData, 1);

    repo->db_filename = gnome_vfs_expand_initial_tilde(
            REPO_DEFAULT_CACHE_DIR);
    repo->url=g_strdup(REPO_DEFAULT_MAP_URI);
    repo->dl_zoom_steps = REPO_DEFAULT_DL_ZOOM_STEPS;
    repo->name = g_strdup(REPO_DEFAULT_NAME);
    repo->view_zoom_steps = REPO_DEFAULT_VIEW_ZOOM_STEPS;
    repo->double_size = FALSE;
    repo->nextable = TRUE;
    repo->min_zoom = REPO_DEFAULT_MIN_ZOOM;
    repo->max_zoom = REPO_DEFAULT_MAX_ZOOM;
    repo->layers = NULL;
    repo->layer_level = 0;
    repo->is_sqlite = TRUE;
    set_repo_type(repo);

    return repo;
}

