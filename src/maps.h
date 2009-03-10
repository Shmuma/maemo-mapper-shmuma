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

size_t map_cache_resize(size_t cache_size);
void map_cache_clean (void);

void maps_init();
void maps_destroy();

RepoData *create_default_repo();

gboolean mapdb_exists(RepoData *repo, gint zoom, gint tilex, gint tiley);
GdkPixbuf* mapdb_get(RepoData *repo, gint zoom, gint tilex, gint tiley);

void set_repo_type(RepoData *repo);
gboolean repo_set_curr(RepoData *rd);
gboolean repo_is_layer (RepoData* base, RepoData* layer);

gboolean mapdb_initiate_update(RepoData *repo, gint zoom, gint tilex,
        gint tiley, gint update_type, gint batch_id, gint priority,
        ThreadLatch *refresh_latch);

guint mut_exists_hashfunc(const MapUpdateTask *a);
gboolean mut_exists_equalfunc(const MapUpdateTask *a, const MapUpdateTask *b);
gint mut_priority_comparefunc(const MapUpdateTask *a, const MapUpdateTask *b);
gboolean thread_proc_mut(void);

gboolean repoman_dialog(void);

gboolean repoman_download(void);

gboolean mapman_dialog(void);

gboolean map_layer_refresh_cb (gpointer data);

void maps_toggle_visible_layers ();

/* returns amount of seconds since tile downloaded */
gint get_tile_age (GdkPixbuf* pixbuf);

#endif /* ifndef MAEMO_MAPPER_MAPS_H */
