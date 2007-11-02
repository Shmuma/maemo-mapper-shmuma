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

#ifndef MAEMO_MAPPER_MAPS_H
#define MAEMO_MAPPER_MAPS_H

#include "defines.h"

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
    GtkWidget *chk_zoom_levels[MAX_ZOOM];
};


gboolean mapdb_exists(RepoData *repo, gint zoom, gint tilex, gint tiley,
        gboolean should_lock);
GdkPixbuf* mapdb_get(RepoData *repo, gint zoom, gint tilex, gint tiley);

void set_repo_type(RepoData *repo);
gboolean repo_set_curr(RepoData *rd);

gboolean mapdb_initiate_update(RepoData *repo, gint zoom, gint tilex,
        gint tiley, gint update_type, gint batch_id, gint priority,
        ThreadLatch *refresh_latch);

guint mut_exists_hashfunc(const MapUpdateTask *a);
gboolean mut_exists_equalfunc(const MapUpdateTask *a, const MapUpdateTask *b);
gint mut_priority_comparefunc(const MapUpdateTask *a, const MapUpdateTask *b);
gboolean thread_proc_mut();

gboolean repoman_dialog();

gboolean mapman_dialog();

#endif /* ifndef MAEMO_MAPPER_MAPS_H */
