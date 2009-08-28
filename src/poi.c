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

#include <string.h>
#include <math.h>

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

#include <sqlite3.h>

#include "types.h"
#include "data.h"
#include "defines.h"

#include "display.h"
#include "gdk-pixbuf-rotate.h"
#include "gpx.h"
#include "main.h"
#include "poi.h"
#include "util.h"

static sqlite3 *_poi_db = NULL;
static sqlite3_stmt *_stmt_browse_poi = NULL;
static sqlite3_stmt *_stmt_browsecat_poi = NULL;
static sqlite3_stmt *_stmt_select_poi = NULL;
static sqlite3_stmt *_stmt_select_nearest_poi = NULL;
static sqlite3_stmt *_stmt_insert_poi = NULL;
static sqlite3_stmt *_stmt_update_poi = NULL;
static sqlite3_stmt *_stmt_delete_poi = NULL;
static sqlite3_stmt *_stmt_delete_poi_by_catid = NULL;
static sqlite3_stmt *_stmt_nextlabel_poi = NULL;
static sqlite3_stmt *_stmt_select_cat = NULL;
static sqlite3_stmt *_stmt_insert_cat = NULL;
static sqlite3_stmt *_stmt_update_cat = NULL;
static sqlite3_stmt *_stmt_delete_cat = NULL;
static sqlite3_stmt *_stmt_toggle_cat = NULL;
static sqlite3_stmt *_stmt_selall_cat = NULL;

typedef struct _PoiListInfo PoiListInfo;
struct _PoiListInfo
{
    GtkWidget *dialog;
    GtkWidget *dialog2;
    GtkTreeViewColumn *select_column;
    GtkWidget *tree_view;
    gboolean select_all;
};

typedef struct _OriginToggleInfo OriginToggleInfo;
struct _OriginToggleInfo {
    GtkWidget *rad_use_gps;
    GtkWidget *rad_use_route;
    GtkWidget *rad_use_text;
    GtkWidget *txt_origin;
    GtkWidget *txt_query;
};

typedef struct _PoiCategoryEditInfo PoiCategoryEditInfo;
struct _PoiCategoryEditInfo
{
    GtkWidget *dialog;
    GtkWidget *cmb_category;
    gint cat_id;
    GtkWidget *tree_view;
};

/** Data used during action: add or edit category/poi **/
typedef struct _DeletePOI DeletePOI;
struct _DeletePOI {
    GtkWidget *dialog;
    gchar *txt_label;
    gint id;
    gboolean deleted;
};

void
poi_db_connect()
{
    gchar buffer[100];
    gchar **pszResult;
    gint nRow, nColumn;
    gchar *db_dirname = NULL;
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(_poi_db)
    {
        sqlite3_close(_poi_db);
        _poi_db = NULL;
    }

    if(!_poi_db_filename)
    {
        /* Do nothing. */
    }
    else if(NULL == (db_dirname = g_path_get_dirname(_poi_db_filename))
            || (g_mkdir_with_parents(db_dirname, 0755), /* comma operator */
                (SQLITE_OK != (sqlite3_open(_poi_db_filename, &_poi_db)))))
    {
        gchar buffer2[BUFFER_SIZE];
        snprintf(buffer2, sizeof(buffer2),
                "%s: %s", _("Error with POI database"),
                sqlite3_errmsg(_poi_db));
        sqlite3_close(_poi_db);
        _poi_db = NULL;
        popup_error(_window, buffer2);
    }
    else if(SQLITE_OK != sqlite3_get_table(_poi_db,
                "select label from poi limit 1",
                &pszResult, &nRow, &nColumn, NULL))
    {
        gchar *create_sql = sqlite3_mprintf(
                "create table poi (poi_id integer PRIMARY KEY, lat real, "
                "lon real, label text, desc text, cat_id integer);"
                "create table category (cat_id integer PRIMARY KEY,"
                "label text, desc text, enabled integer);"
                /* Add some default categories... */
                "insert into category (label, desc, enabled) "
                    "values ('%q', '%q', 1); "
                "insert into category (label, desc, enabled) "
                    "values ('%q', '%q', 1); "
                "insert into category (label, desc, enabled) "
                    "values ('%q', '%q', 1); "
                "insert into category (label, desc, enabled) "
                    "values ('%q', '%q', 1); "
                "insert into category (label, desc, enabled) "
                    "values ('%q', '%q', 1); "
                "insert into category (label, desc, enabled) "
                    "values ('%q', '%q', 1); "
                "insert into category (label, desc, enabled) "
                    "values ('%q', '%q', 1); "
                "insert into category (label, desc, enabled) "
                    "values ('%q', '%q', 1); "
                "insert into category (label, desc, enabled) "
                    "values ('%q', '%q', 1); "
                "insert into category (label, desc, enabled) "
                    "values ('%q', '%q', 1); "
                "insert into category (label, desc, enabled) "
                    "values ('%q', '%q', 1); ",
                _("Service Station"),
                _("Stations for purchasing fuel for vehicles."),
                _("Residence"),
                _("Houses, apartments, or other residences of import."),
                _("Restaurant"),
                _("Places to eat or drink."),
                _("Shopping/Services"),
                _("Places to shop or acquire services."),
                _("Recreation"),
                _("Indoor or Outdoor places to have fun."),
                _("Transportation"),
                _("Bus stops, airports, train stations, etc."),
                _("Lodging"),
                _("Places to stay temporarily or for the night."),
                _("School"),
                _("Elementary schools, college campuses, etc."),
                _("Business"),
                _("General places of business."),
                _("Landmark"),
                _("General landmarks."),
                _("Other"),
                _("Miscellaneous category for everything else."));

        if(SQLITE_OK != sqlite3_exec(_poi_db, create_sql, NULL, NULL, NULL)
                && (SQLITE_OK != sqlite3_get_table(_poi_db,
                        "select label from poi limit 1",
                        &pszResult, &nRow, &nColumn, NULL)))
        {
            snprintf(buffer, sizeof(buffer), "%s:\n%s",
                    _("Failed to open or create database"),
                    sqlite3_errmsg(_poi_db));
            sqlite3_close(_poi_db);
            _poi_db = NULL;
            popup_error(_window, buffer);
        }
    }
    else
        sqlite3_free_table(pszResult);

    g_free(db_dirname);

    if(_poi_db)
    {
        /* Prepare our SQL statements. */
        /* browse poi */
        sqlite3_prepare(_poi_db,
                        "select p.poi_id, p.cat_id, p.lat, p.lon,"
                        " p.label, p.desc, c.label"
                        " from poi p inner join category c"
                        "   on p.cat_id = c.cat_id"
                        " where c.enabled = 1"
                        " and p.label like $QUERY or p.desc like $QUERY"
                        " order by (($LAT - p.lat) * ($LAT - p.lat) "
                                 "+ ($LON - p.lon) * ($LON - p.lon)) DESC "
                        " limit 100",
                        -1, &_stmt_browse_poi, NULL);

        /* browse poi by category */
        sqlite3_prepare(_poi_db,
                        "select p.poi_id, p.cat_id, p.lat, p.lon,"
                        " p.label, p.desc, c.label"
                        " from poi p inner join category c"
                        "   on p.cat_id = c.cat_id"
                        " where c.enabled = 1"
                        " and p.cat_id = $CATID"
                        " and ( p.label like $QUERY or p.desc like $QUERY )"
                        " order by (($LAT - p.lat) * ($LAT - p.lat) "
                                 "+ ($LON - p.lon) * ($LON - p.lon)) DESC"
                        " limit 100",
                        -1, &_stmt_browsecat_poi, NULL);

        /* Prepare our SQL statements. */
        /* select from poi */
        sqlite3_prepare(_poi_db,
                        "select p.lat, p.lon, p.poi_id, p.label, p.desc,"
                        " p.cat_id, c.label, c.desc"
                        " from poi p inner join category c"
                        "    on p.cat_id = c.cat_id"
                        " where c.enabled = 1"
                        " and p.lat between ? and ? "
                        " and p.lon between ? and ? ",
                        -1, &_stmt_select_poi, NULL);

        /* select nearest pois */
        sqlite3_prepare(_poi_db,
                        "select p.poi_id, p.cat_id, p.lat, p.lon,"
                        " p.label, p.desc, c.label"
                        " from poi p inner join category c"
                        "   on p.cat_id = c.cat_id"
                        " where c.enabled = 1"
                        " order by (($LAT - p.lat) * ($LAT - p.lat) "
                                 "+ ($LON - p.lon) * ($LON - p.lon)) limit 1",
                        -1, &_stmt_select_nearest_poi, NULL);

        /* insert poi */
        sqlite3_prepare(_poi_db,
                            "insert into poi (lat, lon, label, desc, cat_id)"
                            " values (?, ?, ?, ?, ?)",
                        -1, &_stmt_insert_poi, NULL);
        /* update poi */
        sqlite3_prepare(_poi_db,
                            "update poi set lat = ?, lon = ?, "
                            "label = ?, desc = ?, cat_id = ? where poi_id = ?",
                        -1, &_stmt_update_poi, NULL);
        /* delete from poi */
        sqlite3_prepare(_poi_db,
                        " delete from poi where poi_id = ?",
                        -1, &_stmt_delete_poi, NULL);
        /* delete from poi by cat_id */
        sqlite3_prepare(_poi_db,
                        "delete from poi where cat_id = ?",
                        -1, &_stmt_delete_poi_by_catid, NULL);
        /* get next poilabel */
        sqlite3_prepare(_poi_db,
                        "select ifnull(max(poi_id) + 1,1) from poi",
                        -1, &_stmt_nextlabel_poi, NULL);

        /* select from category */
        sqlite3_prepare(_poi_db,
                        "select c.label, c.desc, c.enabled"
                        " from category c where c.cat_id = ?",
                        -1, &_stmt_select_cat, NULL);
        /* insert into category */
        sqlite3_prepare(_poi_db,
                        "insert into category (label, desc, enabled)"
                        " values (?, ?, ?)",
                        -1, &_stmt_insert_cat, NULL);
        /* update category */
        sqlite3_prepare(_poi_db,
                        "update category set label = ?, desc = ?,"
                        " enabled = ? where cat_id = ?",
                        -1, &_stmt_update_cat, NULL);
        /* delete from category */
        sqlite3_prepare(_poi_db,
                        "delete from category where cat_id = ?",
                        -1, &_stmt_delete_cat, NULL);
        /* enable category */
        sqlite3_prepare(_poi_db,
                        "update category set enabled = ?"
                        " where cat_id = ?",
                        -1, &_stmt_toggle_cat, NULL);
        /* select all category */
        sqlite3_prepare(_poi_db,
                        "select c.cat_id, c.label, c.desc, c.enabled,"
                        " count(p.poi_id)"
                        " from category c"
                        " left outer join poi p on c.cat_id = p.cat_id"
                        " group by c.cat_id, c.label, c.desc, c.enabled "
                        " order by c.label",
                        -1, &_stmt_selall_cat, NULL);
    }

    _poi_enabled = _poi_db != NULL;

    gtk_widget_set_sensitive(_menu_poi_item, _poi_enabled);
    gtk_widget_set_sensitive(_cmenu_loc_add_poi_item, _poi_enabled);
    gtk_widget_set_sensitive(_cmenu_loc_download_poi_item, _poi_enabled);
    gtk_widget_set_sensitive(_cmenu_loc_browse_poi_item, _poi_enabled);
    gtk_widget_set_sensitive(_cmenu_way_add_poi_item, _poi_enabled);
    gtk_widget_set_sensitive(_cmenu_poi_submenu, _poi_enabled);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

gboolean
get_nearest_poi(gint unitx, gint unity, PoiInfo *poi)
{
    printf("%s(%d, %d)\n", __PRETTY_FUNCTION__, unitx, unity);
    gboolean result;
    gdouble lat, lon;
    unit2latlon(unitx, unity, lat, lon);

    if(SQLITE_OK == sqlite3_bind_double(_stmt_select_nearest_poi, 1, lat)
    && SQLITE_OK == sqlite3_bind_double(_stmt_select_nearest_poi, 2, lon)
        && SQLITE_ROW == sqlite3_step(_stmt_select_nearest_poi))
    {
        poi->poi_id = sqlite3_column_int(_stmt_select_nearest_poi, 0);
        poi->cat_id = sqlite3_column_int(_stmt_select_nearest_poi, 1);
        poi->lat = sqlite3_column_double(_stmt_select_nearest_poi, 2);
        poi->lon = sqlite3_column_double(_stmt_select_nearest_poi, 3);
        poi->label =g_strdup(sqlite3_column_text(_stmt_select_nearest_poi, 4));
        poi->desc = g_strdup(sqlite3_column_text(_stmt_select_nearest_poi, 5));
        poi->clabel=g_strdup(sqlite3_column_text(_stmt_select_nearest_poi, 6));
        result = TRUE;
    }
    else
        result = FALSE;
    sqlite3_reset(_stmt_select_nearest_poi);
    vprintf("%s(): return %d\n", __PRETTY_FUNCTION__, result);
    return result;
}

gboolean
select_poi(gint unitx, gint unity, PoiInfo *poi, gboolean quick)
{
    gint x, y;
    gdouble lat1, lon1, lat2, lon2;
    static GtkWidget *dialog = NULL;
    static GtkWidget *list = NULL;
    static GtkWidget *sw = NULL;
    static GtkTreeViewColumn *column = NULL;
    static GtkCellRenderer *renderer = NULL;
    GtkListStore *store = NULL;
    GtkTreeIter iter;
    gboolean selected = FALSE;
    gchar tmp1[LL_FMT_LEN], tmp2[LL_FMT_LEN];
    gint num_cats = 0;
    printf("%s()\n", __PRETTY_FUNCTION__);

    x = unitx - pixel2unit(3 * _draw_width);
    y = unity + pixel2unit(3 * _draw_width);
    unit2latlon(x, y, lat1, lon1);

    x = unitx + pixel2unit(3 * _draw_width);
    y = unity - pixel2unit(3 * _draw_width);
    unit2latlon(x, y, lat2, lon2);

    if(SQLITE_OK != sqlite3_bind_double(_stmt_select_poi, 1, lat1) ||
          SQLITE_OK != sqlite3_bind_double(_stmt_select_poi, 2, lat2) ||
          SQLITE_OK != sqlite3_bind_double(_stmt_select_poi, 3, lon1) ||
          SQLITE_OK != sqlite3_bind_double(_stmt_select_poi, 4, lon2))
    {
        g_printerr("Failed to bind values for _stmt_select_poi\n");
        return FALSE;
    }

    /* Initialize store. */
    store = gtk_list_store_new(POI_NUM_COLUMNS,
                               G_TYPE_BOOLEAN,/* Selected */
                               G_TYPE_INT,    /* POI ID */
                               G_TYPE_INT,    /* Category ID */
                               G_TYPE_DOUBLE,  /* Latitude */
                               G_TYPE_DOUBLE,  /* Longitude */
                               G_TYPE_STRING, /* Lat/Lon */
                               G_TYPE_FLOAT,  /* Bearing */
                               G_TYPE_FLOAT,  /* Distance */
                               G_TYPE_STRING, /* POI Label */
                               G_TYPE_STRING, /* POI Desc. */
                               G_TYPE_STRING);/* Category Label */

    while(SQLITE_ROW == sqlite3_step(_stmt_select_poi))
    {
        gdouble lat, lon;
        lat = sqlite3_column_double(_stmt_select_poi, 0);
        lon = sqlite3_column_double(_stmt_select_poi, 1);
        
        format_lat_lon(lat, lon, tmp1, tmp2);
        //lat_format(lat, tmp1);
        //lon_format(lon, tmp2);
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                POI_POIID, sqlite3_column_int(_stmt_select_poi, 2),
                POI_CATID, sqlite3_column_int(_stmt_select_poi, 5),
                POI_LAT, lat,
                POI_LON, lon,
                POI_LATLON, g_strdup_printf("%s, %s", tmp1, tmp2),
                POI_LABEL, sqlite3_column_text(_stmt_select_poi, 3),
                POI_DESC, sqlite3_column_text(_stmt_select_poi, 4),
                POI_CLABEL, sqlite3_column_text(_stmt_select_poi, 6),
                -1);
        num_cats++;
    }
    sqlite3_reset(_stmt_select_poi);

    switch(num_cats)
    {
        case 0:
            g_object_unref(G_OBJECT(store));
            if(!quick)
            {
                MACRO_BANNER_SHOW_INFO(_window, _("No POIs found."));
            }
            return FALSE;
            break;
        case 1:
            /* iter is still set to the most-recently added POI. */
            gtk_tree_model_get(GTK_TREE_MODEL(store),
                &iter,
                POI_POIID, &(poi->poi_id),
                POI_CATID, &(poi->cat_id),
                POI_LAT, &(poi->lat),
                POI_LON, &(poi->lon),
                POI_LABEL, &(poi->label),
                POI_DESC, &(poi->desc),
                POI_CLABEL, &(poi->clabel),
                -1);
            g_object_unref(G_OBJECT(store));
            return TRUE;
            break;
        default:
            if(quick)
            {
                g_object_unref(G_OBJECT(store));
                return get_nearest_poi(unitx, unity, poi);
            }
    }

    /* There are at least 2 matching POI's - let the user select one. */
    if(dialog == NULL)
    {
        dialog = gtk_dialog_new_with_buttons(_("Select POI"),
                GTK_WINDOW(_window), GTK_DIALOG_MODAL,
                GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                NULL);

        gtk_window_set_default_size(GTK_WINDOW(dialog), 500, 300);

        sw = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw),
                GTK_SHADOW_ETCHED_IN);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                GTK_POLICY_NEVER,
                GTK_POLICY_AUTOMATIC);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
                sw, TRUE, TRUE, 0);

        list = gtk_tree_view_new();
        gtk_container_add(GTK_CONTAINER(sw), list);

        gtk_tree_selection_set_mode(
                gtk_tree_view_get_selection(GTK_TREE_VIEW(list)),
                GTK_SELECTION_SINGLE);
        gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(list), TRUE);

        renderer = gtk_cell_renderer_text_new();
        column = gtk_tree_view_column_new_with_attributes(
                _("Location"), renderer, "text", POI_LATLON, NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(list), column);

        renderer = gtk_cell_renderer_text_new();
        column = gtk_tree_view_column_new_with_attributes(
                _("Label"), renderer, "text", POI_LABEL, NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(list), column);

        renderer = gtk_cell_renderer_text_new();
        column = gtk_tree_view_column_new_with_attributes(
                _("Category"), renderer, "text", POI_CLABEL, NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(list), column);
    }

    gtk_tree_view_set_model(GTK_TREE_VIEW(list), GTK_TREE_MODEL(store));
    g_object_unref(G_OBJECT(store));

    gtk_widget_show_all(dialog);

    while(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(dialog)))
    {
        if(gtk_tree_selection_get_selected(
                    gtk_tree_view_get_selection(GTK_TREE_VIEW(list)),
                    NULL, &iter))
        {
            gtk_tree_model_get(GTK_TREE_MODEL(store), &iter,
                POI_POIID, &(poi->poi_id),
                POI_CATID, &(poi->cat_id),
                POI_LAT, &(poi->lat),
                POI_LON, &(poi->lon),
                POI_LABEL, &(poi->label),
                POI_DESC, &(poi->desc),
                POI_CLABEL, &(poi->clabel),
                -1);
            selected = TRUE;
            break;
        }
        else
            popup_error(dialog, _("Select one POI from the list."));
    }

    map_force_redraw();

    gtk_widget_hide(dialog);

    vprintf("%s(): return %d\n", __PRETTY_FUNCTION__, selected);
    return selected;
}

static gboolean
category_delete(GtkWidget *widget, DeletePOI *dpoi)
{
    GtkWidget *confirm;
    gint i;
    gchar *buffer;
    printf("%s()\n", __PRETTY_FUNCTION__);

    buffer = g_strdup_printf("%s\n\t%s\n%s",
            _("Delete category?"),
            dpoi->txt_label,
            _("WARNING: All POIs in that category will also be deleted!"));
    confirm = hildon_note_new_confirmation(GTK_WINDOW(dpoi->dialog), buffer);
    g_free(buffer);
    i = gtk_dialog_run(GTK_DIALOG(confirm));
    gtk_widget_destroy(GTK_WIDGET(confirm));

    if(i == GTK_RESPONSE_OK)
    {
        /* delete dpoi->poi_id */
        if(SQLITE_OK != sqlite3_bind_int(_stmt_delete_poi_by_catid, 1,
                    dpoi->id) ||
           SQLITE_DONE != sqlite3_step(_stmt_delete_poi_by_catid))
        {
            MACRO_BANNER_SHOW_INFO(dpoi->dialog, _("Error deleting POI"));
            sqlite3_reset(_stmt_delete_poi_by_catid);
            return FALSE;
        }
        sqlite3_reset(_stmt_delete_poi_by_catid);

        if(SQLITE_OK != sqlite3_bind_int(_stmt_delete_cat, 1, dpoi->id) ||
           SQLITE_DONE != sqlite3_step(_stmt_delete_cat))
        {
            MACRO_BANNER_SHOW_INFO(dpoi->dialog, _("Error deleting category"));
            sqlite3_reset(_stmt_delete_cat);
            return FALSE;
        }
        sqlite3_reset(_stmt_delete_cat);

        map_force_redraw();
    }
    gtk_widget_destroy(confirm);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

gboolean
category_edit_dialog(GtkWidget *parent, gint cat_id)
{
    gchar *cat_label = NULL, *cat_desc = NULL;
    gint cat_enabled;
    GtkWidget *dialog;
    GtkWidget *table;
    GtkWidget *label;
    GtkWidget *txt_label;
    GtkWidget *txt_desc;
    GtkWidget *btn_delete = NULL;
    GtkWidget *txt_scroll;
    GtkWidget *chk_enabled;
    GtkTextBuffer *desc_txt;
    GtkTextIter begin, end;
    gboolean results = TRUE;
    DeletePOI dpoi = {NULL, NULL, 0};
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(cat_id > 0)
    {
        if(SQLITE_OK != sqlite3_bind_double(_stmt_select_cat, 1, cat_id) ||
           SQLITE_ROW != sqlite3_step(_stmt_select_cat))
        {
            vprintf("%s(): return FALSE\n", __PRETTY_FUNCTION__);
            sqlite3_reset(_stmt_select_cat);
            return FALSE;
        }

        cat_label = g_strdup(sqlite3_column_text(_stmt_select_cat, 0));
        cat_desc = g_strdup(sqlite3_column_text(_stmt_select_cat, 1));
        cat_enabled = sqlite3_column_int(_stmt_select_cat, 2);

        sqlite3_reset(_stmt_select_cat);

        dialog = gtk_dialog_new_with_buttons(_("Edit Category"),
            GTK_WINDOW(parent), GTK_DIALOG_MODAL,
            GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
            NULL);

        gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area),
                btn_delete = gtk_button_new_with_label(_("Delete...")));

        dpoi.dialog = dialog;
        dpoi.txt_label = g_strdup(cat_label);
        dpoi.id = cat_id;
        dpoi.deleted = FALSE;

        g_signal_connect(G_OBJECT(btn_delete), "clicked",
                          G_CALLBACK(category_delete), &dpoi);

        gtk_dialog_add_button(GTK_DIALOG(dialog),
                GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT);
    }
    else
    {
        cat_enabled = 1;
        cat_label = g_strdup("");
        cat_id = -1;
        cat_desc = g_strdup("");

        dialog = gtk_dialog_new_with_buttons(_("Add Category"),
            GTK_WINDOW(parent), GTK_DIALOG_MODAL,
            GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
            GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
            NULL);
    }

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
            table = gtk_table_new(6, 4, FALSE), TRUE, TRUE, 0);

    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new(_("Label")),
            0, 1, 0, 1, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
    gtk_table_attach(GTK_TABLE(table),
            txt_label = gtk_entry_new(),
            1, 2, 0, 1, GTK_EXPAND | GTK_FILL, 0, 2, 4);

    gtk_table_attach(GTK_TABLE(table),
            label = gtk_label_new(_("Description")),
            0, 1, 1, 2, GTK_FILL, 0, 2, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);

    txt_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(txt_scroll),
                                   GTK_SHADOW_IN);
    gtk_table_attach(GTK_TABLE(table),
            txt_scroll,
            1, 2, 1, 2, GTK_EXPAND | GTK_FILL, 0, 2, 4);

    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(txt_scroll),
                                 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    txt_desc = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(txt_desc), GTK_WRAP_WORD);

    gtk_container_add(GTK_CONTAINER(txt_scroll), txt_desc);
    gtk_widget_set_size_request(GTK_WIDGET(txt_scroll), 400, 60);

    desc_txt = gtk_text_view_get_buffer(GTK_TEXT_VIEW(txt_desc));

    gtk_table_attach(GTK_TABLE(table),
            chk_enabled = gtk_check_button_new_with_label(
                _("Enabled")),
            0, 2, 2, 3, GTK_EXPAND | GTK_FILL, 0, 2, 4);

    /* label */
    gtk_entry_set_text(GTK_ENTRY(txt_label), cat_label);

    /* desc */
    gtk_text_buffer_set_text(desc_txt, cat_desc, -1);

    /* enabled */
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chk_enabled),
            (cat_enabled == 1 ? TRUE : FALSE));

    g_free(cat_label);
    cat_label = NULL;
    g_free(cat_desc);
    cat_desc = NULL;

    gtk_widget_show_all(dialog);

    while(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(dialog)))
    {
        if(strlen(gtk_entry_get_text(GTK_ENTRY(txt_label))))
            cat_label = g_strdup(gtk_entry_get_text(GTK_ENTRY(txt_label)));
        else
        {
            popup_error(dialog, _("Please specify a name for the category."));
            continue;
        }

        gtk_text_buffer_get_iter_at_offset(desc_txt, &begin,0 );
        gtk_text_buffer_get_end_iter (desc_txt, &end);
        cat_desc = gtk_text_buffer_get_text(desc_txt, &begin, &end, TRUE);

        cat_enabled = (gtk_toggle_button_get_active(
                GTK_TOGGLE_BUTTON(chk_enabled)) ? 1 : 0);

        if(cat_id > 0)
        {
            /* edit category */
            if(SQLITE_OK != sqlite3_bind_text(_stmt_update_cat, 1, cat_label,
                        -1, g_free) ||
               SQLITE_OK != sqlite3_bind_text(_stmt_update_cat, 2, cat_desc,
                        -1, g_free) ||
               SQLITE_OK != sqlite3_bind_int(_stmt_update_cat, 3,cat_enabled)||
               SQLITE_OK != sqlite3_bind_int(_stmt_update_cat, 4, cat_id) ||
               SQLITE_DONE != sqlite3_step(_stmt_update_cat))
            {
                MACRO_BANNER_SHOW_INFO(parent,_("Error updating category"));
                results = FALSE;
            }
            sqlite3_reset(_stmt_update_cat);
        }
        else
        {
            /* add category */
            if(SQLITE_OK != sqlite3_bind_text(_stmt_insert_cat, 1, cat_label,
                        -1, g_free) ||
               SQLITE_OK != sqlite3_bind_text(_stmt_insert_cat, 2, cat_desc,
                        -1, g_free) ||
               SQLITE_OK != sqlite3_bind_int(_stmt_insert_cat, 3,cat_enabled)||
               SQLITE_DONE != sqlite3_step(_stmt_insert_cat))
            {
                MACRO_BANNER_SHOW_INFO(parent, _("Error adding category"));
                results = FALSE;
            }
            sqlite3_reset(_stmt_insert_cat);
        }
        break;
    }

    g_free(dpoi.txt_label);

    g_object_unref (desc_txt);

    if(results)
        map_force_redraw();

    gtk_widget_hide(dialog);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return results;
}

static void
category_toggled(GtkCellRendererToggle *cell, gchar *path, GtkListStore *data)
{
    GtkTreeIter iter;
    gboolean cat_enabled;
    gint cat_id;
    printf("%s()\n", __PRETTY_FUNCTION__);

    GtkTreeModel *model = GTK_TREE_MODEL(data);
    if( !gtk_tree_model_get_iter_from_string(model, &iter, path) )
        return;

    gtk_tree_model_get(model, &iter,
            CAT_ENABLED, &cat_enabled,
            CAT_ID, &cat_id,
            -1);

    cat_enabled ^= 1;

    if(SQLITE_OK != sqlite3_bind_int(_stmt_toggle_cat, 1, cat_enabled) ||
       SQLITE_OK != sqlite3_bind_int(_stmt_toggle_cat, 2, cat_id) ||
       SQLITE_DONE != sqlite3_step(_stmt_toggle_cat))
    {
        MACRO_BANNER_SHOW_INFO(_window, _("Error updating Category"));
    }
    else
    {
        gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                   CAT_ENABLED, cat_enabled, -1);
        map_force_redraw();
    }

    sqlite3_reset(_stmt_toggle_cat);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
}

static GtkListStore*
generate_store()
{
    GtkTreeIter iter;
    GtkListStore *store;
    printf("%s()\n", __PRETTY_FUNCTION__);

    store = gtk_list_store_new(CAT_NUM_COLUMNS,
                               G_TYPE_UINT,
                               G_TYPE_BOOLEAN,
                               G_TYPE_STRING,
                               G_TYPE_STRING,
                               G_TYPE_UINT);

    while(SQLITE_ROW == sqlite3_step(_stmt_selall_cat))
    {
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                CAT_ID, sqlite3_column_int(_stmt_selall_cat, 0),
                CAT_ENABLED, sqlite3_column_int(_stmt_selall_cat, 3),
                CAT_LABEL, sqlite3_column_text(_stmt_selall_cat, 1),
                CAT_DESC, sqlite3_column_text(_stmt_selall_cat, 2),
                CAT_POI_CNT, sqlite3_column_int(_stmt_selall_cat, 4),
                -1);
    }
    sqlite3_reset(_stmt_selall_cat);

    vprintf("%s(): return %p\n", __PRETTY_FUNCTION__, store);
    return store;
}

static gboolean
category_add(GtkWidget *widget, PoiCategoryEditInfo *pcedit)
{
    GtkListStore *store;
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(category_edit_dialog(pcedit->dialog, 0))
    {
        store = generate_store();
        gtk_tree_view_set_model(
                GTK_TREE_VIEW(pcedit->tree_view),
                GTK_TREE_MODEL(store));
        g_object_unref(G_OBJECT(store));
    }
    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
category_edit(GtkWidget *widget, PoiCategoryEditInfo *pcedit)
{
    GtkTreeIter iter;
    GtkTreeModel *store;
    GtkTreeSelection *selection;
    printf("%s()\n", __PRETTY_FUNCTION__);

    store = gtk_tree_view_get_model(GTK_TREE_VIEW(pcedit->tree_view));
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(pcedit->tree_view));
    if(gtk_tree_selection_get_selected(selection, &store, &iter))
    {
        GValue val;
        memset(&val, 0, sizeof(val));
        gtk_tree_model_get_value(store, &iter, 0, &val);
        if(category_edit_dialog(pcedit->dialog, g_value_get_uint(&val)))
        {
            GtkListStore *new_store = generate_store();
            gtk_tree_view_set_model(
                    GTK_TREE_VIEW(pcedit->tree_view),
                    GTK_TREE_MODEL(new_store));
            g_object_unref(G_OBJECT(new_store));
        }
    }
    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

gboolean
category_list_dialog(GtkWidget *parent)
{
    static GtkWidget *dialog = NULL;
    static GtkWidget *tree_view = NULL;
    static GtkWidget *sw = NULL;
    static GtkWidget *btn_edit = NULL;
    static GtkWidget *btn_add = NULL;
    static GtkTreeViewColumn *column = NULL;
    static GtkCellRenderer *renderer = NULL;
    static GtkListStore *store;
    static PoiCategoryEditInfo pcedit;
    printf("%s()\n", __PRETTY_FUNCTION__);

    store = generate_store();

    if(!store)
        return TRUE;

    dialog = gtk_dialog_new_with_buttons(_("POI Categories"),
            GTK_WINDOW(parent), GTK_DIALOG_MODAL,
            GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
            NULL);

    /* Enable the help button. */
#ifndef LEGACY
#else
    ossohelp_dialog_help_enable(
            GTK_DIALOG(dialog), HELP_ID_POICAT, _osso);
#endif

    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area),
            btn_edit = gtk_button_new_with_label(_("Edit...")));

    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area),
            btn_add = gtk_button_new_with_label(_("Add...")));

    sw = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW (sw),
                  GTK_POLICY_NEVER,
                  GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
            sw, TRUE, TRUE, 0);

    tree_view = gtk_tree_view_new();
    /* Maemo-related? */
    g_object_set(tree_view, "allow-checkbox-mode", FALSE, NULL);
    gtk_container_add (GTK_CONTAINER (sw), tree_view);

    gtk_tree_selection_set_mode(
            gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_view)),
            GTK_SELECTION_SINGLE);
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree_view), TRUE);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(
            _("ID"), renderer, "text", CAT_ID, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
    gtk_tree_view_column_set_max_width (column, 1);

    renderer = gtk_cell_renderer_toggle_new();
    g_signal_connect (renderer, "toggled",
            G_CALLBACK (category_toggled), store);
    column = gtk_tree_view_column_new_with_attributes(
            _("Enabled"), renderer, "active", CAT_ENABLED, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(
            _("Label"), renderer, "text", CAT_LABEL, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(
            _("Description"), renderer, "text", CAT_DESC, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(
            _("# POIs"), renderer, "text", CAT_POI_CNT, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);

    gtk_window_set_default_size(GTK_WINDOW(dialog), -1, 400);

    pcedit.dialog = dialog;
    pcedit.tree_view = tree_view;

    g_signal_connect(G_OBJECT(btn_edit), "clicked",
            G_CALLBACK(category_edit), &pcedit);

    g_signal_connect(G_OBJECT(btn_add), "clicked",
            G_CALLBACK(category_add), &pcedit);

    gtk_tree_view_set_model(GTK_TREE_VIEW(tree_view), GTK_TREE_MODEL(store));
    g_object_unref(G_OBJECT(store));

    gtk_widget_show_all(dialog);

    while(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(dialog)))
    {
        break;
    }

    gtk_widget_destroy(dialog);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
poi_delete(GtkWidget *widget, DeletePOI *dpoi)
{
    GtkWidget *confirm;
    gint i;
    gchar *buffer;
    printf("%s()\n", __PRETTY_FUNCTION__);

    buffer = g_strdup_printf("%s\n%s", _("Delete POI?"), dpoi->txt_label);
    confirm = hildon_note_new_confirmation(GTK_WINDOW(dpoi->dialog), buffer);
    g_free(buffer);
    i = gtk_dialog_run(GTK_DIALOG(confirm));
    gtk_widget_destroy(GTK_WIDGET(confirm));

    if(i == GTK_RESPONSE_OK)
    {
        if(SQLITE_OK != sqlite3_bind_int(_stmt_delete_poi, 1, dpoi->id) ||
           SQLITE_DONE != sqlite3_step(_stmt_delete_poi))
        {
            MACRO_BANNER_SHOW_INFO(dpoi->dialog, _("Error deleting POI"));
        }
        else
        {
            dpoi->deleted = TRUE;
            gtk_widget_hide(dpoi->dialog);
            map_force_redraw();
        }
        sqlite3_reset(_stmt_delete_poi);
    }

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
poi_populate_categories(GtkListStore *store, gint cat_id,
        GtkTreeIter *out_active)
{
    gboolean has_active = FALSE;
    printf("%s()\n", __PRETTY_FUNCTION__);

    gtk_list_store_clear(store);

    while(SQLITE_ROW == sqlite3_step(_stmt_selall_cat))
    {
        GtkTreeIter iter;
        gint cid = sqlite3_column_int(_stmt_selall_cat, 0);
        const gchar *clab = sqlite3_column_text(_stmt_selall_cat, 1);

        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter, 0, cid, 1, clab, -1);

        if(cid == cat_id || !has_active)
        {
            if(out_active)
                *out_active = iter;
            has_active = TRUE;
        }
    }
    sqlite3_reset(_stmt_selall_cat);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return has_active;
}

static gboolean
poi_edit_cat(GtkWidget *widget, PoiCategoryEditInfo *data)
{
    printf("%s()\n", __PRETTY_FUNCTION__);
    if(category_list_dialog(data->dialog))
    {
        GtkTreeIter active;
        if(poi_populate_categories(GTK_LIST_STORE(gtk_combo_box_get_model(
                        GTK_COMBO_BOX(data->cmb_category))),
                data->cat_id, &active))
        {
            gtk_combo_box_set_active_iter(
                    GTK_COMBO_BOX(data->cmb_category), &active);
        }
    }
    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static GtkWidget*
poi_create_cat_combo()
{
    GtkWidget *cmb_category;
    GtkTreeModel *model;
    printf("%s()\n", __PRETTY_FUNCTION__);

    model = GTK_TREE_MODEL(gtk_list_store_new(2,
                G_TYPE_INT,      /* Category ID */
                G_TYPE_STRING)); /* Category Label */
    cmb_category = gtk_combo_box_new_with_model(model);
    g_object_unref(model);

    /* Set up the view for the combo box. */
    {
        GtkCellRenderer *renderer;
        GtkTreeIter active;
        renderer = gtk_cell_renderer_text_new();
        gtk_cell_layout_pack_start(
                GTK_CELL_LAYOUT(cmb_category), renderer, TRUE);
        gtk_cell_layout_set_attributes(
                GTK_CELL_LAYOUT(cmb_category), renderer, "text", 1, NULL);

        poi_populate_categories(GTK_LIST_STORE(gtk_combo_box_get_model(
                        GTK_COMBO_BOX(cmb_category))), -1, &active);
    }
    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
    return cmb_category;
}

gboolean
poi_add_dialog(GtkWidget *parent, gint unitx, gint unity)
{
    static PoiInfo poi;
    static GtkWidget *dialog;
    static GtkWidget *table;
    static GtkWidget *label;
    static GtkWidget *txt_label;
    static GtkWidget *txt_lat;
    static GtkWidget *txt_lon;
    static GtkWidget *cmb_category;
    static GtkWidget *txt_desc;
    static GtkWidget *btn_catedit;
    static GtkWidget *hbox;
    static GtkWidget *txt_scroll;
    static GtkTextBuffer *desc_txt;
    static GtkTextIter begin, end;
    static DeletePOI dpoi = {NULL, NULL, 0};
    static PoiCategoryEditInfo pcedit;
    static int last_deg_format = 0;
    
    printf("%s()\n", __PRETTY_FUNCTION__);

    unit2latlon(unitx, unity, poi.lat, poi.lon);

    
    gint fallback_deg_format = _degformat;
    
    if(!coord_system_check_lat_lon (poi.lat, poi.lon, &fallback_deg_format))
    {
    	last_deg_format = _degformat;
    	_degformat = fallback_deg_format;
    	
    	if(dialog != NULL) gtk_widget_destroy(dialog);
    	dialog = NULL;
    }
    else if(_degformat != last_deg_format)
    {
    	last_deg_format = _degformat;
    	
		if(dialog != NULL) gtk_widget_destroy(dialog);
    	dialog = NULL;
    }
    
    


    if(dialog == NULL)
    {
        dialog = gtk_dialog_new_with_buttons(_("Add POI"),
            GTK_WINDOW(parent), GTK_DIALOG_MODAL,
            GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
            GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
            NULL);

        /* Set the lat/lon strings. */
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
                table = gtk_table_new(6, 4, FALSE), TRUE, TRUE, 0);

        gtk_table_attach(GTK_TABLE(table),
                label = gtk_label_new(DEG_FORMAT_ENUM_TEXT[_degformat].short_field_1),
                0, 1, 0, 1, GTK_FILL, 0, 2, 0);
        gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
        gtk_table_attach(GTK_TABLE(table),
                txt_lat = gtk_entry_new(),
                1, (DEG_FORMAT_ENUM_TEXT[_degformat].field_2_in_use ? 2 : 4), 
                0, 1, GTK_FILL, 0, 2, 0);

        if(DEG_FORMAT_ENUM_TEXT[_degformat].field_2_in_use )
        {
	        gtk_table_attach(GTK_TABLE(table),
	                label = gtk_label_new(DEG_FORMAT_ENUM_TEXT[_degformat].short_field_2),
	                2, 3, 0, 1, GTK_FILL, 0, 2, 0);
	        gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
	        gtk_table_attach(GTK_TABLE(table),
	                txt_lon = gtk_entry_new(),
	                3, 4, 0, 1, GTK_FILL, 0, 2, 0);
        }
        
        gtk_table_attach(GTK_TABLE(table),
                label = gtk_label_new(_("Label")),
                0, 1, 1, 2, GTK_FILL, 0, 2, 0);
        gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
        gtk_table_attach(GTK_TABLE(table),
                txt_label = gtk_entry_new(),
                1, 4, 1, 2, GTK_EXPAND | GTK_FILL, 0, 2, 0);

        gtk_table_attach(GTK_TABLE(table),
                label = gtk_label_new(_("Category")),
                0, 1, 3, 4, GTK_FILL, 0, 2, 0);
        gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
        gtk_table_attach(GTK_TABLE(table),
                hbox = gtk_hbox_new(FALSE, 4),
                1, 4, 3, 4, GTK_EXPAND | GTK_FILL, 0, 2, 0);
        gtk_box_pack_start(GTK_BOX(hbox),
                cmb_category = poi_create_cat_combo(),
                FALSE, FALSE, 0);

        gtk_box_pack_start(GTK_BOX(hbox),
                btn_catedit = gtk_button_new_with_label(
                    _("Edit Categories...")),
                FALSE, FALSE, 0);

        gtk_table_attach(GTK_TABLE(table),
                label = gtk_label_new(_("Description")),
                0, 1, 5, 6, GTK_FILL, GTK_FILL, 2, 0);
        gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.0f);

        txt_scroll = gtk_scrolled_window_new(NULL, NULL);
        gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(txt_scroll),
                GTK_SHADOW_IN);
        gtk_table_attach(GTK_TABLE(table),
                txt_scroll,
                1, 4, 5, 6, GTK_EXPAND | GTK_FILL, 0, 2, 0);

        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(txt_scroll),
                GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

        txt_desc = gtk_text_view_new ();
        gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(txt_desc), GTK_WRAP_WORD);

        gtk_container_add(GTK_CONTAINER(txt_scroll), txt_desc);
        gtk_widget_set_size_request(GTK_WIDGET(txt_scroll), 550, 120);

        desc_txt = gtk_text_view_get_buffer(GTK_TEXT_VIEW (txt_desc));

        g_signal_connect(G_OBJECT(btn_catedit), "clicked",
                G_CALLBACK(poi_edit_cat), &pcedit);
    }

    poi.poi_id = -1;
    poi.cat_id = -1;
    poi.clabel = NULL;
    poi.desc = g_strdup("");

    /* Lat/Lon */
    {
        gchar tmp1[LL_FMT_LEN], tmp2[LL_FMT_LEN];

        format_lat_lon(poi.lat, poi.lon, tmp1, tmp2);
        //lat_format(poi.lat, tmp1);
        //lon_format(poi.lon, tmp2);

        gtk_entry_set_text(GTK_ENTRY(txt_lat), tmp1);
        
        if(txt_lon != NULL)
        	gtk_entry_set_text(GTK_ENTRY(txt_lon), tmp2);
    }

    /* Label */
    if(SQLITE_ROW == sqlite3_step(_stmt_nextlabel_poi))
        poi.label = g_strdup_printf("Point%06d",
                sqlite3_column_int(_stmt_nextlabel_poi, 0));
    else
        poi.label = g_strdup("");
    sqlite3_reset(_stmt_nextlabel_poi);
    gtk_entry_set_text(GTK_ENTRY(txt_label), poi.label);

    /* POI Desc. */
    gtk_text_buffer_set_text(desc_txt, "", -1);

    /* Category. */
    {
        GtkTreeIter iter;
        gint cat_id = -1;
        gboolean had_cat_id = FALSE;

        if(gtk_combo_box_get_active_iter(
                GTK_COMBO_BOX(cmb_category), &iter))
        {
            gtk_tree_model_get(
                    gtk_combo_box_get_model(GTK_COMBO_BOX(cmb_category)),&iter,
                    0, &cat_id,
                    -1);
            had_cat_id = TRUE;
        }
        gtk_list_store_clear(GTK_LIST_STORE(gtk_combo_box_get_model(
                        GTK_COMBO_BOX(cmb_category))));
        if(poi_populate_categories(GTK_LIST_STORE(gtk_combo_box_get_model(
                    GTK_COMBO_BOX(cmb_category))), cat_id, &iter)
                && had_cat_id)
        {
            gtk_combo_box_set_active_iter(GTK_COMBO_BOX(cmb_category), &iter);
        }
    }

    pcedit.dialog = dialog;
    pcedit.cmb_category = cmb_category;
    pcedit.cat_id = poi.cat_id;

    gtk_widget_show_all(dialog);

    while(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(dialog)))
    {
        GtkTreeIter iter;
        const gchar *lat, *lon = NULL;

        lat = gtk_entry_get_text(GTK_ENTRY(txt_lat));
        
        if(txt_lon != NULL)
        	lon = gtk_entry_get_text(GTK_ENTRY(txt_lon));
        
        if(!parse_coords(lat, lon, &poi.lat, &poi.lon))
        {
        	popup_error(dialog, _("Invalid Coordinate specified"));
        	continue;
        }
        
        if(strlen(gtk_entry_get_text(GTK_ENTRY(txt_label))))
        {
            if(poi.label)
                g_free(poi.label);
            poi.label = g_strdup(gtk_entry_get_text(GTK_ENTRY(txt_label)));
        }
        else
        {
            popup_error(dialog, _("Please specify a name."));
            continue;
        }

        if(!gtk_combo_box_get_active_iter(
                GTK_COMBO_BOX(cmb_category), &iter))
        {
            popup_error(dialog, _("Please specify a category."));
            continue;
        }

        gtk_text_buffer_get_iter_at_offset(desc_txt, &begin,0 );
        gtk_text_buffer_get_end_iter (desc_txt, &end);
        if(poi.desc)
            g_free(poi.desc);
        poi.desc = gtk_text_buffer_get_text(desc_txt, &begin, &end, TRUE);

        if(poi.clabel)
            g_free(poi.clabel);
        gtk_tree_model_get(
                gtk_combo_box_get_model(GTK_COMBO_BOX(cmb_category)), &iter,
                0, &poi.cat_id,
                1, &poi.clabel,
                -1);

        /* add poi */
        if(SQLITE_OK != sqlite3_bind_double(_stmt_insert_poi, 1, poi.lat)
        || SQLITE_OK != sqlite3_bind_double(_stmt_insert_poi, 2, poi.lon)
        || SQLITE_OK != sqlite3_bind_text(_stmt_insert_poi, 3, poi.label,
               -1, SQLITE_STATIC)
        || SQLITE_OK != sqlite3_bind_text(_stmt_insert_poi, 4, poi.desc,
               -1, SQLITE_STATIC)
        || SQLITE_OK != sqlite3_bind_int(_stmt_insert_poi, 5, poi.cat_id)
        || SQLITE_DONE != sqlite3_step(_stmt_insert_poi))
        {
            MACRO_BANNER_SHOW_INFO(parent, _("Error adding POI"));
        }

        sqlite3_reset(_stmt_insert_poi);

        /* We're done. */
        break;
    }

    g_free(poi.label);
    g_free(poi.desc);
    g_free(dpoi.txt_label);

    map_force_redraw();

    gtk_widget_hide(dialog);

    _degformat = last_deg_format;
    	
    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return !dpoi.deleted;
}

gboolean
poi_view_dialog(GtkWidget *parent, PoiInfo *poi)
{
    GtkTreeIter iter;
    static GtkWidget *dialog;
    static GtkWidget *table;
    static GtkWidget *label;
    static GtkWidget *txt_label;
    static GtkWidget *txt_lat;
    static GtkWidget *txt_lon;
    static GtkWidget *cmb_category;
    static GtkWidget *txt_desc;
    static GtkWidget *btn_delete = NULL;
    static GtkWidget *btn_catedit;
    static GtkWidget *hbox;
    static GtkWidget *txt_scroll;
    static GtkTextBuffer *desc_txt;
    static GtkTextIter begin, end;
    static DeletePOI dpoi = {NULL, NULL, 0};
    static PoiCategoryEditInfo pcedit;
    static int last_deg_format = 0;
    
    printf("%s()\n", __PRETTY_FUNCTION__);
    
    gint fallback_deg_format = _degformat;
    
    if(!coord_system_check_lat_lon (poi->lat, poi->lon, &fallback_deg_format))
    {
    	last_deg_format = _degformat;
    	_degformat = fallback_deg_format;
    	
    	if(dialog != NULL) gtk_widget_destroy(dialog);
    	dialog = NULL;
    }
    else if(_degformat != last_deg_format)
    {
    	last_deg_format = _degformat;
    	
		if(dialog != NULL) gtk_widget_destroy(dialog);
    	dialog = NULL;
    }

    if(dialog == NULL)
    {
        dialog = gtk_dialog_new_with_buttons(_("Edit POI"),
            GTK_WINDOW(parent), GTK_DIALOG_MODAL,
            GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
            NULL);

        gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area),
                btn_delete = gtk_button_new_with_label(_("Delete...")));

        gtk_dialog_add_button(GTK_DIALOG(dialog),
                GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT);

        /* Set the lat/lon strings. */
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
                table = gtk_table_new(6, 4, FALSE), TRUE, TRUE, 0);

        gtk_table_attach(GTK_TABLE(table),
                label = gtk_label_new(DEG_FORMAT_ENUM_TEXT[_degformat].short_field_1),
                0, 1, 0, 1, GTK_FILL, 0, 2, 0);
        gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
        gtk_table_attach(GTK_TABLE(table),
                txt_lat = gtk_entry_new(),
                1, 2, 0, 1, GTK_FILL, 0, 2, 0);

        if(DEG_FORMAT_ENUM_TEXT[_degformat].field_2_in_use)
        {
	        gtk_table_attach(GTK_TABLE(table),
	                label = gtk_label_new(DEG_FORMAT_ENUM_TEXT[_degformat].short_field_2),
	                2, 3, 0, 1, GTK_FILL, 0, 2, 0);
	        gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
	        gtk_table_attach(GTK_TABLE(table),
	                txt_lon = gtk_entry_new(),
	                3, 4, 0, 1, GTK_FILL, 0, 2, 0);
        }
        
        gtk_table_attach(GTK_TABLE(table),
                label = gtk_label_new(_("Label")),
                0, 1, 1, 2, GTK_FILL, 0, 2, 0);
        gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
        gtk_table_attach(GTK_TABLE(table),
                txt_label = gtk_entry_new(),
                1, 4, 1, 2, GTK_EXPAND | GTK_FILL, 0, 2, 0);

        gtk_table_attach(GTK_TABLE(table),
                label = gtk_label_new(_("Category")),
                0, 1, 3, 4, GTK_FILL, 0, 2, 0);
        gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
        gtk_table_attach(GTK_TABLE(table),
                hbox = gtk_hbox_new(FALSE, 4),
                1, 4, 3, 4, GTK_EXPAND | GTK_FILL, 0, 2, 0);
        gtk_box_pack_start(GTK_BOX(hbox),
                cmb_category = poi_create_cat_combo(),
                FALSE, FALSE, 0);

        gtk_box_pack_start(GTK_BOX(hbox),
                btn_catedit = gtk_button_new_with_label(
                    _("Edit Categories...")),
                FALSE, FALSE, 0);

        gtk_table_attach(GTK_TABLE(table),
                label = gtk_label_new(_("Description")),
                0, 1, 5, 6, GTK_FILL, GTK_FILL, 2, 0);
        gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.0f);

        txt_scroll = gtk_scrolled_window_new(NULL, NULL);
        gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(txt_scroll),
                GTK_SHADOW_IN);
        gtk_table_attach(GTK_TABLE(table),
                txt_scroll,
                1, 4, 5, 6, GTK_EXPAND | GTK_FILL, 0, 2, 0);

        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(txt_scroll),
                GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

        txt_desc = gtk_text_view_new ();
        gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(txt_desc), GTK_WRAP_WORD);

        gtk_container_add(GTK_CONTAINER(txt_scroll), txt_desc);
        gtk_widget_set_size_request(GTK_WIDGET(txt_scroll), 550, 120);

        desc_txt = gtk_text_view_get_buffer (GTK_TEXT_VIEW (txt_desc));

        g_signal_connect(G_OBJECT(btn_delete), "clicked",
                          G_CALLBACK(poi_delete), &dpoi);

        g_signal_connect(G_OBJECT(btn_catedit), "clicked",
                G_CALLBACK(poi_edit_cat), &pcedit);
    }

    dpoi.dialog = dialog;
    dpoi.txt_label = g_strdup(poi->label);
    dpoi.id = poi->poi_id;
    dpoi.deleted = FALSE;

    /* Lat/Lon */
    {
        gchar tmp1[LL_FMT_LEN], tmp2[LL_FMT_LEN];

        format_lat_lon(poi->lat, poi->lon, tmp1, tmp2);
        //lat_format(poi->lat, tmp1);
        //lon_format(poi->lon, tmp2);

        gtk_entry_set_text(GTK_ENTRY(txt_lat), tmp1);
        
        if(DEG_FORMAT_ENUM_TEXT[_degformat].field_2_in_use)
        	gtk_entry_set_text(GTK_ENTRY(txt_lon), tmp2);
        else
        	gtk_entry_set_text(GTK_ENTRY(txt_lon), g_strdup(""));
    }

    /* label */
    gtk_entry_set_text(GTK_ENTRY(txt_label), poi->label);

    /* poi_desc */
    gtk_text_buffer_set_text(desc_txt, poi->desc, -1);

    /* Category. */
    gtk_list_store_clear(GTK_LIST_STORE(gtk_combo_box_get_model(
                    GTK_COMBO_BOX(cmb_category))));
    if(poi_populate_categories(GTK_LIST_STORE(gtk_combo_box_get_model(
                GTK_COMBO_BOX(cmb_category))), poi->cat_id, &iter))
        gtk_combo_box_set_active_iter(GTK_COMBO_BOX(cmb_category), &iter);

    /* Connect Signals */
    pcedit.dialog = dialog;
    pcedit.cmb_category = cmb_category;
    pcedit.cat_id = poi->cat_id;

    gtk_widget_show_all(dialog);

    while(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(dialog)))
    {
        const gchar *text_lat, *text_lon;

        text_lat = gtk_entry_get_text(GTK_ENTRY(txt_lat));
        text_lon = gtk_entry_get_text(GTK_ENTRY(txt_lon));
        
        if(!parse_coords(text_lat, text_lon, &poi->lat, &poi->lon))
        {
        	popup_error(dialog, _("Invalid coordinate specified"));
        	continue;
        }

        
        if(strlen(gtk_entry_get_text(GTK_ENTRY(txt_label))))
        {
            if(poi->label)
                g_free(poi->label);
            poi->label = g_strdup(gtk_entry_get_text(GTK_ENTRY(txt_label)));
        }
        else
        {
            popup_error(dialog, _("Please specify a name."));
            continue;
        }

        if(!gtk_combo_box_get_active_iter(
                GTK_COMBO_BOX(cmb_category), &iter))
        {
            popup_error(dialog, _("Please specify a category."));
            continue;
        }

        gtk_text_buffer_get_iter_at_offset(desc_txt, &begin,0 );
        gtk_text_buffer_get_end_iter (desc_txt, &end);
        if(poi->desc)
            g_free(poi->desc);
        poi->desc = gtk_text_buffer_get_text(desc_txt, &begin, &end, TRUE);

        if(poi->clabel)
            g_free(poi->clabel);
        gtk_tree_model_get(
                gtk_combo_box_get_model(GTK_COMBO_BOX(cmb_category)), &iter,
                0, &poi->cat_id,
                1, &poi->clabel,
                -1);

        /* edit poi */
        if(SQLITE_OK != sqlite3_bind_double(
                    _stmt_update_poi, 1, poi->lat) ||
           SQLITE_OK != sqlite3_bind_double(
               _stmt_update_poi, 2, poi->lon) ||
           SQLITE_OK != sqlite3_bind_text(_stmt_update_poi, 3, poi->label,
                    -1, SQLITE_STATIC) ||
           SQLITE_OK != sqlite3_bind_text(_stmt_update_poi, 4, poi->desc,
               -1, SQLITE_STATIC) ||
           SQLITE_OK != sqlite3_bind_int(
               _stmt_update_poi, 5, poi->cat_id) ||
           SQLITE_OK != sqlite3_bind_int(
               _stmt_update_poi, 6, poi->poi_id) ||
           SQLITE_DONE != sqlite3_step(_stmt_update_poi))
        {
            MACRO_BANNER_SHOW_INFO(parent, _("Error updating POI"));
        }

        sqlite3_reset(_stmt_update_poi);

        /* We're done. */
        break;
    }

    g_free(dpoi.txt_label);

    map_force_redraw();

    gtk_widget_hide(dialog); /* Destroying causes a crash.... ??? */

    _degformat = last_deg_format;
    
    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return !dpoi.deleted;
}

static gint
poi_list_insert(GtkWidget *parent, GList *poi_list, GtkComboBox *cmb_category)
{
    gint default_cat_id;
    gchar *default_cat_label;
    gint num_inserts = 0;
    GList *curr;
    GtkTreeIter iter;
    printf("%s()\n", __PRETTY_FUNCTION__);

    /* Get defaults from the given GtkComboBox */
    if(!gtk_combo_box_get_active_iter(
            GTK_COMBO_BOX(cmb_category), &iter))
    {
        vprintf("%s(): return 0\n", __PRETTY_FUNCTION__);
        return 0;
    }
    gtk_tree_model_get(
            gtk_combo_box_get_model(GTK_COMBO_BOX(cmb_category)),
            &iter,
            0, &default_cat_id,
            1, &default_cat_label,
            -1);

    /* Iterate through the data model and import as desired. */
    for(curr = poi_list; curr; )
    {
        PoiInfo *poi = curr->data;
        if(
        (    SQLITE_OK != sqlite3_bind_double(_stmt_insert_poi, 1, poi->lat)
          || SQLITE_OK != sqlite3_bind_double(_stmt_insert_poi, 2, poi->lon)
          || SQLITE_OK != sqlite3_bind_text(_stmt_insert_poi, 3, poi->label,
             -1, SQLITE_STATIC)
          || SQLITE_OK != sqlite3_bind_text(_stmt_insert_poi, 4, poi->desc,
             -1, SQLITE_STATIC)
          || SQLITE_OK != sqlite3_bind_int(_stmt_insert_poi, 5,
              poi->cat_id = default_cat_id)
          || SQLITE_DONE != sqlite3_step(_stmt_insert_poi)
        ))
        {
            /* Failure. */
            GList *tmp = curr->next;
            if(poi->label)
                g_free(poi->label);
            if(poi->desc)
                g_free(poi->desc);
            g_slice_free(PoiInfo, poi);
            poi_list = g_list_delete_link(poi_list, curr);
            curr = tmp;
        }
        else
        {
            /* Success. */
            ++num_inserts;
            if(default_cat_label)
                poi->clabel = g_strdup(default_cat_label);
            poi->poi_id = sqlite3_last_insert_rowid(_poi_db);
            curr = curr->next;
        }
        sqlite3_reset(_stmt_insert_poi);
    }

    if(num_inserts)
    {
        gchar buffer[BUFFER_SIZE];
        map_force_redraw();
        snprintf(buffer, sizeof(buffer), "%d %s", num_inserts,
           _("POIs were added to the POI database.  The following screen will "
               "allow you to modify or delete any of the new POIs."));
        popup_error(parent, buffer);
    }
    else
    {
        popup_error(parent, _("No POIs were found."));
    }

    if(default_cat_label)
        g_free(default_cat_label);

    vprintf("%s(): return %d\n", __PRETTY_FUNCTION__, num_inserts);
    return num_inserts;
}

static void
poi_list_free(GList *poi_list)
{
    GList *curr;
    printf("%s()\n", __PRETTY_FUNCTION__);

    for(curr = poi_list; curr; curr = curr->next)
    {
        PoiInfo *poi_info = curr->data;
        if(poi_info)
        {
            if(poi_info->label)
                g_free(poi_info->label);
            if(poi_info->desc)
                g_free(poi_info->desc);
            if(poi_info->clabel)
                g_free(poi_info->clabel);
            g_slice_free(PoiInfo, poi_info);
        }
    }

    g_list_free(poi_list);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

static void
poi_list_bearing_cell_data_func(
        GtkTreeViewColumn *tree_column,
        GtkCellRenderer *cell,
        GtkTreeModel *tree_model,
        GtkTreeIter *iter)
{
    gchar buffer[80];
    gfloat f;
    vprintf("%s()\n", __PRETTY_FUNCTION__);

    gtk_tree_model_get(tree_model, iter, POI_BEARING, &f, -1);
    snprintf(buffer, sizeof(buffer), "%.1f", f);
    g_object_set(cell, "text", buffer, NULL);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

static void
poi_list_distance_cell_data_func(
        GtkTreeViewColumn *tree_column,
        GtkCellRenderer *cell,
        GtkTreeModel *tree_model,
        GtkTreeIter *iter)
{
    gchar buffer[80];
    gfloat f;
    vprintf("%s()\n", __PRETTY_FUNCTION__);

    gtk_tree_model_get(tree_model, iter, POI_DISTANCE, &f, -1);
    snprintf(buffer, sizeof(buffer), "%.2f", f);
    g_object_set(cell, "text", buffer, NULL);

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

static gboolean
poi_list_row_selected(GtkCellRendererToggle *renderer,
        gchar *path_string, GtkTreeModel *tree_model)
{
    GtkTreeIter iter;
    vprintf("%s()\n", __PRETTY_FUNCTION__);

    if(gtk_tree_model_get_iter_from_string(tree_model, &iter, path_string))
    {
        gboolean old_value;
        gtk_tree_model_get(tree_model, &iter, POI_SELECTED, &old_value, -1);
        gtk_list_store_set(GTK_LIST_STORE(tree_model), &iter,
                POI_SELECTED, !old_value,
                -1);
    }

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
poi_list_set_category(GtkWidget *widget, PoiListInfo *pli)
{
    static GtkWidget *dialog = NULL;
    static GtkWidget *cmb_category = NULL;
    static GtkWidget *btn_catedit = NULL;
    static PoiCategoryEditInfo pcedit;
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(dialog == NULL)
    {
        GtkWidget *hbox;
        GtkWidget *label;

        dialog = gtk_dialog_new_with_buttons(_("Set Category..."),
                GTK_WINDOW(pli->dialog2), GTK_DIALOG_MODAL,
                GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                NULL);

        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
                hbox = gtk_hbox_new(FALSE, 4), FALSE, FALSE, 4);

        gtk_box_pack_start(GTK_BOX(hbox),
                label = gtk_label_new(_("Category")),
                FALSE, FALSE, 0);

        gtk_box_pack_start(GTK_BOX(hbox),
                cmb_category = poi_create_cat_combo(),
                FALSE, FALSE, 4);

        gtk_box_pack_start(GTK_BOX(hbox),
                btn_catedit = gtk_button_new_with_label(
                    _("Edit Categories...")),
                FALSE, FALSE, 0);

        /* Connect Signals */
        pcedit.dialog = dialog;
        pcedit.cmb_category = cmb_category;
        pcedit.cat_id = -1;
        g_signal_connect(G_OBJECT(btn_catedit), "clicked",
                G_CALLBACK(poi_edit_cat), &pcedit);
    }

    gtk_widget_show_all(dialog);

    while(GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(dialog)))
    {
        GtkTreeIter iter;
        GtkListStore *store;
        gint cat_id;
        const gchar *cat_label;

        /* Get the text of the chosen category. */
        if(!gtk_combo_box_get_active_iter(
                GTK_COMBO_BOX(cmb_category), &iter))
        {
            popup_error(dialog, _("Please specify a category."));
            continue;
        }

        gtk_tree_model_get(
                gtk_combo_box_get_model(GTK_COMBO_BOX(cmb_category)),
                &iter,
                0, &cat_id,
                1, &cat_label,
                -1);

        /* Iterate through the data store and categorize as desired. */
        store = GTK_LIST_STORE(gtk_tree_view_get_model(
                    GTK_TREE_VIEW(pli->tree_view)));
        if(gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter)) do
        {
            PoiInfo poi;
            gboolean selected;

            memset(&poi, 0, sizeof(poi));

            gtk_tree_model_get(GTK_TREE_MODEL(store), &iter,
                    POI_SELECTED, &selected,
                    POI_POIID, &(poi.poi_id),
                    POI_LAT, &(poi.lat),
                    POI_LON, &(poi.lon),
                    POI_LABEL, &(poi.label),
                    POI_DESC, &(poi.desc),
                    -1);

            if(selected)
            {
                gtk_list_store_set(store, &iter,
                    POI_CATID, cat_id,
                    POI_CLABEL, cat_label,
                    -1);
                /* edit poi */
                if(SQLITE_OK != sqlite3_bind_double(
                            _stmt_update_poi, 1, poi.lat) ||
                   SQLITE_OK != sqlite3_bind_double(
                       _stmt_update_poi, 2, poi.lon) ||
                   SQLITE_OK != sqlite3_bind_text(_stmt_update_poi,
                       3, poi.label, -1, SQLITE_STATIC) ||
                   SQLITE_OK != sqlite3_bind_text(_stmt_update_poi,
                       4, poi.desc, -1, SQLITE_STATIC) ||
                   SQLITE_OK != sqlite3_bind_int(
                       _stmt_update_poi, 5, cat_id) ||
                   SQLITE_OK != sqlite3_bind_int(
                       _stmt_update_poi, 6, poi.poi_id) ||
                   SQLITE_DONE != sqlite3_step(_stmt_update_poi))
                {
                    MACRO_BANNER_SHOW_INFO(pli->dialog2,
                            _("Error updating POI"));
                }
                sqlite3_reset(_stmt_update_poi);
            }
        } while(gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter));

        break;
    }

    map_force_redraw();
    gtk_widget_hide(dialog);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
poi_list_select_all(GtkTreeViewColumn *column, PoiListInfo *pli)
{
    GtkTreeIter iter;
    GtkListStore *store;
    printf("%s()\n", __PRETTY_FUNCTION__);

    /* Iterate through the data store and select as desired. */
    store = GTK_LIST_STORE(gtk_tree_view_get_model(
                GTK_TREE_VIEW(pli->tree_view)));
    if(gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter)) do
    {
        gtk_list_store_set(store, &iter,
            POI_SELECTED, pli->select_all,
            -1);
    } while(gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter));

    pli->select_all = !pli->select_all;

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
poi_list_view(GtkWidget *widget, PoiListInfo *pli)
{
    GtkTreeIter iter;
    GtkTreeSelection *selection;
    GtkListStore *store;
    printf("%s()\n", __PRETTY_FUNCTION__);

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(pli->tree_view));
    store = GTK_LIST_STORE(gtk_tree_view_get_model(
                GTK_TREE_VIEW(pli->tree_view)));

    /* Iterate through the data store and import as desired. */
    if(gtk_tree_selection_get_selected(selection, NULL, &iter))
    {
        PoiInfo poi;
        memset(&poi, 0, sizeof(poi));

        gtk_tree_model_get(GTK_TREE_MODEL(store), &iter,
                POI_POIID, &(poi.poi_id),
                POI_CATID, &(poi.cat_id),
                POI_LAT, &(poi.lat),
                POI_LON, &(poi.lon),
                POI_LABEL, &(poi.label),
                POI_DESC, &(poi.desc),
                POI_CLABEL, &(poi.clabel),
                -1);

        if(poi_view_dialog(pli->dialog, &poi))
        {
            gtk_list_store_set(store, &iter,
                    POI_POIID, poi.poi_id,
                    POI_CATID, poi.cat_id,
                    POI_LAT, poi.lat,
                    POI_LON, poi.lon,
                    POI_LABEL, poi.label,
                    POI_DESC, poi.desc,
                    POI_CLABEL, poi.clabel,
                    -1);
        }
        else
        {
            /* POI was deleted. */
            gtk_list_store_remove(store, &iter);
        }
    }

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static void
poi_list_row_activated(GtkTreeView *tree_view, GtkTreePath *path,
        GtkTreeViewColumn *column, PoiListInfo *pli)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(column != pli->select_column)
        poi_list_view(GTK_WIDGET(tree_view), pli);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
}

static gboolean
poi_list_goto(GtkWidget *widget, PoiListInfo *pli)
{
    GtkTreeIter iter;
    GtkTreeSelection *selection;
    GtkListStore *store;
    printf("%s()\n", __PRETTY_FUNCTION__);

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(pli->tree_view));
    store = GTK_LIST_STORE(gtk_tree_view_get_model(
                GTK_TREE_VIEW(pli->tree_view)));

    /* Iterate through the data store and import as desired. */
    if(gtk_tree_selection_get_selected(selection, NULL, &iter))
    {
        gdouble lat, lon;
        Point unit;

        gtk_tree_model_get(GTK_TREE_MODEL(store), &iter,
                POI_LAT, &lat,
                POI_LON, &lon,
                -1);

        latlon2unit(lat, lon, unit.unitx, unit.unity);

        if(_center_mode > 0)
            gtk_check_menu_item_set_active(
                    GTK_CHECK_MENU_ITEM(_menu_view_ac_none_item), TRUE);

        map_center_unit(unit);
    }

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
poi_list_delete(GtkWidget *widget, PoiListInfo *pli)
{
    GtkWidget *confirm;
    printf("%s()\n", __PRETTY_FUNCTION__);

    confirm = hildon_note_new_confirmation(
            GTK_WINDOW(pli->dialog2), _("Delete selected POI?"));

    if(GTK_RESPONSE_OK == gtk_dialog_run(GTK_DIALOG(confirm)))
    {
        GtkTreeIter iter;
        GtkListStore *store;
        gboolean already_next;
        gboolean must_iterate;;

        /* Iterate through the data store and import as desired. */
        store = GTK_LIST_STORE(gtk_tree_view_get_model(
                    GTK_TREE_VIEW(pli->tree_view)));
        if(gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter)) do
        {
            gboolean selected;
            must_iterate = TRUE;
            already_next = FALSE;
            gint poi_id;
            gtk_tree_model_get(GTK_TREE_MODEL(store), &iter,
                POI_SELECTED, &selected,
                POI_POIID, &poi_id,
                -1);
            if(selected)
            {
                /* Delete POI. */
                if(SQLITE_OK != sqlite3_bind_int(_stmt_delete_poi, 1, poi_id)
                || SQLITE_DONE != sqlite3_step(_stmt_delete_poi))
                {
                    MACRO_BANNER_SHOW_INFO(pli->dialog2,
                            _("Error deleting POI"));
                }
                else
                {
                    already_next = gtk_list_store_remove(store, &iter);
                    must_iterate = FALSE;
                }
                sqlite3_reset(_stmt_delete_poi);
            }
        } while(already_next || (must_iterate
                && gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter)));
    }

    map_force_redraw();

    gtk_widget_destroy(confirm);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
poi_list_export_gpx(GtkWidget *widget, PoiListInfo *pli)
{
    GnomeVFSHandle *handle;
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(display_open_file(GTK_WINDOW(pli->dialog2), NULL, &handle, NULL,
                NULL, NULL, GTK_FILE_CHOOSER_ACTION_SAVE))
    {
        gint num_exported = gpx_poi_write(
               gtk_tree_view_get_model(GTK_TREE_VIEW(pli->tree_view)), handle);
        if(num_exported >= 0)
        {
            gchar buffer[80];
            snprintf(buffer, sizeof(buffer), "%d %s\n", num_exported,
                    _("POIs Exported"));
            MACRO_BANNER_SHOW_INFO(pli->dialog2, buffer);
        }
        else
            popup_error(pli->dialog2, _("Error writing GPX file."));
        gnome_vfs_close(handle);
    }

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
poi_list_manage_checks(GtkWidget *widget, PoiListInfo *pli)
{
    GtkWidget *btn_category;
    GtkWidget *btn_delete;
    GtkWidget *btn_export_gpx;

    printf("%s()\n", __PRETTY_FUNCTION__);

    pli->dialog2 = gtk_dialog_new_with_buttons(_("Checked POI Actions..."),
            GTK_WINDOW(pli->dialog), GTK_DIALOG_MODAL,
            NULL);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(pli->dialog2)->vbox),
            gtk_label_new(_("Select an operation to perform\n"
                            "on the POIs that you checked\n"
                            "in the POI list.")),
                FALSE, FALSE, 4);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(pli->dialog2)->vbox),
            btn_category = gtk_button_new_with_label(_("Set Category...")),
                FALSE, FALSE, 4);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(pli->dialog2)->vbox),
            btn_delete = gtk_button_new_with_label(_("Delete...")),
                FALSE, FALSE, 4);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(pli->dialog2)->vbox),
            btn_export_gpx = gtk_button_new_with_label(
                _("Export to GPX...")),
                FALSE, FALSE, 4);

    gtk_dialog_add_button(GTK_DIALOG(pli->dialog2),
            GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT);

    g_signal_connect(G_OBJECT(btn_category), "clicked",
            G_CALLBACK(poi_list_set_category), pli);

    g_signal_connect(G_OBJECT(btn_delete), "clicked",
            G_CALLBACK(poi_list_delete), pli);

    g_signal_connect(G_OBJECT(btn_export_gpx), "clicked",
            G_CALLBACK(poi_list_export_gpx), pli);

    gtk_widget_show_all(pli->dialog2);

    gtk_dialog_run(GTK_DIALOG(pli->dialog2));

    gtk_widget_destroy(pli->dialog2);
    pli->dialog2 = NULL;

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

static gboolean
poi_list_dialog(GtkWidget *parent, gint unitx, gint unity, GList *poi_list)
{
    static PoiListInfo pli = { NULL, NULL };
    static GtkWidget *scroller;
    static GtkWidget *btn_goto;
    static GtkWidget *btn_edit;
    static GtkWidget *btn_manage_checks;
    static GtkListStore *store;
    GtkTreeIter iter;
    GList *curr;
    gdouble src_lat, src_lon;
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(pli.dialog == NULL)
    {
        GtkCellRenderer *renderer;
        GtkTreeViewColumn *column;

        pli.dialog = gtk_dialog_new_with_buttons(_("POI List"),
            GTK_WINDOW(parent), GTK_DIALOG_MODAL,
                NULL);

        store = gtk_list_store_new(POI_NUM_COLUMNS,
                                   G_TYPE_BOOLEAN,/* Selected */
                                   G_TYPE_INT,    /* POI ID */
                                   G_TYPE_INT,    /* Category ID */
                                   G_TYPE_DOUBLE, /* Latitude */
                                   G_TYPE_DOUBLE, /* Longitude */
                                   G_TYPE_STRING, /* Lat/Lon */
                                   G_TYPE_FLOAT,  /* Bearing */
                                   G_TYPE_FLOAT,  /* Distance */
                                   G_TYPE_STRING, /* POI Label */
                                   G_TYPE_STRING, /* POI Desc. */
                                   G_TYPE_STRING);/* Category Label */

        /* Set up the tree view. */
        pli.tree_view = gtk_tree_view_new();
        g_object_set(G_OBJECT(pli.tree_view),
                "allow-checkbox-mode", FALSE, NULL);

        gtk_tree_selection_set_mode(
                gtk_tree_view_get_selection(GTK_TREE_VIEW(pli.tree_view)),
                GTK_SELECTION_SINGLE);
        gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(pli.tree_view), TRUE);

        renderer = gtk_cell_renderer_toggle_new();
        gtk_cell_renderer_toggle_set_active(GTK_CELL_RENDERER_TOGGLE(renderer),
                TRUE);
        g_signal_connect(G_OBJECT(renderer), "toggled",
                G_CALLBACK(poi_list_row_selected), store);
        pli.select_column = gtk_tree_view_column_new_with_attributes(
                "*", renderer, "active", POI_SELECTED, NULL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(pli.tree_view),
                pli.select_column);
        gtk_tree_view_column_set_clickable(pli.select_column, TRUE);
        g_signal_connect(G_OBJECT(pli.select_column), "clicked",
                G_CALLBACK(poi_list_select_all), &pli);

        renderer = gtk_cell_renderer_combo_new();
        column = gtk_tree_view_column_new_with_attributes(
                _("Category"), renderer, "text", POI_CLABEL, NULL);
        gtk_tree_view_column_set_sizing(column,GTK_TREE_VIEW_COLUMN_GROW_ONLY);
        gtk_tree_view_column_set_sort_column_id(column, POI_CLABEL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(pli.tree_view), column);

        renderer = gtk_cell_renderer_text_new();
        g_object_set(renderer, "xalign", 1.f, NULL);
        column = gtk_tree_view_column_new_with_attributes(
                _("Dist."), renderer, "text", POI_DISTANCE, NULL);
        gtk_tree_view_column_set_cell_data_func(column, renderer,
                (GtkTreeCellDataFunc)poi_list_distance_cell_data_func,
                NULL, NULL);
        gtk_tree_view_column_set_sort_column_id(column, POI_DISTANCE);
        gtk_tree_view_append_column(GTK_TREE_VIEW(pli.tree_view), column);

        renderer = gtk_cell_renderer_text_new();
        g_object_set(renderer, "xalign", 1.f, NULL);
        column = gtk_tree_view_column_new_with_attributes(
                _("Bear."), renderer, "text", POI_BEARING, NULL);
        gtk_tree_view_column_set_cell_data_func(column, renderer,
                (GtkTreeCellDataFunc)poi_list_bearing_cell_data_func,
                NULL, NULL);
        gtk_tree_view_column_set_sort_column_id(column, POI_BEARING);
        gtk_tree_view_append_column(GTK_TREE_VIEW(pli.tree_view), column);

        renderer = gtk_cell_renderer_text_new();
        column = gtk_tree_view_column_new_with_attributes(
                _("Label"), renderer, "text", POI_LABEL, NULL);
        gtk_tree_view_column_set_sort_column_id(column, POI_LABEL);
        gtk_tree_view_append_column(GTK_TREE_VIEW(pli.tree_view), column);

        g_signal_connect(G_OBJECT(pli.tree_view), "row-activated",
                G_CALLBACK(poi_list_row_activated), &pli);

        gtk_tree_view_set_model(GTK_TREE_VIEW(pli.tree_view),
                GTK_TREE_MODEL(store));
        g_object_unref(G_OBJECT(store));

        /* Enable the help button. */
#ifndef LEGACY
#else
        ossohelp_dialog_help_enable(
                GTK_DIALOG(pli.dialog), HELP_ID_POILIST, _osso);
#endif

        gtk_container_add(GTK_CONTAINER(GTK_DIALOG(pli.dialog)->action_area),
                btn_goto = gtk_button_new_with_label(_("Go to")));

        gtk_container_add(GTK_CONTAINER(GTK_DIALOG(pli.dialog)->action_area),
                btn_edit = gtk_button_new_with_label(_("Edit...")));

        gtk_container_add(GTK_CONTAINER(GTK_DIALOG(pli.dialog)->action_area),
                btn_manage_checks = gtk_button_new_with_label(
                    _("Checked POI Actions...")));

        gtk_dialog_add_button(GTK_DIALOG(pli.dialog),
                GTK_STOCK_CLOSE, GTK_RESPONSE_ACCEPT);

        gtk_window_set_default_size(GTK_WINDOW(pli.dialog), 500, 400);

        scroller = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroller),
                GTK_SHADOW_ETCHED_IN);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroller),
                GTK_POLICY_NEVER,
                GTK_POLICY_AUTOMATIC);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(pli.dialog)->vbox),
                scroller, TRUE, TRUE, 0);

        gtk_container_add(GTK_CONTAINER(scroller), pli.tree_view);

        g_signal_connect(G_OBJECT(btn_goto), "clicked",
                G_CALLBACK(poi_list_goto), &pli);

        g_signal_connect(G_OBJECT(btn_edit), "clicked",
                G_CALLBACK(poi_list_view), &pli);

        g_signal_connect(G_OBJECT(btn_manage_checks), "clicked",
                G_CALLBACK(poi_list_manage_checks), &pli);
    }

    /* Initialize the tree store. */

    gtk_list_store_clear(store);
    pli.select_all = FALSE;

    unit2latlon(unitx, unity, src_lat, src_lon);

    for(curr = poi_list; curr; curr = curr->next)
    {
        PoiInfo *poi_info = curr->data;
        gchar tmp1[LL_FMT_LEN], tmp2[LL_FMT_LEN];

        printf("poi: (%f, %f, %s, %s)\n",
                poi_info->lat, poi_info->lon,
                poi_info->label, poi_info->desc);

        format_lat_lon(poi_info->lat, poi_info->lon, tmp1, tmp2);
        //lat_format(poi_info->lat, tmp1);
        //lon_format(poi_info->lon, tmp2);

        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                POI_SELECTED, TRUE,
                POI_POIID, poi_info->poi_id,
                POI_LAT, poi_info->lat,
                POI_LON, poi_info->lon,
                POI_BEARING, calculate_bearing(src_lat, src_lon,
                    poi_info->lat, poi_info->lon),
                POI_DISTANCE, calculate_distance(src_lat,src_lon,
                    poi_info->lat, poi_info->lon) * UNITS_CONVERT[_units],
                POI_LABEL, poi_info->label,
                POI_DESC, poi_info->desc,
                POI_CATID, poi_info->cat_id,
                POI_CLABEL, poi_info->clabel,
                -1);
    }

    gtk_widget_show_all(pli.dialog);

    GTK_RESPONSE_ACCEPT == gtk_dialog_run(GTK_DIALOG(pli.dialog));

    map_force_redraw();

    gtk_widget_hide(pli.dialog);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

gboolean
poi_import_dialog(gint unitx, gint unity)
{
    GtkWidget *dialog = NULL;
    gboolean success = FALSE;
    printf("%s()\n", __PRETTY_FUNCTION__);

    dialog = hildon_file_chooser_dialog_new(GTK_WINDOW(_window),
            GTK_FILE_CHOOSER_ACTION_OPEN);

    gtk_widget_show_all(dialog);

    while(!success && gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK)
    {
        gchar *file_uri_str = NULL;
        gchar *bytes = NULL;
        gint size;
        GnomeVFSResult vfs_result;
        GList *poi_list = NULL;

        file_uri_str = gtk_file_chooser_get_uri(GTK_FILE_CHOOSER(dialog));

        /* Parse the given file as GPX. */
        if(GNOME_VFS_OK != (vfs_result = gnome_vfs_read_entire_file(
                        file_uri_str, &size, &bytes)))
        {
            popup_error(dialog, gnome_vfs_result_to_string(vfs_result));
        }
        else if(gpx_poi_parse(bytes, size, &poi_list))
        {
            static GtkWidget *cat_dialog = NULL;
            static GtkWidget *cmb_category = NULL;
            static GtkWidget *btn_catedit = NULL;
            static PoiCategoryEditInfo pcedit;

            if(!cat_dialog)
            {
                GtkWidget *hbox;
                GtkWidget *label;
                cat_dialog = gtk_dialog_new_with_buttons(_("Default Category"),
                        GTK_WINDOW(dialog), GTK_DIALOG_MODAL,
                        GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                        GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                        NULL);

                gtk_box_pack_start(GTK_BOX(GTK_DIALOG(cat_dialog)->vbox),
                        hbox = gtk_hbox_new(FALSE, 4), FALSE, FALSE, 4);

                gtk_box_pack_start(GTK_BOX(hbox),
                        label = gtk_label_new(_("Category")),
                        FALSE, FALSE, 0);

                gtk_box_pack_start(GTK_BOX(hbox),
                        cmb_category = poi_create_cat_combo(),
                        FALSE, FALSE, 4);

                gtk_box_pack_start(GTK_BOX(hbox),
                        btn_catedit = gtk_button_new_with_label(
                            _("Edit Categories...")),
                        FALSE, FALSE, 0);

                /* Connect Signals */
                pcedit.dialog = dialog;
                pcedit.cmb_category = cmb_category;
                pcedit.cat_id = -1;
                g_signal_connect(G_OBJECT(btn_catedit), "clicked",
                        G_CALLBACK(poi_edit_cat), &pcedit);
            }

            gtk_widget_show_all(cat_dialog);

            while(GTK_RESPONSE_ACCEPT ==gtk_dialog_run(GTK_DIALOG(cat_dialog)))
            {
                if(gtk_combo_box_get_active(GTK_COMBO_BOX(cmb_category)) == -1)
                {
                    popup_error(dialog,
                            _("Please specify a default category."));
                    continue;
                }

                /* Insert the POIs into the database. */
                gint num_inserts = poi_list_insert(dialog,
                        poi_list, GTK_COMBO_BOX(cmb_category));

                if(num_inserts)
                {
                    /* Hide the dialogs. */
                    gtk_widget_hide(cat_dialog);

                    /* Create a new dialog with the results. */
                    poi_list_dialog(dialog, unitx, unity, poi_list);
                    success = TRUE;
                }
                break;
            }

            gtk_widget_hide(cat_dialog);

            poi_list_free(poi_list);
        }
        else
            popup_error(dialog, _("Error parsing GPX file."));

        g_free(file_uri_str);
        g_free(bytes);
    }

    /* Hide the dialog. */
    gtk_widget_destroy(dialog);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return success;
}

static gboolean
poi_download_cat_selected(GtkComboBox *cmb_category, GtkEntry *txt_query)
{
    GtkTreeIter iter;
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(gtk_combo_box_get_active_iter(GTK_COMBO_BOX(cmb_category), &iter))
    {
        gchar buffer[BUFFER_SIZE];
        GtkWidget *confirm = NULL;
        gchar *category = NULL;

        gtk_tree_model_get(
                gtk_combo_box_get_model(GTK_COMBO_BOX(cmb_category)), &iter,
                1, &category,
                -1);

        if(*gtk_entry_get_text(txt_query))
        {
            snprintf(buffer, sizeof(buffer), "%s\n  %s",
                    _("Overwrite query with the following text?"), category);
            confirm = hildon_note_new_confirmation(GTK_WINDOW(_window),buffer);

        }

        if(confirm == NULL
                || GTK_RESPONSE_OK == gtk_dialog_run(GTK_DIALOG(confirm)))
            gtk_entry_set_text(txt_query, category);

        if(confirm)
            gtk_widget_destroy(confirm);
    }

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}


static gboolean
origin_type_selected(GtkWidget *toggle, OriginToggleInfo *oti)
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle)))
        gtk_widget_set_sensitive(oti->txt_origin, toggle == oti->rad_use_text);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

gboolean
poi_download_dialog(gint unitx, gint unity)
{
    static GtkWidget *dialog = NULL;
    static GtkWidget *hbox = NULL;
    static GtkWidget *table = NULL;
    static GtkWidget *table2 = NULL;
    static GtkWidget *label = NULL;
    static GtkWidget *num_page = NULL;
    static GtkWidget *txt_source_url = NULL;
    static OriginToggleInfo oti;
    static GtkWidget *cmb_category;
    printf("%s()\n", __PRETTY_FUNCTION__);

    conic_recommend_connected();

    if(!dialog)
    {
        GtkEntryCompletion *origin_comp;

        dialog = gtk_dialog_new_with_buttons(_("Download POIs"),
                GTK_WINDOW(_window), GTK_DIALOG_MODAL,
                GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                NULL);

        /* Enable the help button. */
#ifndef LEGACY
#else
        ossohelp_dialog_help_enable(
                GTK_DIALOG(dialog), HELP_ID_DOWNPOI, _osso);
#endif

        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
                table = gtk_table_new(4, 4, FALSE), TRUE, TRUE, 0);

        /* Source URL. */
        gtk_table_attach(GTK_TABLE(table),
                hbox = gtk_hbox_new(FALSE, 4),
                0, 4, 0, 1, GTK_EXPAND | GTK_FILL, 0, 2, 4);
        gtk_box_pack_start(GTK_BOX(hbox),
                label = gtk_label_new(_("Source URL")), FALSE, TRUE, 4);
        gtk_box_pack_start(GTK_BOX(hbox),
                txt_source_url = gtk_entry_new(), TRUE, TRUE, 4);

        /* Auto. */
        gtk_table_attach(GTK_TABLE(table),
                oti.rad_use_gps = gtk_radio_button_new_with_label(NULL,
                    _("Use GPS Location")),
                0, 1, 1, 2, GTK_FILL, 0, 2, 4);

        /* Use End of Route. */
        gtk_table_attach(GTK_TABLE(table),
               oti.rad_use_route = gtk_radio_button_new_with_label_from_widget(
                   GTK_RADIO_BUTTON(oti.rad_use_gps), _("Use End of Route")),
               0, 1, 2, 3, GTK_FILL, 0, 2, 4);


        gtk_table_attach(GTK_TABLE(table),
                gtk_vseparator_new(),
                1, 2, 1, 3, GTK_FILL, GTK_FILL, 2,4);

        /* Category. */
        gtk_table_attach(GTK_TABLE(table),
                label = gtk_label_new(_("Category")),
                2, 3, 1, 2, GTK_FILL, 0, 2, 4);
        gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
        gtk_table_attach(GTK_TABLE(table),
                cmb_category = poi_create_cat_combo(),
                3, 4, 1, 2, GTK_FILL, 0, 2, 4);

        /* Page. */
        gtk_table_attach(GTK_TABLE(table),
                label = gtk_label_new(_("Page")),
                2, 3, 2, 3, GTK_FILL, 0, 2, 4);
        gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
        gtk_table_attach(GTK_TABLE(table),
                num_page = hildon_number_editor_new(1, 999),
                3, 4, 2, 3, GTK_FILL, 0, 2, 4);


        /* Another table for the Origin and Query. */
        gtk_table_attach(GTK_TABLE(table),
                table2 = gtk_table_new(2, 2, FALSE),
                0, 4, 3, 4, GTK_EXPAND | GTK_FILL, 0, 2, 4);

        /* Origin. */
        gtk_table_attach(GTK_TABLE(table2),
                oti.rad_use_text = gtk_radio_button_new_with_label_from_widget(
                    GTK_RADIO_BUTTON(oti.rad_use_gps), _("Origin")),
                0, 1, 0, 1, GTK_FILL, 0, 2, 4);
        gtk_table_attach(GTK_TABLE(table2),
                oti.txt_origin = gtk_entry_new(),
                1, 2, 0, 1, GTK_EXPAND | GTK_FILL, 0, 2, 4);
        gtk_entry_set_width_chars(GTK_ENTRY(oti.txt_origin), 25);
#ifdef MAEMO_CHANGES
#ifndef LEGACY
        g_object_set(G_OBJECT(oti.txt_origin), "hildon-input-mode",
                HILDON_GTK_INPUT_MODE_FULL, NULL);
#else
        g_object_set(G_OBJECT(oti.txt_origin), HILDON_AUTOCAP, FALSE, NULL);
#endif
#endif

        /* Query. */
        gtk_table_attach(GTK_TABLE(table2),
                label = gtk_label_new(_("Query")),
                0, 1, 1, 2, GTK_FILL, 0, 2, 4);
        gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
        gtk_table_attach(GTK_TABLE(table2),
                oti.txt_query = gtk_entry_new(),
                1, 2, 1, 2, GTK_EXPAND | GTK_FILL, 0, 2, 4);
        gtk_entry_set_width_chars(GTK_ENTRY(oti.txt_query), 25);
#ifdef MAEMO_CHANGES
#ifndef LEGACY
        g_object_set(G_OBJECT(oti.txt_query), "hildon-input-mode",
                HILDON_GTK_INPUT_MODE_FULL, NULL);
#else
        g_object_set(G_OBJECT(oti.txt_query), HILDON_AUTOCAP, FALSE, NULL);
#endif
#endif

        /* Set up auto-completion. */
        origin_comp = gtk_entry_completion_new();
        gtk_entry_completion_set_model(origin_comp,GTK_TREE_MODEL(_loc_model));
        gtk_entry_completion_set_text_column(origin_comp, 0);
        gtk_entry_set_completion(GTK_ENTRY(oti.txt_origin), origin_comp);

        g_signal_connect(G_OBJECT(oti.rad_use_gps), "toggled",
                          G_CALLBACK(origin_type_selected), &oti);
        g_signal_connect(G_OBJECT(oti.rad_use_route), "toggled",
                          G_CALLBACK(origin_type_selected), &oti);
        g_signal_connect(G_OBJECT(oti.rad_use_text), "toggled",
                          G_CALLBACK(origin_type_selected), &oti);

        g_signal_connect(G_OBJECT(cmb_category), "changed",
                G_CALLBACK(poi_download_cat_selected), oti.txt_query);
    }

    /* Initialize fields. */

    hildon_number_editor_set_value(HILDON_NUMBER_EDITOR(num_page), 1);

    gtk_entry_set_text(GTK_ENTRY(txt_source_url), _poi_dl_url);
    if(unity != 0)
    {
        gchar buffer[80];
        gchar strlat[32];
        gchar strlon[32];
        gdouble lat, lon;

        unit2latlon(unitx, unity, lat, lon);

        g_ascii_formatd(strlat, 32, "%.06f", lat);
        g_ascii_formatd(strlon, 32, "%.06f", lon);
        snprintf(buffer, sizeof(buffer), "%s, %s", strlat, strlon);

        gtk_entry_set_text(GTK_ENTRY(oti.txt_origin), buffer);
        gtk_toggle_button_set_active(
                GTK_TOGGLE_BUTTON(oti.rad_use_text), TRUE);
    }
    /* Else use "End of Route" by default if they have a route. */
    else if(_route.head != _route.tail)
    {
        /* There is no route, so make it the default. */
        gtk_widget_set_sensitive(oti.rad_use_route, TRUE);
        gtk_toggle_button_set_active(
                GTK_TOGGLE_BUTTON(oti.rad_use_route), TRUE);
        gtk_widget_grab_focus(oti.rad_use_route);
    }
    /* Else use "GPS Location" if they have GPS enabled. */
    else
    {
        /* There is no route, so desensitize "Use End of Route." */
        gtk_widget_set_sensitive(oti.rad_use_route, FALSE);
        if(_enable_gps)
        {
            gtk_toggle_button_set_active(
                    GTK_TOGGLE_BUTTON(oti.rad_use_gps), TRUE);
            gtk_widget_grab_focus(oti.rad_use_gps);
        }
        /* Else use text. */
        else
        {
            gtk_toggle_button_set_active(
                    GTK_TOGGLE_BUTTON(oti.rad_use_text), TRUE);
            gtk_widget_grab_focus(oti.txt_origin);
        }
    }

    gtk_widget_show_all(dialog);

    while(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
    {
        gchar origin_buffer[BUFFER_SIZE];
        const gchar *source_url, *origin, *query;
        gchar *file_uri_str = NULL;
        gchar *bytes = NULL;
        gint size;
        GnomeVFSResult vfs_result;
        GList *poi_list = NULL;

        source_url = gtk_entry_get_text(GTK_ENTRY(txt_source_url));
        if(!strlen(source_url))
        {
            popup_error(dialog, _("Please specify a source URL."));
            continue;
        }
        else
        {
            g_free(_poi_dl_url);
            _poi_dl_url = g_strdup(source_url);
        }

        if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(oti.rad_use_gps)))
        {
            gchar strlat[32];
            gchar strlon[32];
            latlon2unit(_gps.lat, _gps.lon, unitx, unity);
            g_ascii_formatd(strlat, 32, "%.06f", _gps.lat);
            g_ascii_formatd(strlon, 32, "%.06f", _gps.lon);
            snprintf(origin_buffer, sizeof(origin_buffer),
                    "%s, %s", strlat, strlon);
            origin = origin_buffer;
        }
        else if(gtk_toggle_button_get_active(
                    GTK_TOGGLE_BUTTON(oti.rad_use_route)))
        {
            gchar strlat[32];
            gchar strlon[32];
            Point *p;
            gdouble lat, lon;

            /* Use last non-zero route point. */
            for(p = _route.tail; !p->unity; p--) { }

            unitx = p->unitx;
            unity = p->unity;
            unit2latlon(p->unitx, p->unity, lat, lon);
            g_ascii_formatd(strlat, 32, "%.06f", lat);
            g_ascii_formatd(strlon, 32, "%.06f", lon);
            snprintf(origin_buffer, sizeof(origin_buffer),
                    "%s, %s", strlat, strlon);
            origin = origin_buffer;
        }
        else
        {
            Point porig;
            origin = gtk_entry_get_text(GTK_ENTRY(oti.txt_origin));
            if(*origin)
            {
                porig = locate_address(dialog, origin);
                if(!porig.unity)
                    continue;
            }
        }

        if(!*origin)
        {
            popup_error(dialog, _("Please specify an origin."));
            continue;
        }

        if(gtk_combo_box_get_active(GTK_COMBO_BOX(cmb_category)) == -1)
        {
            popup_error(dialog, _("Please specify a default category."));
            continue;
        }

        query = gtk_entry_get_text(GTK_ENTRY(oti.txt_query));
        if(!strlen(query))
        {
            popup_error(dialog, _("Please specify a query."));
            continue;
        }

        /* Construct the URL. */
        {
            gchar *origin_escaped;
            gchar *query_escaped;

            origin_escaped = gnome_vfs_escape_string(origin);
            query_escaped = gnome_vfs_escape_string(query);
            file_uri_str = g_strdup_printf(
                    source_url, origin_escaped, query_escaped,
                    hildon_number_editor_get_value(
                        HILDON_NUMBER_EDITOR(num_page)));
            g_free(origin_escaped);
            g_free(query_escaped);
        }

        /* Parse the given file as GPX. */
        if(GNOME_VFS_OK != (vfs_result = gnome_vfs_read_entire_file(
                        file_uri_str, &size, &bytes)))
        {
            popup_error(dialog, gnome_vfs_result_to_string(vfs_result));
        }
        else if(strncmp(bytes, "<?xml", strlen("<?xml")))
        {
            /* Not an XML document - must be bad locations. */
            popup_error(dialog, _("Invalid origin or query."));
            printf("bytes: %s\n", bytes);
        }
        else if(gpx_poi_parse(bytes, size, &poi_list))
        {
            /* Insert the POIs into the database. */
            gint num_inserts = poi_list_insert(dialog, poi_list,
                    GTK_COMBO_BOX(cmb_category));

            if(num_inserts)
            {
                /* Create a new dialog with the results. */
                poi_list_dialog(dialog, unitx, unity, poi_list);
            }

            poi_list_free(poi_list);
        }
        else
            popup_error(dialog, _("Error parsing GPX file."));

        g_free(file_uri_str);
        g_free(bytes);

        /* Increment the page number for them. */
        hildon_number_editor_set_value(HILDON_NUMBER_EDITOR(num_page),
            hildon_number_editor_get_value(HILDON_NUMBER_EDITOR(num_page)) +1);
    }

    /* Hide the dialog. */
    gtk_widget_hide(dialog);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

gboolean
poi_browse_dialog(gint unitx, gint unity)
{
    static GtkWidget *dialog = NULL;
    static GtkWidget *table = NULL;
    static GtkWidget *table2 = NULL;
    static GtkWidget *label = NULL;
    static GtkWidget *cmb_category = NULL;
    static OriginToggleInfo oti;
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(!dialog)
    {
        GtkEntryCompletion *origin_comp;

        dialog = gtk_dialog_new_with_buttons(_("Browse POIs"),
                GTK_WINDOW(_window), GTK_DIALOG_MODAL,
                GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
                NULL);

        /* Enable the help button. */
#ifndef LEGACY
#else
        ossohelp_dialog_help_enable(
                GTK_DIALOG(dialog), HELP_ID_BROWSEPOI, _osso);
#endif

        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
                table = gtk_table_new(3, 4, FALSE), TRUE, TRUE, 0);

        /* Auto. */
        gtk_table_attach(GTK_TABLE(table),
                oti.rad_use_gps = gtk_radio_button_new_with_label(NULL,
                    _("Use GPS Location")),
                0, 1, 0, 1, GTK_FILL, 0, 2, 4);

        /* Use End of Route. */
        gtk_table_attach(GTK_TABLE(table),
               oti.rad_use_route = gtk_radio_button_new_with_label_from_widget(
                   GTK_RADIO_BUTTON(oti.rad_use_gps), _("Use End of Route")),
               0, 1, 1, 2, GTK_FILL, 0, 2, 4);

        gtk_table_attach(GTK_TABLE(table),
                gtk_vseparator_new(),
                1, 2, 0, 2, GTK_FILL, GTK_FILL, 2, 4);

        /* Category. */
        gtk_table_attach(GTK_TABLE(table),
                label = gtk_label_new(_("Category")),
                2, 3, 0, 1, GTK_FILL, 0, 2, 4);
        gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
        gtk_table_attach(GTK_TABLE(table),
                cmb_category = poi_create_cat_combo(),
                3, 4, 0, 1, GTK_FILL, 0, 2, 4);
        /* Add an extra, "<any>" category. */
        {
            GtkTreeIter iter;
            GtkListStore *store = GTK_LIST_STORE(gtk_combo_box_get_model(
                        GTK_COMBO_BOX(cmb_category)));
            gtk_list_store_prepend(store, &iter);
            gtk_list_store_set(store, &iter, 0, -1, 1, "<any>", -1);
            gtk_combo_box_set_active_iter(GTK_COMBO_BOX(cmb_category), &iter);
        }


        /* Another table for the Origin and Query. */
        gtk_table_attach(GTK_TABLE(table),
                table2 = gtk_table_new(2, 2, FALSE),
                0, 4, 2, 3, GTK_EXPAND | GTK_FILL, 0, 2, 4);

        /* Origin. */
        gtk_table_attach(GTK_TABLE(table2),
                oti.rad_use_text = gtk_radio_button_new_with_label_from_widget(
                    GTK_RADIO_BUTTON(oti.rad_use_gps), _("Origin")),
                0, 1, 0, 1, GTK_FILL, 0, 2, 4);
        gtk_table_attach(GTK_TABLE(table2),
                oti.txt_origin = gtk_entry_new(),
                1, 2, 0, 1, GTK_EXPAND | GTK_FILL, 0, 2, 4);
        gtk_entry_set_width_chars(GTK_ENTRY(oti.txt_origin), 25);
#ifdef MAEMO_CHANGES
#ifndef LEGACY
        g_object_set(G_OBJECT(oti.txt_origin), "hildon-input-mode",
                HILDON_GTK_INPUT_MODE_FULL, NULL);
#else
        g_object_set(G_OBJECT(oti.txt_origin), HILDON_AUTOCAP, FALSE, NULL);
#endif
#endif

        /* Destination. */
        gtk_table_attach(GTK_TABLE(table2),
                label = gtk_label_new(_("Query")),
                0, 1, 1, 2, GTK_FILL, 0, 2, 4);
        gtk_misc_set_alignment(GTK_MISC(label), 1.f, 0.5f);
        gtk_table_attach(GTK_TABLE(table2),
                oti.txt_query = gtk_entry_new(),
                1, 2, 1, 2, GTK_EXPAND | GTK_FILL, 0, 2, 4);
        gtk_entry_set_width_chars(GTK_ENTRY(oti.txt_query), 25);
#ifdef MAEMO_CHANGES
#ifndef LEGACY
        g_object_set(G_OBJECT(oti.txt_query), "hildon-input-mode",
                HILDON_GTK_INPUT_MODE_FULL, NULL);
#else
        g_object_set(G_OBJECT(oti.txt_query), HILDON_AUTOCAP, FALSE, NULL);
#endif
#endif

        /* Set up auto-completion. */
        origin_comp = gtk_entry_completion_new();
        gtk_entry_completion_set_model(origin_comp,GTK_TREE_MODEL(_loc_model));
        gtk_entry_completion_set_text_column(origin_comp, 0);
        gtk_entry_set_completion(GTK_ENTRY(oti.txt_origin), origin_comp);

        g_signal_connect(G_OBJECT(oti.rad_use_gps), "toggled",
                          G_CALLBACK(origin_type_selected), &oti);
        g_signal_connect(G_OBJECT(oti.rad_use_route), "toggled",
                          G_CALLBACK(origin_type_selected), &oti);
        g_signal_connect(G_OBJECT(oti.rad_use_text), "toggled",
                          G_CALLBACK(origin_type_selected), &oti);
    }

    /* Initialize fields. */

    if(unity != 0)
    {
        gchar buffer[80];
        gchar strlat[32];
        gchar strlon[32];
        gdouble lat, lon;

        unit2latlon(unitx, unity, lat, lon);

        g_ascii_formatd(strlat, 32, "%.06f", lat);
        g_ascii_formatd(strlon, 32, "%.06f", lon);
        snprintf(buffer, sizeof(buffer), "%s, %s", strlat, strlon);

        gtk_entry_set_text(GTK_ENTRY(oti.txt_origin), buffer);
        gtk_toggle_button_set_active(
                GTK_TOGGLE_BUTTON(oti.rad_use_text), TRUE);
    }
    /* Else use "End of Route" by default if they have a route. */
    else if(_route.head != _route.tail)
    {
        /* There is no route, so make it the default. */
        gtk_widget_set_sensitive(oti.rad_use_route, TRUE);
        gtk_toggle_button_set_active(
                GTK_TOGGLE_BUTTON(oti.rad_use_route), TRUE);
        gtk_widget_grab_focus(oti.rad_use_route);
    }
    /* Else use "GPS Location" if they have GPS enabled. */
    else
    {
        /* There is no route, so desensitize "Use End of Route." */
        gtk_widget_set_sensitive(oti.rad_use_route, FALSE);
        if(_enable_gps)
        {
            gtk_toggle_button_set_active(
                    GTK_TOGGLE_BUTTON(oti.rad_use_gps), TRUE);
            gtk_widget_grab_focus(oti.rad_use_gps);
        }
        /* Else use text. */
        else
        {
            gtk_toggle_button_set_active(
                    GTK_TOGGLE_BUTTON(oti.rad_use_text), TRUE);
            gtk_widget_grab_focus(oti.txt_origin);
        }
    }

    gtk_widget_show_all(dialog);

    while(gtk_dialog_run(GTK_DIALOG(dialog)) ==GTK_RESPONSE_ACCEPT)
    {
        gchar buffer[BUFFER_SIZE];
        const gchar *origin, *query;
        gdouble lat, lon;
        GList *poi_list = NULL;
        gint cat_id;
        gboolean is_cat = FALSE;
        sqlite3_stmt *stmt;

        if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(oti.rad_use_gps)))
        {
            gchar strlat[32];
            gchar strlon[32];
            latlon2unit(_gps.lat, _gps.lon, unitx, unity);
            g_ascii_formatd(strlat, 32, "%.06f", _gps.lat);
            g_ascii_formatd(strlon, 32, "%.06f", _gps.lon);
            snprintf(buffer, sizeof(buffer), "%s, %s", strlat, strlon);
            origin = buffer;
        }
        else if(gtk_toggle_button_get_active(
                    GTK_TOGGLE_BUTTON(oti.rad_use_route)))
        {
            gchar strlat[32];
            gchar strlon[32];
            Point *p;
            gdouble lat, lon;

            /* Use last non-zero route point. */
            for(p = _route.tail; !p->unity; p--) { }

            unitx = p->unitx;
            unity = p->unity;
            unit2latlon(p->unitx, p->unity, lat, lon);
            g_ascii_formatd(strlat, 32, "%.06f", lat);
            g_ascii_formatd(strlon, 32, "%.06f", lon);
            snprintf(buffer, sizeof(buffer), "%s, %s", strlat, strlon);
            origin = buffer;
        }
        else
        {
            Point porig;
            origin = gtk_entry_get_text(GTK_ENTRY(oti.txt_origin));
            porig = locate_address(dialog, origin);
            if(!porig.unity)
                continue;
        }

        if(!strlen(origin))
        {
            popup_error(dialog, _("Please specify an origin."));
            continue;
        }

        /* Check if we're doing a category search. */
        {
            GtkTreeIter iter;
            if(gtk_combo_box_get_active_iter(
                    GTK_COMBO_BOX(cmb_category), &iter))
            {
                gtk_tree_model_get(
                        gtk_combo_box_get_model(GTK_COMBO_BOX(cmb_category)),
                        &iter, 0, &cat_id, -1);
                if(cat_id >= 0)
                {
                    is_cat = TRUE;
                }
            }
        }

        query = g_strdup_printf("%%%s%%",
                gtk_entry_get_text(GTK_ENTRY(oti.txt_query)));

        unit2latlon(unitx, unity, lat, lon);

        if(is_cat)
        {
            if(SQLITE_OK != sqlite3_bind_int(_stmt_browsecat_poi, 1, cat_id) ||
               SQLITE_OK != sqlite3_bind_text(_stmt_browsecat_poi, 2, query,
                   -1, g_free) ||
               SQLITE_OK != sqlite3_bind_double(_stmt_browsecat_poi, 3, lat) ||
               SQLITE_OK != sqlite3_bind_double(_stmt_browsecat_poi, 4, lon))
            {
                g_printerr("Failed to bind values for _stmt_browsecat_poi\n");
                continue;
            }
            stmt = _stmt_browsecat_poi;
        }
        else
        {
            if(SQLITE_OK != sqlite3_bind_text(_stmt_browse_poi, 1, query,
                        -1, g_free) ||
               SQLITE_OK != sqlite3_bind_double(_stmt_browse_poi, 2, lat) ||
               SQLITE_OK != sqlite3_bind_double(_stmt_browse_poi, 3, lon))
            {
                g_printerr("Failed to bind values for _stmt_browse_poi\n");
                continue;
            }
            stmt = _stmt_browse_poi;
        }

        while(SQLITE_ROW == sqlite3_step(stmt))
        {
            PoiInfo *poi = g_slice_new(PoiInfo);
            poi->poi_id = sqlite3_column_int(stmt, 0);
            poi->cat_id = sqlite3_column_int(stmt, 1);
            poi->lat = sqlite3_column_double(stmt, 2);
            poi->lon = sqlite3_column_double(stmt, 3);
            poi->label =g_strdup(sqlite3_column_text(stmt, 4));
            poi->desc = g_strdup(sqlite3_column_text(stmt, 5));
            poi->clabel=g_strdup(sqlite3_column_text(stmt, 6));
            poi_list = g_list_prepend(poi_list, poi);
        }
        sqlite3_reset(stmt);

        if(poi_list)
        {
            /* Create a new dialog with the results. */
            poi_list_dialog(dialog, unitx, unity, poi_list);
            poi_list_free(poi_list);
        }
        else
            popup_error(dialog, _("No POIs found."));
    }

    map_force_redraw();

    /* Hide the dialog. */
    gtk_widget_hide(dialog);

    vprintf("%s(): return TRUE\n", __PRETTY_FUNCTION__);
    return TRUE;
}

/**
 * Render all the POI data.  This should be done before rendering track data.
 */
void
map_render_poi()
{
    gint unitx, unity;
    gdouble lat1, lat2, lon1, lon2;
    gchar buffer[100];
    gint poix, poiy;
    GdkPixbuf *pixbuf = NULL;
    GError *error = NULL;
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(_poi_db && _poi_zoom > _zoom)
    {
        gint diag_offset = pixel2unit(MAX(_view_width_pixels,
                    _view_height_pixels) / 2);
        buf2unit(0, _view_height_pixels, unitx, unity);
        unitx = _center.unitx - diag_offset;
        unity = _center.unity + diag_offset;
        unit2latlon(unitx, unity, lat1, lon1);
        unitx = _center.unitx + diag_offset;
        unity = _center.unity - diag_offset;
        unit2latlon(unitx, unity, lat2, lon2);

        if(SQLITE_OK != sqlite3_bind_double(_stmt_select_poi, 1, lat1) ||
           SQLITE_OK != sqlite3_bind_double(_stmt_select_poi, 2, lat2) ||
           SQLITE_OK != sqlite3_bind_double(_stmt_select_poi, 3, lon1) ||
           SQLITE_OK != sqlite3_bind_double(_stmt_select_poi, 4, lon2))
        {
            g_printerr("Failed to bind values for _stmt_select_poi\n");
            return;
        }

        while(SQLITE_ROW == sqlite3_step(_stmt_select_poi))
        {
            lat1 = sqlite3_column_double(_stmt_select_poi, 0);
            lon1 = sqlite3_column_double(_stmt_select_poi, 1);
            gchar *poi_label = g_utf8_strdown(sqlite3_column_text(
                    _stmt_select_poi, 3), -1);
            gchar *cat_label = g_utf8_strdown(sqlite3_column_text(
                    _stmt_select_poi, 6), -1);

            latlon2unit(lat1, lon1, unitx, unity);
            unit2buf(unitx, unity, poix, poiy);

            /* Try to get icon for specific POI first. */
            snprintf(buffer, sizeof(buffer), "%s/%s.jpg",
                    _poi_db_dirname, poi_label);
            pixbuf = gdk_pixbuf_new_from_file(buffer, &error);
            if(error)
            {
                /* No icon for specific POI - try for category. */
                error = NULL;
                snprintf(buffer, sizeof(buffer), "%s/%s.jpg",
                        _poi_db_dirname, cat_label);
                pixbuf = gdk_pixbuf_new_from_file(buffer, &error);
            }
            if(error)
            {
                /* No icon for POI or for category.
                 * Try default POI icon file. */
                error = NULL;
                snprintf(buffer, sizeof(buffer), "%s/poi.jpg",
                        _poi_db_dirname);
                pixbuf = gdk_pixbuf_new_from_file(buffer, &error);
            }
            if(error)
            {
                /* No icon for POI or for category or default POI icon file.
                   Draw default purple square. */
                error = NULL;
                gdk_draw_rectangle(_map_pixmap, _gc[COLORABLE_POI], TRUE,
                        poix - (gint)(1.5f * _draw_width),
                        poiy - (gint)(1.5f * _draw_width),
                        3 * _draw_width,
                        3 * _draw_width);
            }
            else
            {
                /* We found an icon to draw. */
                gdk_draw_pixbuf(
                        _map_pixmap,
                        _gc[COLORABLE_POI],
                        pixbuf,
                        0, 0,
                        poix - gdk_pixbuf_get_width(pixbuf) / 2,
                        poiy - gdk_pixbuf_get_height(pixbuf) / 2,
                        -1,-1,
                        GDK_RGB_DITHER_NONE, 0, 0);
                g_object_unref(pixbuf);
            }

            g_free(poi_label);
            g_free(cat_label);
        }
        sqlite3_reset(_stmt_select_poi);
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

void
poi_destroy()
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(_poi_db) 
    { 
        sqlite3_close(_poi_db); 
        _poi_db = NULL; 
    }

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

#ifdef INCLUDE_APRS
extern AprsDataRow *n_first;               // pointer to first element in name sorted station list


/////////////////////

/**
 * Render all the APRS data.
 */
void
map_render_aprs()
{
    printf("%s()\n", __PRETTY_FUNCTION__);

    if(_poi_zoom > _zoom)
    {

        AprsDataRow *p_station = n_first;

        while ( (p_station) != NULL) 
        {
            if( p_station->coord_lat != 0.0f 
                || p_station->coord_lon != 0.0f  ) 
            {
            		plot_aprs_station( p_station, FALSE);
            } // If valid data

            (p_station) = (p_station)->n_next;  // Next element in list
        } // End of while loop


    } // check for zoom level

    vprintf("%s(): return\n", __PRETTY_FUNCTION__);
}

#endif // INCLUDE_APRS

