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
#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif
#include "dialog.h"

#include <hildon/hildon-pannable-area.h>

enum
{
    PROP_0,
    PROP_SINGLE_COLUMN,
};

struct _MapDialogPrivate
{
    GtkBox *vbox;
    GtkBox *last_box;
    gboolean single_column;
};

typedef struct {
    gint response;
} MapDialogResponseData;

G_DEFINE_TYPE(MapDialog, map_dialog, GTK_TYPE_DIALOG);

#define MAP_DIALOG_PRIV(dialog) (MAP_DIALOG(dialog)->priv)

static void
response_data_free(MapDialogResponseData *rd)
{
    g_slice_free(MapDialogResponseData, rd);
}

static MapDialogResponseData *
response_data_get(GtkWidget *widget, gboolean create)
{
    MapDialogResponseData *rd;

    rd = g_object_get_data(G_OBJECT(widget), "map_dialog_response");
    if (!rd && create)
    {
        rd = g_slice_new(MapDialogResponseData);
        g_object_set_data_full((GObject *)widget, "map_dialog_response",
                               rd, (GDestroyNotify)response_data_free);
    }

    return rd;
}

static void
on_button_clicked(GtkWidget *button, GtkDialog *dialog)
{
    MapDialogResponseData *rd;

    rd = response_data_get(button, FALSE);
    if (rd)
        gtk_dialog_response(dialog, rd->response);
}

static void
map_dialog_set_property(GObject *object, guint property_id,
                        const GValue *value, GParamSpec *pspec)
{
    MapDialogPrivate *priv = MAP_DIALOG_PRIV(object);

    switch (property_id)
    {
    case PROP_SINGLE_COLUMN:
        priv->single_column = g_value_get_boolean(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

static void
on_pannable_size_request(GtkWidget *pannable, GtkRequisition *req,
                         GtkWidget *vbox)
{
    gtk_widget_get_child_requisition(vbox, req);
}

static void
map_dialog_init(MapDialog *dialog)
{
    MapDialogPrivate *priv;
    GtkWidget *pannable, *vbox;

    priv = G_TYPE_INSTANCE_GET_PRIVATE(dialog, MAP_TYPE_DIALOG, MapDialogPrivate);
    dialog->priv = priv;

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_widget_show(vbox);
    pannable = hildon_pannable_area_new();
    gtk_widget_show(pannable);
    hildon_pannable_area_add_with_viewport(HILDON_PANNABLE_AREA(pannable),
                                           vbox);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), pannable,
                       TRUE, TRUE, 0);

    priv->vbox = GTK_BOX(vbox);

    g_signal_connect(pannable, "size-request",
                     G_CALLBACK(on_pannable_size_request), vbox);
}

static void
map_dialog_class_init(MapDialogClass * klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(object_class, sizeof(MapDialogPrivate));

    object_class->set_property = map_dialog_set_property;

    g_object_class_install_property(object_class, PROP_SINGLE_COLUMN,
        g_param_spec_boolean("single-column", "single-column", "single-column",
                             TRUE,
                             G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
                             G_PARAM_STATIC_STRINGS));
}

GtkWidget *
map_dialog_new(const gchar *title, GtkWindow *parent, gboolean single_column)
{
    return g_object_new(MAP_TYPE_DIALOG,
                        "title", title,
                        "modal", TRUE,
                        "has-separator", FALSE,
                        "transient-for", parent,
                        "destroy-with-parent", TRUE,
                        "single-column", single_column,
                        NULL);
}

void
map_dialog_add_widget(MapDialog *self, GtkWidget *widget)
{
    MapDialogPrivate *priv;
    GtkBox *box;

    g_return_if_fail(MAP_IS_DIALOG(self));
    priv = self->priv;

    box = priv->vbox;

    if (priv->single_column)
    {
        gtk_box_pack_start(box, widget, FALSE, TRUE, 0);
    }
    else
    {
        if (priv->last_box)
        {
            gtk_box_pack_start(priv->last_box, widget, FALSE, TRUE, 0);
            priv->last_box = NULL;
        }
        else
        {
            GtkWidget *hbox;
            hbox = gtk_hbox_new(TRUE, 0);
            gtk_widget_show(hbox);
            gtk_box_pack_start(box, hbox, FALSE, TRUE, 0);
            priv->last_box = GTK_BOX(hbox);
            gtk_box_pack_start(priv->last_box, widget, FALSE, TRUE, 0);
        }
    }
}

GtkWidget *
map_dialog_create_button(MapDialog *self, const gchar *label, gint response)
{
    MapDialogResponseData *rd;
    GtkWidget *button;

    g_return_val_if_fail(MAP_IS_DIALOG(self), NULL);
    button = gtk_button_new_with_label(label);
    hildon_gtk_widget_set_theme_size(button, HILDON_SIZE_FINGER_HEIGHT);
    gtk_widget_show(button);
    map_dialog_add_widget(self, button);

    if (response != GTK_RESPONSE_NONE)
    {
        rd = response_data_get(button, TRUE);
        rd->response = response;
        g_signal_connect(button, "clicked",
                         G_CALLBACK(on_button_clicked), self);
    }
    return button;
}

