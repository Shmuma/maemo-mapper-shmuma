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
#include <glib/gstdio.h>
#include <fcntl.h>
#include <osso-helplib.h>
#include <hildon-widgets/hildon-note.h>
#include <hildon-widgets/hildon-file-chooser-dialog.h>
#include <hildon-widgets/hildon-number-editor.h>
#include <hildon-widgets/hildon-banner.h>
#include <hildon-widgets/hildon-input-mode-hint.h>

#include <locale.h>

#include "types.h"
#include "data.h"
#include "defines.h"

#include "display.h"
#include "main.h"
#include "maps.h"
#include "menu.h"
#include "settings.h"
#include "util.h"


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
    GtkWidget *txt_url;
    GtkWidget *txt_db_filename;
    GtkWidget *num_dl_zoom_steps;
    GtkWidget *num_view_zoom_steps;
    GtkWidget *chk_double_size;
    GtkWidget *chk_nextable;
    GtkWidget *btn_browse;
    GtkWidget *num_min_zoom;
    GtkWidget *num_max_zoom;
    BrowseInfo browse_info;
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


gboolean
mapdb_exists(RepoData *repo, gint zoom, gint tilex, gint tiley)
{
    gboolean exists;
    vprintf("%s(%s, %d, %d, %d)\n", __PRETTY_FUNCTION__,
            repo->name, zoom, tilex, tiley);

    g_mutex_lock(_mapdb_mutex);

    if(!repo->db)
    {
        /* There is no cache.  Return FALSE. */
        g_mutex_unlock(_mapdb_mutex);
        vprintf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
        return FALSE;
    }

#ifdef MAPDB_SQLITE
    /* Attempt to retrieve map from database. */
    if(SQLITE_OK == sqlite3_bind_int(repo->stmt_map_exists, 1, zoom)
    && SQLITE_OK == sqlite3_bind_int(repo->stmt_map_exists, 2, tilex)
    && SQLITE_OK == sqlite3_bind_int(repo->stmt_map_exists, 3, tiley)
    && SQLITE_ROW == sqlite3_step(repo->stmt_map_exists)
    && sqlite3_column_int(repo->stmt_map_exists, 0) > 0)
    {
        exists = TRUE;
    }
    else
    {
        exists = FALSE;
    }
    sqlite3_reset(repo->stmt_map_exists);
#else
    {
        datum d;
        gint32 key[] = {
            GINT32_TO_BE(zoom),
            GINT32_TO_BE(tilex),
            GINT32_TO_BE(tiley)
        };
        d.dptr = (gchar*)&key;
        d.dsize = sizeof(key);
        exists = gdbm_exists(repo->db, d);
    }
#endif

    g_mutex_unlock(_mapdb_mutex);

    vprintf("%s(): return %d\n", __PRETTY_FUNCTION__, exists);
    return exists;
}

GdkPixbuf*
mapdb_get(RepoData *repo, gint zoom, gint tilex, gint tiley)
{
    GdkPixbuf *pixbuf = NULL;
    vprintf("%s(%s, %d, %d, %d)\n", __PRETTY_FUNCTION__,
            repo->name, zoom, tilex, tiley);

    g_mutex_lock(_mapdb_mutex);

    if(!repo->db)
    {
        /* There is no cache.  Return NULL. */
        g_mutex_unlock(_mapdb_mutex);
        vprintf("%s(): return NULL\n", __PRETTY_FUNCTION__);
        return NULL;
    }

#ifdef MAPDB_SQLITE
    /* Attempt to retrieve map from database. */
    if(SQLITE_OK == sqlite3_bind_int(repo->stmt_map_select, 1, zoom)
    && SQLITE_OK == sqlite3_bind_int(repo->stmt_map_select, 2, tilex)
    && SQLITE_OK == sqlite3_bind_int(repo->stmt_map_select, 3, tiley)
    && SQLITE_ROW == sqlite3_step(repo->stmt_map_select))
    {
        const gchar *bytes = NULL;
        gint size = sqlite3_column_bytes(repo->stmt_map_select, 0);

        /* "Pixbufs" of size less than or equal to MAX_PIXBUF_DUP_SIZE are
         * actually keys into the dups table. */
        if(size <= MAX_PIXBUF_DUP_SIZE)
        {
            gint hash = sqlite3_column_int(repo->stmt_map_select, 0);
            if(SQLITE_OK == sqlite3_bind_int(repo->stmt_dup_select, 1, hash)
            && SQLITE_ROW == sqlite3_step(repo->stmt_dup_select))
            {
                bytes = sqlite3_column_blob(repo->stmt_dup_select, 0);
                size = sqlite3_column_bytes(repo->stmt_dup_select, 0);
            }
            else
            {
                /* Not there?  Delete the entry, then. */
                if(SQLITE_OK != sqlite3_bind_int(
                            repo->stmt_map_delete, 1, zoom)
                || SQLITE_OK != sqlite3_bind_int(
                    repo->stmt_map_delete, 2, tilex)
                || SQLITE_OK != sqlite3_bind_int(
                    repo->stmt_map_delete, 3, tiley)
                || SQLITE_DONE != sqlite3_step(repo->stmt_map_delete))
                {
                    printf("Error in stmt_map_delete: %s\n", 
                                sqlite3_errmsg(repo->db));
                }
                sqlite3_reset(repo->stmt_map_delete);

                /* We have no bytes to return to the caller. */
                bytes = NULL;
            }
            /* Don't reset the statement yet - we need the blob. */
        }
        else
        {
            bytes = sqlite3_column_blob(repo->stmt_map_select, 0);
        }
        if(bytes)
        {
            GError *error = NULL;
            GdkPixbufLoader *loader = gdk_pixbuf_loader_new();
            gdk_pixbuf_loader_write(loader, bytes, size, NULL);
            gdk_pixbuf_loader_close(loader, &error);
            if(!error)
                pixbuf = g_object_ref(gdk_pixbuf_loader_get_pixbuf(loader));
            g_object_unref(loader);
        }
        if(size <= MAX_PIXBUF_DUP_SIZE)
            sqlite3_reset(repo->stmt_dup_select);
    }
    sqlite3_reset(repo->stmt_map_select);
#else
    {
        datum d;
        gint32 key[] = {
            GINT32_TO_BE(zoom),
            GINT32_TO_BE(tilex),
            GINT32_TO_BE(tiley)
        };
        d.dptr = (gchar*)&key;
        d.dsize = sizeof(key);
        d = gdbm_fetch(repo->db, d);
        if(d.dptr)
        {
            GError *error = NULL;
            GdkPixbufLoader *loader = gdk_pixbuf_loader_new();
            gdk_pixbuf_loader_write(loader, d.dptr, d.dsize, NULL);
            g_free(d.dptr);
            gdk_pixbuf_loader_close(loader, &error);
            if(!error)
                pixbuf = g_object_ref(gdk_pixbuf_loader_get_pixbuf(loader));
            g_object_unref(loader);
        }
    }
#endif

    g_mutex_unlock(_mapdb_mutex);

    vprintf("%s(): return %p\n", __PRETTY_FUNCTION__, pixbuf);
    return pixbuf;
}

#ifdef MAPDB_SQLITE
static gboolean
mapdb_checkdec(RepoData *repo, gint zoom, gint tilex, gint tiley)
{
    gboolean success = TRUE;
    vprintf("%s(%s, %d, %d, %d)\n", __PRETTY_FUNCTION__,
            repo->name, zoom, tilex, tiley);

    /* First, we have to check if the old map was a dup. */
    if(SQLITE_OK == sqlite3_bind_int(repo->stmt_map_select, 1, zoom)
    && SQLITE_OK == sqlite3_bind_int(repo->stmt_map_select, 2, tilex)
    && SQLITE_OK == sqlite3_bind_int(repo->stmt_map_select, 3, tiley)
    && SQLITE_ROW == sqlite3_step(repo->stmt_map_select)
    && sqlite3_column_bytes(repo->stmt_map_select, 0)
            <= MAX_PIXBUF_DUP_SIZE)
    {
        /* Old map was indeed a dup. Decrement the reference count. */
        gint hash = sqlite3_column_int(repo->stmt_map_select, 0);
        if(SQLITE_OK != sqlite3_bind_int(
                    repo->stmt_dup_decrem, 1, hash)
        || SQLITE_DONE != sqlite3_step(repo->stmt_dup_decrem)
        || SQLITE_OK != sqlite3_bind_int(
                    repo->stmt_dup_delete, 1, hash)
        || SQLITE_DONE != sqlite3_step(repo->stmt_dup_delete))
        {
            success = FALSE;
            printf("Error in stmt_dup_decrem: %s\n",
                    sqlite3_errmsg(repo->db));
        }
        sqlite3_reset(repo->stmt_dup_delete);
        sqlite3_reset(repo->stmt_dup_decrem);
    }
    sqlite3_reset(repo->stmt_map_select);

    vprintf("%s(): return %d\n", __PRETTY_FUNCTION__, success);
    return success;
}
#endif

static gboolean
mapdb_update(gboolean exists, RepoData *repo,
        gint zoom, gint tilex, gint tiley, void *bytes, gint size)
{
#ifdef MAPDB_SQLITE
    sqlite3_stmt *stmt;
    gint hash = 0;
#endif
    gint success = TRUE;
    vprintf("%s(%s, %d, %d, %d)\n", __PRETTY_FUNCTION__,
            repo->name, zoom, tilex, tiley);

    g_mutex_lock(_mapdb_mutex);

    if(!repo->db)
    {
        /* There is no cache.  Return FALSE. */
        g_mutex_unlock(_mapdb_mutex);
        vprintf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
        return FALSE;
    }

#ifdef MAPDB_SQLITE
    /* At least try to open a transaction. */
    sqlite3_step(repo->stmt_trans_begin);
    sqlite3_reset(repo->stmt_trans_begin);

    /* Pixbufs of size MAX_PIXBUF_DUP_SIZE or less are special.  They are
     * probably PNGs of a single color (like blue for water or beige for empty
     * land).  To reduce redundancy in the database, we will store them in a
     * separate table and, in the maps table, only refer to them. */
    if(size <= MAX_PIXBUF_DUP_SIZE)
    {
        /* Duplicate pixbuf. */
        if(exists)
        {
            /* First, check if we need to remove a count from the dups table.*/
            mapdb_checkdec(repo, zoom, tilex, tiley);
        }
        if(success)
        {
            /* Compute hash of the bytes. */
            gchar *cur = bytes, *end = bytes + size;
            hash = *cur;
            while(cur < end)
                hash = (hash << 5) - hash + *(++cur);

            /* Check if dup already exists. */
            if(SQLITE_OK == sqlite3_bind_int(repo->stmt_dup_exists, 1, hash)
            && SQLITE_ROW == sqlite3_step(repo->stmt_dup_exists)
            && sqlite3_column_int(repo->stmt_dup_exists, 0) > 0)
            {
                /* Dup already exists - increment existing entry. */
                if(SQLITE_OK != sqlite3_bind_int(repo->stmt_dup_increm,1, hash)
                || SQLITE_DONE != sqlite3_step(repo->stmt_dup_increm))
                {
                    success = FALSE;
                    printf("Error in stmt_dup_increm: %s\n",
                            sqlite3_errmsg(repo->db));
                }
                sqlite3_reset(repo->stmt_dup_increm);
            }
            else
            {
                /* Dup doesn't exist - add new entry. */
                if(SQLITE_OK != sqlite3_bind_int(repo->stmt_dup_insert,1, hash)
                || SQLITE_OK != sqlite3_bind_blob(repo->stmt_dup_insert,
                    2, bytes, size, NULL)
                || SQLITE_DONE != sqlite3_step(repo->stmt_dup_insert))
                {
                    success = FALSE;
                    printf("Error in stmt_dup_insert: %s\n",
                            sqlite3_errmsg(repo->db));
                }
                sqlite3_reset(repo->stmt_dup_insert);
            }
            sqlite3_reset(repo->stmt_dup_exists);
        }
        /* Now, if successful so far, we fall through the end of this if
         * statement and insert the hash as the blob.  Setting bytes to NULL
         * is the signal to do this. */
        bytes = NULL;
    }

    if(success)
    {
        stmt = exists ? repo->stmt_map_update : repo->stmt_map_insert;

        /* Attempt to insert map from database. */
        if(SQLITE_OK != (bytes ? sqlite3_bind_blob(stmt, 1, bytes, size, NULL)
                    : sqlite3_bind_int(stmt, 1, hash))
        || SQLITE_OK != sqlite3_bind_int(stmt, 2, zoom)
        || SQLITE_OK != sqlite3_bind_int(stmt, 3, tilex)
        || SQLITE_OK != sqlite3_bind_int(stmt, 4, tiley)
        || SQLITE_DONE != sqlite3_step(stmt))
        {
            success = FALSE;
            printf("Error in mapdb_update: %s\n", sqlite3_errmsg(repo->db));
        }
        sqlite3_reset(stmt);
    }

    if(success)
    {
        sqlite3_step(repo->stmt_trans_commit);
        sqlite3_reset(repo->stmt_trans_commit);
    }
    else
    {
        sqlite3_step(repo->stmt_trans_rollback);
        sqlite3_reset(repo->stmt_trans_rollback);
    }

#else
    {
        datum dkey, dcon;
        gint32 key[] = {
            GINT32_TO_BE(zoom),
            GINT32_TO_BE(tilex),
            GINT32_TO_BE(tiley)
        };
        dkey.dptr = (gchar*)&key;
        dkey.dsize = sizeof(key);
        dcon.dptr = bytes;
        dcon.dsize = size;
        success = !gdbm_store(repo->db, dkey, dcon, GDBM_REPLACE);
    }
#endif
    g_mutex_unlock(_mapdb_mutex);

    vprintf("%s(): return %d\n", __PRETTY_FUNCTION__, success);
    return success;
}

static gboolean
mapdb_delete(RepoData *repo, gint zoom, gint tilex, gint tiley)
{
    gint success = FALSE;
    vprintf("%s(%s, %d, %d, %d)\n", __PRETTY_FUNCTION__,
            repo->name, zoom, tilex, tiley);

    g_mutex_lock(_mapdb_mutex);

    if(!repo->db)
    {
        /* There is no cache.  Return FALSE. */
        g_mutex_unlock(_mapdb_mutex);
        vprintf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
        return FALSE;
    }

#ifdef MAPDB_SQLITE
    /* At least try to open a transaction. */
    sqlite3_step(repo->stmt_trans_begin);
    sqlite3_reset(repo->stmt_trans_begin);

    /* First, check if we need to remove a count from the dups table. */
    /* Then, attempt to delete map from database. */
    if(!mapdb_checkdec(repo, zoom, tilex, tiley)
    || SQLITE_OK != sqlite3_bind_int(repo->stmt_map_delete, 1, zoom)
    || SQLITE_OK != sqlite3_bind_int(repo->stmt_map_delete, 2, tilex)
    || SQLITE_OK != sqlite3_bind_int(repo->stmt_map_delete, 3, tiley)
    || SQLITE_DONE != sqlite3_step(repo->stmt_map_delete))
    {
        success = FALSE;
        printf("Error in stmt_map_delete: %s\n", 
                    sqlite3_errmsg(repo->db));
    }
    sqlite3_reset(repo->stmt_map_delete);

    if(success)
    {
        sqlite3_step(repo->stmt_trans_commit);
        sqlite3_reset(repo->stmt_trans_commit);
    }
    else
    {
        sqlite3_step(repo->stmt_trans_rollback);
        sqlite3_reset(repo->stmt_trans_rollback);
    }
#else
    {
        datum d;
        gint32 key[] = {
            GINT32_TO_BE(zoom),
            GINT32_TO_BE(tilex),
            GINT32_TO_BE(tiley)
        };
        d.dptr = (gchar*)&key;
        d.dsize = sizeof(key);
        success = !gdbm_delete(repo->db, d);
    }
#endif
    g_mutex_unlock(_mapdb_mutex);

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

/* Returns the directory containing the given database filename, or NULL
 * if the database file could not be created. */
static gboolean
repo_make_db(RepoData *rd)
{
    printf("%s(%s)\n", __PRETTY_FUNCTION__, rd->db_filename);
    gchar *db_dirname;
    gint fd;

    db_dirname = g_path_get_dirname(rd->db_filename);
    
    /* Check if db_filename is a directory and ask to upgrade. */
    if(g_file_test(rd->db_filename, G_FILE_TEST_IS_DIR))
    {
        gchar buffer[BUFFER_SIZE];
        gchar *new_name = g_strdup_printf("%s.db", rd->db_filename);
        g_free(rd->db_filename);
        rd->db_filename = new_name;

        snprintf(buffer, sizeof(buffer), "%s",
                _("The current repository is in a legacy format and will "
                    "be converted.  You should delete your old maps if you "
                    "no longer plan to use them."));
        popup_error(_window, buffer);
    }

    if(g_mkdir_with_parents(db_dirname, 0755))
    {
        g_free(db_dirname);
        return FALSE;
    }
    g_free(db_dirname);

    if(!g_file_test(rd->db_filename, G_FILE_TEST_EXISTS))
    {
        fd = g_creat(rd->db_filename, 0644);
        if(fd == -1)
        {
            g_free(db_dirname);
            return FALSE;
        }
        close(fd);
    }

    vprintf("%s(): return %d\n", __PRETTY_FUNCTION__,
           g_file_test(rd->db_filename, G_FILE_TEST_EXISTS));
    return g_file_test(rd->db_filename, G_FILE_TEST_EXISTS);
}

gboolean
repo_set_curr(RepoData *rd)
{
    printf("%s()\n", __PRETTY_FUNCTION__);
    if(!rd->db_filename || !*rd->db_filename
            || repo_make_db(rd))
    {
        if(_curr_repo)
        {
            if(_curr_repo->db)
            {
                g_mutex_lock(_mapdb_mutex);
#ifdef MAPDB_SQLITE
                sqlite3_close(_curr_repo->db);
#else
                gdbm_close(_curr_repo->db);
#endif
                _curr_repo->db = NULL;
                g_mutex_unlock(_mapdb_mutex);
            }
        }

        /* Set the current repository! */
        _curr_repo = rd;

        /* Set up the database. */
        if(_curr_repo->db_filename && *_curr_repo->db_filename)
        {

#ifdef MAPDB_SQLITE
            if(SQLITE_OK != (sqlite3_open(_curr_repo->db_filename,
                            &(_curr_repo->db)))
            /* Open worked. Now create tables, failing if they already exist.*/
            || (sqlite3_exec(_curr_repo->db,
                        "create table maps ("
                        "zoom integer, "
                        "tilex integer, "
                        "tiley integer, "
                        "pixbuf blob, "
                        "primary key (zoom, tilex, tiley))"
                        ";"
                        "create table dups ("
                        "hash integer primary key, "
                        "uses integer, "
                        "pixbuf blob)",
                        NULL, NULL, NULL), FALSE) /* !! Comma operator !! */
                /* Prepare select map statement. */
             || SQLITE_OK != sqlite3_prepare(_curr_repo->db,
                        "select pixbuf from maps "
                        "where zoom = ? and tilex = ? and tiley = ?",
                        -1, &_curr_repo->stmt_map_select, NULL)
                /* Prepare exists map statement. */
             || SQLITE_OK != sqlite3_prepare(_curr_repo->db,
                        "select count(*) from maps "
                        "where zoom = ? and tilex = ? and tiley = ?",
                        -1, &_curr_repo->stmt_map_exists, NULL)
                /* Prepare insert map statement. */
             || SQLITE_OK != sqlite3_prepare(_curr_repo->db,
                        "insert into maps (pixbuf, zoom, tilex, tiley)"
                        " values (?, ?, ?, ?)",
                        -1, &_curr_repo->stmt_map_insert, NULL)
                /* Prepare update map statement. */
             || SQLITE_OK != sqlite3_prepare(_curr_repo->db,
                        "update maps set pixbuf = ? "
                        "where zoom = ? and tilex = ? and tiley = ?",
                        -1, &_curr_repo->stmt_map_update, NULL)
                /* Prepare delete map statement. */
             || SQLITE_OK != sqlite3_prepare(_curr_repo->db,
                        "delete from maps "
                        "where zoom = ? and tilex = ? and tiley = ?",
                        -1, &_curr_repo->stmt_map_delete, NULL)

                /* Prepare select-by-map dup statement. */
                /* Prepare select-by-hash dup statement. */
             || SQLITE_OK != sqlite3_prepare(_curr_repo->db,
                        "select pixbuf from dups "
                        "where hash = ?",
                        -1, &_curr_repo->stmt_dup_select, NULL)
                /* Prepare exists map statement. */
             || SQLITE_OK != sqlite3_prepare(_curr_repo->db,
                        "select count(*) from dups "
                        "where hash = ?",
                        -1, &_curr_repo->stmt_dup_exists, NULL)
                /* Prepare insert dup statement. */
             || SQLITE_OK != sqlite3_prepare(_curr_repo->db,
                        "insert into dups (hash, pixbuf, uses) "
                        "values (?, ?, 1)",
                        -1, &_curr_repo->stmt_dup_insert, NULL)
                /* Prepare increment dup statement. */
             || SQLITE_OK != sqlite3_prepare(_curr_repo->db,
                        "update dups "
                        "set uses = uses + 1 "
                        "where hash = ?",
                        -1, &_curr_repo->stmt_dup_increm, NULL)
                /* Prepare decrement dup statement. */
             || SQLITE_OK != sqlite3_prepare(_curr_repo->db,
                        "update dups "
                        "set uses = uses - 1 "
                        "where hash = ? ",
                        -1, &_curr_repo->stmt_dup_decrem, NULL)
                /* Prepare delete dup statement. */
             || SQLITE_OK != sqlite3_prepare(_curr_repo->db,
                        "delete from dups "
                        "where hash = ? and uses <= 0",
                        -1, &_curr_repo->stmt_dup_delete, NULL)

                /* Prepare begin-transaction statement. */
             || SQLITE_OK != sqlite3_prepare(_curr_repo->db,
                     "begin transaction",
                        -1, &_curr_repo->stmt_trans_begin, NULL)
             || SQLITE_OK != sqlite3_prepare(_curr_repo->db,
                     "commit transaction",
                        -1, &_curr_repo->stmt_trans_commit, NULL)
             || SQLITE_OK != sqlite3_prepare(_curr_repo->db,
                     "rollback transaction", -1,
                     &_curr_repo->stmt_trans_rollback, NULL))
            {
                gchar buffer[BUFFER_SIZE];
                snprintf(buffer, sizeof(buffer), "%s: %s\n%s",
                        _("Failed to open map database for repository"),
                        sqlite3_errmsg(_curr_repo->db),
                        _("Downloaded maps will not be cached."));
                sqlite3_close(_curr_repo->db);
                _curr_repo->db = NULL;
                popup_error(_window, buffer);
            }
#else
            _curr_repo->db = gdbm_open(_curr_repo->db_filename,
                    0, GDBM_WRCREAT, 0644, NULL);
            if(!_curr_repo->db)
            {
                gchar buffer[BUFFER_SIZE];
                snprintf(buffer, sizeof(buffer), "%s\n%s",
                        _("Failed to open map database for repository"),
                        _("Downloaded maps will not be cached."));
                _curr_repo->db = NULL;
                popup_error(_window, buffer);
            }
#endif
        }
        else
        {
            _curr_repo->db = NULL;
        }
        vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
        return TRUE;
    }
    else
    {
        gchar buffer[BUFFER_SIZE];
        snprintf(buffer, sizeof(buffer), "%s: %s",
                _("Unable to create map database for repository"), rd->name);
        popup_error(_window, buffer);
        _curr_repo = rd;
        vprintf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
        return FALSE;
    }
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
    vprintf("%s()\n", __PRETTY_FUNCTION__);
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
    if(!_download_banner && _num_downloads != _curr_download)
        _download_banner = hildon_banner_show_progress(
                _window, NULL, _("Processing Maps"));
    return FALSE;
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
    MapUpdateTask *mut;
    MapUpdateTask *old_mut;
    gboolean is_replacing = FALSE;
    vprintf("%s(%s, %d, %d, %d, %d)\n", __PRETTY_FUNCTION__,
            repo->name, zoom, tilex, tiley, update_type);

    mut = g_slice_new(MapUpdateTask);
    if(!mut)
    {
        /* Could not allocate memory. */
        g_printerr("Out of memory in allocation of update task #%d\n",
                _num_downloads + 1);
        return FALSE;
    }
    mut->zoom = zoom;
    mut->tilex = tilex;
    mut->tiley = tiley;
    mut->update_type = update_type;

    /* Lock the mutex if this is an auto-update. */
    if(update_type == MAP_UPDATE_AUTO)
        g_mutex_lock(_mut_priority_mutex);
    if(NULL != (old_mut = g_hash_table_lookup(_mut_exists_table, mut)))
    {
        /* Check if new mut is in a newer batch that the old mut.
         * We use vfs_result to indicate a MUT that is already in the process
         * of being downloaded. */
        if(old_mut->batch_id < batch_id && old_mut->vfs_result < 0)
        {
            /* It is, so remove the old one so we can re-add this one. */
            g_hash_table_remove(_mut_exists_table, old_mut);
            g_tree_remove(_mut_priority_tree, old_mut);
            g_slice_free(MapUpdateTask, old_mut);
            is_replacing = TRUE;
        }
        else
        {
            /* It's not, so just ignore it. */
            if(update_type == MAP_UPDATE_AUTO)
                g_mutex_unlock(_mut_priority_mutex);
            g_slice_free(MapUpdateTask, mut);
            vprintf("%s(): return FALSE (1)\n", __PRETTY_FUNCTION__);
            return FALSE;
        }
    }

    g_hash_table_insert(_mut_exists_table, mut, mut);

    mut->repo = repo;
    mut->refresh_latch = refresh_latch;
    mut->priority = priority;
    mut->batch_id = batch_id;
    mut->pixbuf = NULL;
    mut->vfs_result = -1;

    g_tree_insert(_mut_priority_tree, mut, mut);

    /* Unlock the mutex if this is an auto-update. */
    if(update_type == MAP_UPDATE_AUTO)
        g_mutex_unlock(_mut_priority_mutex);

    if(!is_replacing)
    {
        /* Increment download count and (possibly) display banner. */
        if(++_num_downloads == 20 && !_download_banner)
            g_idle_add((GSourceFunc)mapdb_initiate_update_banner_idle, NULL);

        if(_num_downloads <= NUM_DOWNLOAD_THREADS)
            g_thread_pool_push(_mut_thread_pool, (gpointer)1, NULL);
    }

    vprintf("%s(): return FALSE (2)\n", __PRETTY_FUNCTION__);
    return FALSE;
}

static gboolean
get_next_mut(gpointer key, gpointer value, MapUpdateTask **data)
{
    *data = key;
    return TRUE;
}

gboolean
thread_proc_mut()
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    /* Make sure things are inititalized. */
    gnome_vfs_init();

    while(TRUE)
    {
        gint retries;
        gboolean refresh_sent = FALSE;
        MapUpdateTask *mut = NULL;

        /* Wait until we are connected. */
        conic_ensure_connected();

        /* Get the next MUT from the mut tree. */
        g_mutex_lock(_mut_priority_mutex);
        g_tree_foreach(_mut_priority_tree, (GTraverseFunc)get_next_mut, &mut);
        if(!mut)
        {
            /* No more MUTs to process.  Return. */
            g_mutex_unlock(_mut_priority_mutex);
            return FALSE;
        }
        /* Mark this MUT as "in-progress". */
        mut->vfs_result = GNOME_VFS_NUM_ERRORS;
        g_tree_remove(_mut_priority_tree, mut);
        g_mutex_unlock(_mut_priority_mutex);

        printf("%s(%s, %d, %d, %d)\n", __PRETTY_FUNCTION__,
                mut->repo->name, mut->zoom, mut->tilex, mut->tiley);

        if(mut->repo != _curr_repo)
        {
            /* Do nothing, except report that there is no error. */
            mut->vfs_result = GNOME_VFS_OK;
        }
        else if(mut->update_type == MAP_UPDATE_DELETE)
        {
            /* Easy - just delete the entry from the database.  We don't care
             * about failures (sorry). */
            if(mut->repo->db)
                mapdb_delete(mut->repo, mut->zoom, mut->tilex, mut->tiley);

            /* Report that there is no error. */
            mut->vfs_result = GNOME_VFS_OK;
        }
        else for(retries = INITIAL_DOWNLOAD_RETRIES; retries > 0; --retries)
        {
            gboolean exists = FALSE;
            gchar *src_url;
            gchar *bytes;
            gint size;
            GdkPixbufLoader *loader;
            RepoData *repo;
            gint zoom, tilex, tiley;
            GError *error = NULL;

#ifdef MAPDB_SQLITE
            /* First check for existence. */
            exists = mut->repo->db
                ? mapdb_exists(mut->repo, mut->zoom,
                        mut->tilex, mut->tiley)
                : FALSE;
            if(exists && mut->update_type == MAP_UPDATE_ADD)
            {
                /* Map already exists, and we're not going to overwrite. */
                /* Report that there is no error. */
                mut->vfs_result = GNOME_VFS_OK;
                break;
            }
#else
            /* First check for existence. */
            if(mut->update_type == MAP_UPDATE_ADD)
            {
                /* We don't want to overwrite, so check for existence. */
                /* Map already exists, and we're not going to overwrite. */
                if(mapdb_exists(mut->repo, mut->zoom,
                            mut->tilex,mut->tiley))
                {
                    /* Report that there is no error. */
                    mut->vfs_result = GNOME_VFS_OK;
                    break;
                }
            }
#endif

            /* First, construct the URL from which we will get the data. */
            src_url = map_construct_url(mut->repo, mut->zoom,
                    mut->tilex, mut->tiley);

            /* Now, attempt to read the entire contents of the URL. */
            mut->vfs_result = gnome_vfs_read_entire_file(
                    src_url, &size, &bytes);
            g_free(src_url);
            if(mut->vfs_result != GNOME_VFS_OK || !bytes)
            {
                /* Try again. */
                printf("Error reading URL: %s\n",
                        gnome_vfs_result_to_string(mut->vfs_result));
                g_free(bytes);
                continue;
            }
            /* usleep(100000); DEBUG */

            /* Attempt to parse the bytes into a pixbuf. */
            loader = gdk_pixbuf_loader_new();
            gdk_pixbuf_loader_write(loader, bytes, size, NULL);
            gdk_pixbuf_loader_close(loader, &error);
            if(error || (NULL == (mut->pixbuf = g_object_ref(
                        gdk_pixbuf_loader_get_pixbuf(loader)))))
            {
                mut->vfs_result = GNOME_VFS_NUM_ERRORS;
                if(mut->pixbuf)
                    g_object_unref(mut->pixbuf);
                mut->pixbuf = NULL;
                g_free(bytes);
                g_object_unref(loader);
                printf("Error parsing pixbuf: %s\n",
                        error ? error->message : "?");
                continue;
            }
            g_object_unref(loader);

            /* Copy database-relevant mut data before we release it. */
            repo = mut->repo;
            zoom = mut->zoom;
            tilex = mut->tilex;
            tiley = mut->tiley;

            /* Pass the mut to the GTK thread for redrawing, but only if a
             * redraw isn't already in the pipeline. */
            if(mut->refresh_latch)
            {
                /* Wait until the latch is open. */
                g_mutex_lock(mut->refresh_latch->mutex);
                while(!mut->refresh_latch->is_open)
                {
                    g_cond_wait(mut->refresh_latch->cond,
                            mut->refresh_latch->mutex);
                }
                /* Latch is open.  Decrement the number of waiters and
                 * check if we're the last waiter to run. */
                if(mut->refresh_latch->is_done_adding_tasks)
                {
                    if(++mut->refresh_latch->num_done
                                == mut->refresh_latch->num_tasks)
                    {
                        /* Last waiter.  Free the latch resources. */
                        g_mutex_unlock(mut->refresh_latch->mutex);
                        g_cond_free(mut->refresh_latch->cond);
                        g_mutex_free(mut->refresh_latch->mutex);
                        g_slice_free(ThreadLatch, mut->refresh_latch);
                        mut->refresh_latch = NULL;
                    }
                    else
                    {
                        /* Not the last waiter. Signal the next waiter.*/
                        g_cond_signal(mut->refresh_latch->cond);
                        g_mutex_unlock(mut->refresh_latch->mutex);
                    }
                }
                else
                    g_mutex_unlock(mut->refresh_latch->mutex);
            }

            g_idle_add_full(G_PRIORITY_HIGH_IDLE,
                    (GSourceFunc)map_download_refresh_idle, mut, NULL);
            refresh_sent = TRUE;

            /* DO NOT USE mut FROM THIS POINT ON. */

            /* Also attempt to add to the database. */
            mapdb_update(exists, repo, zoom,
                    tilex, tiley, bytes, size);

            /* Success! */
            g_free(bytes);
            break;
        }

        if(!refresh_sent)
            g_idle_add_full(G_PRIORITY_HIGH_IDLE,
                    (GSourceFunc)map_download_refresh_idle, mut, NULL);
    }

    vprintf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
    return FALSE;
}

guint
mut_exists_hashfunc(const MapUpdateTask *a)
{
    gint sum = a->zoom + a->tilex + a->tiley + a->update_type;
    return g_int_hash(&sum);
}

gboolean
mut_exists_equalfunc(const MapUpdateTask *a, const MapUpdateTask *b)
{
    return (a->tilex == b->tilex
            && a->tiley == b->tiley
            && a->zoom == b->zoom
            && a->update_type == b->update_type);
}

gint
mut_priority_comparefunc(const MapUpdateTask *a, const MapUpdateTask *b)
{
    /* The update_type enum is sorted in order of ascending priority. */
    gint diff = (b->update_type - a->update_type);
    if(diff)
        return diff;
    diff = (b->batch_id - a->batch_id); /* More recent ones first. */
    if(diff)
        return diff;
    diff = (a->priority - b->priority); /* Lower priority numbers first. */
    if(diff)
        return diff;

    /* At this point, we don't care, so just pick arbitrarily. */
    diff = (a->tilex - b->tilex);
    if(diff)
        return diff;
    diff = (a->tiley - b->tiley);
    if(diff)
        return diff;
    return (a->zoom - b->zoom);
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
repoman_dialog_add_repo(RepoManInfo *rmi, gchar *name)
{
    GtkWidget *vbox;
    GtkWidget *table;
    GtkWidget *label;
    GtkWidget *hbox;
    RepoEditInfo *rei = g_new(RepoEditInfo, 1);
    printf("%s(%s)\n", __PRETTY_FUNCTION__, name);

    rei->name = name;

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

    /* Initialize cache dir */
    {
        gchar buffer[BUFFER_SIZE];
        snprintf(buffer, sizeof(buffer), "%s.db", name);
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

    gtk_widget_show_all(vbox);

    gtk_combo_box_append_text(GTK_COMBO_BOX(rmi->cmb_repos), name);
    gtk_combo_box_set_active(GTK_COMBO_BOX(rmi->cmb_repos),
            gtk_tree_model_iter_n_children(GTK_TREE_MODEL(
                    gtk_combo_box_get_model(GTK_COMBO_BOX(rmi->cmb_repos))),
                NULL) - 1);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return rei;
}

static gboolean
repoman_dialog_new(GtkWidget *widget, RepoManInfo *rmi)
{
    static GtkWidget *hbox = NULL;
    static GtkWidget *label = NULL;
    static GtkWidget *txt_name = NULL;
    static GtkWidget *dialog = NULL;
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(dialog == NULL)
    {
        dialog = gtk_dialog_new_with_buttons(_("New Repository"),
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

    gtk_entry_set_text(GTK_ENTRY(txt_name), "");

    gtk_widget_show_all(dialog);

    while(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(dialog)))
    {
        repoman_dialog_add_repo(rmi,
                g_strdup(gtk_entry_get_text(GTK_ENTRY(txt_name))));
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
        repoman_dialog_add_repo(rmi, REPO_DEFAULT_NAME);
        gtk_entry_set_text(
                GTK_ENTRY(((RepoEditInfo*)rmi->repo_edits->data)->txt_url),
                REPO_DEFAULT_MAP_URI);

        gtk_combo_box_set_active(GTK_COMBO_BOX(rmi->cmb_repos), 0);
    }
    gtk_widget_destroy(confirm);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
repoman_download(GtkWidget *widget, RepoManInfo *rmi)
{
    GtkWidget *confirm;
    printf("%s()\n", __PRETTY_FUNCTION__);

    confirm = hildon_note_new_confirmation(
            GTK_WINDOW(rmi->dialog),
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
                    "http://www.gnuite.com/nokia770/maemo-mapper/repos.txt",
                    &size, &bytes)))
        {
            popup_error(rmi->dialog,
                    _("An error occurred while retrieving the repositories.  "
                        "The web service may be temporarily down."));
            g_printerr("Error while download repositories: %s\n",
                    gnome_vfs_result_to_string(vfs_result));
        }
        /* Parse each line as a reposotory. */
        else
        {
            for(head = bytes; head && *head; head = tail)
            {
                gchar buffer[BUFFER_SIZE];
                RepoData *rd;
                RepoEditInfo *rei;
                tail = strchr(head, '\n');
                *tail++ = '\0';

                rd = settings_parse_repo(head);
                snprintf(buffer, sizeof(buffer), "%s.db", rd->db_filename);
                rei = repoman_dialog_add_repo(
                        rmi, g_strdup(rd->name));
                /* Initialize fields with data from the RepoData object. */
                gtk_entry_set_text(GTK_ENTRY(rei->txt_url), rd->url);
                gtk_entry_set_text(GTK_ENTRY(rei->txt_db_filename), buffer);
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
            }
            g_free(bytes);
        }
    }
    gtk_widget_destroy(confirm);

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
    static GtkWidget *btn_download = NULL;
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
        ossohelp_dialog_help_enable(
                GTK_DIALOG(dialog), HELP_ID_REPOMAN, _osso);

        /* Reset button. */
        gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area),
                btn_reset = gtk_button_new_with_label(_("Reset...")));
        g_signal_connect(G_OBJECT(btn_reset), "clicked",
                          G_CALLBACK(repoman_reset), &rmi);

        /* Download button. */
        gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area),
                btn_download = gtk_button_new_with_label(_("Download...")));
        g_signal_connect(G_OBJECT(btn_download), "clicked",
                          G_CALLBACK(repoman_download), &rmi);

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
        RepoEditInfo *rei = repoman_dialog_add_repo(&rmi, g_strdup(rd->name));

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
            RepoData *rd = g_new(RepoData, 1);
            rd->name = g_strdup(rei->name);
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
                        mapdb_initiate_update(_curr_repo, z, tilex, tiley,
                                update_type, download_batch_id,
                                (abs(tilex - unit2tile(_next_center.unitx))
                                 + abs(tiley - unit2tile(_next_center.unity))),
                                NULL);
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
    gchar buffer[80];
    gdouble lat, lon;
    gint z;
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(!_curr_repo->db)
    {
        popup_error(_window, "To manage maps, you must set a valid repository "
                "database filename in the \"Manage Repositories\" dialog.");
        vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
        return TRUE;
    }

    if(dialog == NULL)
    {
        mapman_info.dialog = dialog = gtk_dialog_new_with_buttons(
                _("Manage Maps"),
                GTK_WINDOW(_window), GTK_DIALOG_MODAL,
                GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                NULL);

        /* Enable the help button. */
        ossohelp_dialog_help_enable(
                GTK_DIALOG(mapman_info.dialog), HELP_ID_MAPMAN, _osso);

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
                label = gtk_label_new(_("Latitude")),
                1, 2, 0, 1, GTK_FILL, 0, 4, 0);
        gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
        gtk_table_attach(GTK_TABLE(mapman_info.tbl_area),
                label = gtk_label_new(_("Longitude")),
                2, 3, 0, 1, GTK_FILL, 0, 4, 0);
        gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);

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
        gtk_table_attach(GTK_TABLE(mapman_info.tbl_area),
                lbl_gps_lon = gtk_label_new(""),
                2, 3, 1, 2, GTK_FILL, 0, 4, 0);
        gtk_label_set_selectable(GTK_LABEL(lbl_gps_lon), TRUE);
        gtk_misc_set_alignment(GTK_MISC(lbl_gps_lon), 1.f, 0.5f);

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
        gtk_table_attach(GTK_TABLE(mapman_info.tbl_area),
                lbl_center_lon = gtk_label_new(""),
                2, 3, 2, 3, GTK_FILL, 0, 4, 0);
        gtk_label_set_selectable(GTK_LABEL(lbl_center_lon), TRUE);
        gtk_misc_set_alignment(GTK_MISC(lbl_center_lon), 1.f, 0.5f);

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
        g_object_set(G_OBJECT(mapman_info.txt_topleft_lat),
                HILDON_INPUT_MODE_HINT,
                HILDON_INPUT_MODE_HINT_ALPHANUMERICSPECIAL, NULL);
        g_object_set(G_OBJECT(mapman_info.txt_topleft_lat),
                HILDON_AUTOCAP,
                FALSE, NULL);
        gtk_table_attach(GTK_TABLE(mapman_info.tbl_area),
                mapman_info.txt_topleft_lon = gtk_entry_new(),
                2, 3, 3, 4, GTK_FILL, 0, 4, 0);
        gtk_entry_set_width_chars(GTK_ENTRY(mapman_info.txt_topleft_lon), 12);
        gtk_entry_set_alignment(GTK_ENTRY(mapman_info.txt_topleft_lon), 1.f);
        g_object_set(G_OBJECT(mapman_info.txt_topleft_lon),
                HILDON_INPUT_MODE_HINT,
                HILDON_INPUT_MODE_HINT_ALPHANUMERICSPECIAL, NULL);
        g_object_set(G_OBJECT(mapman_info.txt_topleft_lon),
                HILDON_AUTOCAP,
                FALSE, NULL);

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
        g_object_set(G_OBJECT(mapman_info.txt_botright_lat),
                HILDON_INPUT_MODE_HINT,
                HILDON_INPUT_MODE_HINT_ALPHANUMERICSPECIAL, NULL);
        g_object_set(G_OBJECT(mapman_info.txt_botright_lat),
                HILDON_AUTOCAP,
                FALSE, NULL);
        gtk_table_attach(GTK_TABLE(mapman_info.tbl_area),
                mapman_info.txt_botright_lon = gtk_entry_new(),
                2, 3, 4, 5, GTK_FILL, 0, 4, 0);
        gtk_entry_set_width_chars(GTK_ENTRY(mapman_info.txt_botright_lat), 12);
        gtk_entry_set_alignment(GTK_ENTRY(mapman_info.txt_botright_lon), 1.f);
        g_object_set(G_OBJECT(mapman_info.txt_botright_lon),
                HILDON_INPUT_MODE_HINT,
                HILDON_INPUT_MODE_HINT_ALPHANUMERICSPECIAL, NULL);
        g_object_set(G_OBJECT(mapman_info.txt_botright_lon),
                HILDON_AUTOCAP,
                FALSE, NULL);

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

    lat_format(_gps.lat, buffer);
    gtk_label_set_text(GTK_LABEL(lbl_gps_lat), buffer);
    lon_format(_gps.lon, buffer);
    gtk_label_set_text(GTK_LABEL(lbl_gps_lon), buffer);

    unit2latlon(_center.unitx, _center.unity, lat, lon);
    lat_format(lat, buffer);
    gtk_label_set_text(GTK_LABEL(lbl_center_lat), buffer);
    lon_format(lon, buffer);
    gtk_label_set_text(GTK_LABEL(lbl_center_lon), buffer);

    /* Initialize to the bounds of the screen. */
    unit2latlon(
            _center.unitx - pixel2unit(MAX(_screen_width_pixels,
                    _screen_height_pixels) / 2),
            _center.unity - pixel2unit(MAX(_screen_width_pixels,
                    _screen_height_pixels) / 2), lat, lon);
    BOUND(lat, -90.f, 90.f);
    BOUND(lon, -180.f, 180.f);
    lat_format(lat, buffer);
    gtk_entry_set_text(GTK_ENTRY(mapman_info.txt_topleft_lat), buffer);
    lon_format(lon, buffer);
    gtk_entry_set_text(GTK_ENTRY(mapman_info.txt_topleft_lon), buffer);

    unit2latlon(
            _center.unitx + pixel2unit(MAX(_screen_width_pixels,
                    _screen_height_pixels) / 2),
            _center.unity + pixel2unit(MAX(_screen_width_pixels,
                    _screen_height_pixels) / 2), lat, lon);
    BOUND(lat, -90.f, 90.f);
    BOUND(lon, -180.f, 180.f);
    lat_format(lat, buffer);
    gtk_entry_set_text(GTK_ENTRY(mapman_info.txt_botright_lat), buffer);
    lon_format(lon, buffer);
    gtk_entry_set_text(GTK_ENTRY(mapman_info.txt_botright_lon), buffer);

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
            const gchar *text;
            gchar *error_check;
            gdouble start_lat, start_lon, end_lat, end_lon;

            text = gtk_entry_get_text(GTK_ENTRY(mapman_info.txt_topleft_lat));
            start_lat = strdmstod(text, &error_check);
            if(text == error_check || start_lat < -90. || start_lat > 90.) {
                popup_error(dialog, _("Invalid Top-Left Latitude"));
                continue;
            }

            text = gtk_entry_get_text(GTK_ENTRY(mapman_info.txt_topleft_lon));
            start_lon = strdmstod(text, &error_check);
            if(text == error_check || start_lon < -180. || start_lon>180.) {
                popup_error(dialog, _("Invalid Top-Left Longitude"));
                continue;
            }

            text = gtk_entry_get_text(GTK_ENTRY(mapman_info.txt_botright_lat));
            end_lat = strdmstod(text, &error_check);
            if(text == error_check || end_lat < -90. || end_lat > 90.) {
                popup_error(dialog, _("Invalid Bottom-Right Latitude"));
                continue;
            }

            text = gtk_entry_get_text(GTK_ENTRY(mapman_info.txt_botright_lon));
            end_lon = strdmstod(text, &error_check);
            if(text == error_check || end_lon < -180. || end_lon > 180.) {
                popup_error(dialog,_("Invalid Bottom-Right Longitude"));
                continue;
            }

            if(mapman_by_area(start_lat, start_lon, end_lat, end_lon,
                        &mapman_info, update_type, download_batch_id))
                break;
        }
    }

    gtk_widget_hide(dialog);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

