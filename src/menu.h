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

#ifndef MAEMO_MAPPER_MENU_H
#define MAEMO_MAPPER_MENU_H

gboolean menu_cb_view_goto_nextway(GtkMenuItem *item);
gboolean menu_cb_view_goto_nearpoi(GtkMenuItem *item);

void menu_maps_remove_repos(void);
void menu_maps_add_repos(void);
void menu_layers_remove_repos(void);
void menu_layers_add_repos(void);
void menu_init(void);
void map_menu_route(void);
void map_menu_track(void);
void map_menu_go_to(void);
void map_menu_view(void);

#endif /* ifndef MAEMO_MAPPER_MENU_H */
