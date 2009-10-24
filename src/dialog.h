/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * Copyright (C) 2009 Alberto Mardegan.
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

#ifndef MAP_DIALOG_H
#define MAP_DIALOG_H

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>
 
G_BEGIN_DECLS

#define MAP_TYPE_DIALOG         (map_dialog_get_type ())
#define MAP_DIALOG(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MAP_TYPE_DIALOG, MapDialog))
#define MAP_DIALOG_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), MAP_TYPE_DIALOG, MapDialogClass))
#define MAP_IS_DIALOG(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MAP_TYPE_DIALOG))
#define MAP_IS_DIALOG_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MAP_TYPE_DIALOG))
#define MAP_DIALOG_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MAP_TYPE_DIALOG, MapDialogClass))

typedef struct _MapDialog MapDialog;
typedef struct _MapDialogPrivate MapDialogPrivate;
typedef struct _MapDialogClass MapDialogClass;


struct _MapDialog
{
    GtkDialog parent;

    MapDialogPrivate *priv;
};

struct _MapDialogClass
{
    GtkDialogClass parent_class;
};

GType map_dialog_get_type (void);

GtkWidget *map_dialog_new(const gchar *title, GtkWindow *parent,
                          gboolean single_column);
void map_dialog_add_widget(MapDialog *self, GtkWidget *widget);
GtkWidget *map_dialog_create_button(MapDialog *self, const gchar *label);

G_END_DECLS
#endif /* MAP_DIALOG_H */
