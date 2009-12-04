/* vi: set et sw=4 ts=8 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 8 -*- */
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
#include "screen.h"

#include "data.h"
#include "defines.h"
#include "display.h"
#include "maps.h"
#include "math.h"
#include "osm.h"
#include "path.h"
#include "tile.h"
#include "util.h"

#include <cairo/cairo.h>
#include <hildon/hildon-banner.h>
#include <hildon/hildon-defines.h>

#define SCALE_WIDTH     100
#define SCALE_HEIGHT    22
#define ZOOM_WIDTH      25
#define ZOOM_HEIGHT     SCALE_HEIGHT

#define VELVEC_SIZE_FACTOR 4
#define MARK_WIDTH      (_draw_width * 4)
#define MARK_HEIGHT     100

/* (im)precision of a finger tap, in screen pixels */
#define TOUCH_RADIUS    25

struct _MapScreenPrivate
{
    ClutterActor *map;
    gint map_center_ux;
    gint map_center_uy;

    ClutterActor *tile_group;
    ClutterActor *poi_group;

    /* On-screen Menu */
    ClutterActor *osm;

    ClutterActor *compass;
    ClutterActor *compass_north;
    ClutterActor *scale;
    ClutterActor *zoom_box;

    ClutterActor *message_box;

    /* marker for the GPS position/speed */
    ClutterActor *mark;

    /* layer for drawing over the map (used for paths) */
    ClutterActor *overlay;

    /* center of the draw layer, in pixels */
    gint overlay_center_px; /* Y is the same */

    /* upper left of the draw layer, in units */
    gint overlay_start_px;
    gint overlay_start_py;

    /* coordinates of last button press; reset x to -1 when released */
    gint btn_press_screen_x;
    gint btn_press_screen_y;

    gint zoom;

    guint source_overlay_redraw;

    /* Set this flag to TRUE when there is an action ongoing which requires
     * touchscreen interaction: this prevents the OSM from popping up */
    guint action_ongoing : 1;
    /* Set this flag to TRUE if action_ongoing is TRUE, but you still want to
     * allow the map to be scrolled (i.e., you are interested only in tap
     * events, not motion ones */
    guint allow_scrolling : 1;

    guint is_dragging : 1;

    guint show_compass : 1;

    guint is_disposed : 1;
};

G_DEFINE_TYPE(MapScreen, map_screen, GTK_CLUTTER_TYPE_EMBED);

#define MAP_SCREEN_PRIV(screen) (MAP_SCREEN(screen)->priv)

static void
actor_set_rotation_cb(ClutterActor *actor, gpointer angle)
{
    clutter_actor_set_rotation(actor, CLUTTER_Z_AXIS,
                               GPOINTER_TO_INT(angle), 0, 0, 0);
}

static void
map_screen_pixel_to_screen_units(MapScreenPrivate *priv, gint px, gint py,
                                 gint *ux, gint *uy)
{
    gfloat x, y, angle, sin_angle, cos_angle;
    gint px2, py2;

    angle = clutter_actor_get_rotation(priv->map, CLUTTER_Z_AXIS,
                                       NULL, NULL, NULL);
    angle = angle * M_PI / 180;
    cos_angle = cos(angle);
    sin_angle = sin(angle);
    x = px, y = py;
    px2 = x * cos_angle + y * sin_angle;
    py2 = y * cos_angle - x * sin_angle;
    *ux = pixel2zunit(px2, priv->zoom);
    *uy = pixel2zunit(py2, priv->zoom);
}

static inline void
map_screen_pixel_to_units(MapScreen *screen, gint px, gint py,
                          gint *ux, gint *uy)
{
    MapScreenPrivate *priv = screen->priv;
    GtkAllocation *allocation = &(GTK_WIDGET(screen)->allocation);
    gint usx, usy;

    px -= allocation->width / 2;
    py -= allocation->height / 2;
    map_screen_pixel_to_screen_units(priv, px, py, &usx, &usy);
    *ux = usx + priv->map_center_ux;
    *uy = usy + priv->map_center_uy;
}

static inline void
activate_point_menu(MapScreen *screen, ClutterButtonEvent *event)
{
    MapController *controller;
    gint x, y;

    /* Get the coordinates of the point, in units */
    map_screen_pixel_to_units(screen, event->x, event->y, &x, &y);

    controller = map_controller_get_instance();
    map_controller_activate_menu_point(controller, x, y);
}

static gboolean
on_point_chosen(ClutterActor *actor, ClutterButtonEvent *event,
                MapScreen *screen)
{
    MapScreenPrivate *priv = screen->priv;

    g_signal_handlers_disconnect_by_func(actor, on_point_chosen, screen);
    clutter_actor_hide(priv->message_box);
    priv->action_ongoing = FALSE;

    activate_point_menu(screen, event);
    return TRUE; /* Event handled */
}

static gboolean
on_pointer_event(ClutterActor *actor, ClutterEvent *event, MapScreen *screen)
{
    MapScreenPrivate *priv = screen->priv;
    gboolean handled = FALSE;
    gint dx, dy;

    if (event->type == CLUTTER_BUTTON_PRESS)
    {
        ClutterButtonEvent *be = (ClutterButtonEvent *)event;
        priv->btn_press_screen_x = be->x;
        priv->btn_press_screen_y = be->y;
    }
    else if (event->type == CLUTTER_BUTTON_RELEASE)
    {
        ClutterButtonEvent *be = (ClutterButtonEvent *)event;

        /* if the screen was not being dragged, show the On-Screen Menu */
        if (!priv->is_dragging && !priv->action_ongoing)
            clutter_actor_show(priv->osm);
        else if (priv->is_dragging)
        {
            MapController *controller;
            Point p;

            map_screen_pixel_to_screen_units(priv,
                                             be->x - priv->btn_press_screen_x,
                                             be->y - priv->btn_press_screen_y,
                                             &dx, &dy);
            p.unitx = priv->map_center_ux - dx;
            p.unity = priv->map_center_uy - dy;
            controller = map_controller_get_instance();
            map_controller_disable_auto_center(controller);
            map_controller_set_center(controller, p, -1);
            handled = TRUE;
        }

        priv->btn_press_screen_x = -1;
        priv->is_dragging = FALSE;

        if (be->click_count > 1)
        {
            /* activating the point menu seems a reasonable action for double
             * clicks */
            activate_point_menu(screen, be);
        }
    }
    else if (!priv->action_ongoing || priv->allow_scrolling) /* motion event */
    {
        ClutterMotionEvent *me = (ClutterMotionEvent *)event;

        if (!(me->modifier_state & CLUTTER_BUTTON1_MASK))
            return TRUE; /* ignore pure motion events */

        dx = me->x - priv->btn_press_screen_x;
        dy = me->y - priv->btn_press_screen_y;

        if (!priv->is_dragging &&
            (ABS(dx) > TOUCH_RADIUS || ABS(dy) > TOUCH_RADIUS))
        {
            priv->is_dragging = TRUE;
            clutter_actor_hide(priv->osm);
        }

        if (priv->is_dragging)
        {
            GtkAllocation *allocation = &(GTK_WIDGET(screen)->allocation);

            clutter_actor_set_position(priv->map,
                                       allocation->width / 2 + dx,
                                       allocation->height / 2 + dy);
            handled = TRUE;
        }
    }

    return handled;
}

static void
map_screen_change_zoom_by_step(MapScreen *self, gboolean zoom_in)
{
    MapScreenPrivate *priv;
    gint new_zoom, sign;

    g_return_if_fail(MAP_IS_SCREEN(self));
    priv = self->priv;

    /* TODO: reimplement, once the old map is dropped */
    sign = zoom_in ? -1 : 1;
    new_zoom = priv->zoom + _curr_repo->view_zoom_steps * sign;
    BOUND(new_zoom, 0, MAX_ZOOM);
    if (new_zoom == priv->zoom) return;

    {
        gchar buffer[80];
        snprintf(buffer, sizeof(buffer),"%s %d",
                 _("Zoom to Level"), new_zoom);
        MACRO_BANNER_SHOW_INFO(_window, buffer);
    }

    map_set_zoom(new_zoom);
}

static inline void
point_to_pixels(MapScreenPrivate *priv, Point *p, gint *x, gint *y)
{
    *x = unit2zpixel(p->unitx, priv->zoom) - priv->overlay_start_px;
    *y = unit2zpixel(p->unity, priv->zoom) - priv->overlay_start_py;
}

static inline void
set_source_color(cairo_t *cr, GdkColor *color)
{
    cairo_set_source_rgb(cr,
                         color->red / 65535.0,
                         color->green / 65535.0,
                         color->blue / 65535.0);
}

static void
draw_break(cairo_t *cr, GdkColor *color, gint x, gint y)
{
    cairo_save(cr);
    cairo_new_sub_path(cr);
    cairo_arc(cr, x, y, _draw_width, 0, 2 * M_PI);
    cairo_set_line_width(cr, _draw_width);
    set_source_color(cr, color);
    cairo_stroke(cr);
    cairo_restore(cr);
}

static void
draw_path(MapScreen *screen, cairo_t *cr, Path *path, Colorable base)
{
    MapScreenPrivate *priv = screen->priv;
    Point *curr;
    WayPoint *wcurr;
    gint x = 0, y = 0;
    gboolean segment_open = FALSE;

    g_debug ("%s", G_STRFUNC);

    set_source_color(cr, &_color[base]);
    cairo_set_line_width(cr, _draw_width);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_BEVEL);
    for (curr = path->head, wcurr = path->whead; curr <= path->tail; curr++)
    {
        if (G_UNLIKELY(curr->unity == 0))
        {
            if (segment_open)
            {
                /* use x and y from the previous iteration */
                cairo_stroke(cr);
                draw_break(cr, &_color[base + 2], x, y);
                segment_open = FALSE;
            }
        }
        else
        {
            point_to_pixels(priv, curr, &x, &y);
            if (G_UNLIKELY(!segment_open))
            {
                draw_break(cr, &_color[base + 2], x, y);
                cairo_move_to(cr, x, y);
                segment_open = TRUE;
            }
            else
                cairo_line_to(cr, x, y);
        }

        if (wcurr->point == curr)
        {
            gint x1, y1;
            point_to_pixels(priv, wcurr->point, &x1, &y1);
            draw_break(cr, &_color[base + 1], x1, y1);
            cairo_move_to(cr, x1, y1);
            if (wcurr <= path->wtail)
                wcurr++;
        }
    }
    cairo_stroke(cr);
}

static void
draw_paths(MapScreen *screen, cairo_t *cr)
{
    if ((_show_paths & ROUTES_MASK) && _route.head != _route.tail)
    {
        WayPoint *next_way;

        draw_path(screen, cr, &_route, COLORABLE_ROUTE);
        next_way = path_get_next_way();

        /* Now, draw the next waypoint on top of all other waypoints. */
        if (next_way)
        {
            gint x1, y1;
            point_to_pixels(screen->priv, next_way->point, &x1, &y1);
            draw_break(cr, &_color[COLORABLE_ROUTE_BREAK], x1, y1);
        }
    }

    if (_show_paths & TRACKS_MASK)
        draw_path(screen, cr, &_track, COLORABLE_TRACK);
}

static gboolean
overlay_redraw_real(MapScreen *screen)
{
    MapScreenPrivate *priv;
    GtkAllocation *allocation;
    gfloat center_x, center_y;
    cairo_t *cr;

    g_return_val_if_fail (MAP_IS_SCREEN (screen), FALSE);
    priv = screen->priv;

    clutter_cairo_texture_clear(CLUTTER_CAIRO_TEXTURE(priv->overlay));
    allocation = &(GTK_WIDGET(screen)->allocation);
    clutter_actor_get_anchor_point(priv->map, &center_x, &center_y);
    clutter_actor_set_position(priv->overlay, center_x, center_y);

    priv->overlay_start_px = center_x - priv->overlay_center_px;
    priv->overlay_start_py = center_y - priv->overlay_center_px;

    cr = clutter_cairo_texture_create(CLUTTER_CAIRO_TEXTURE(priv->overlay));
    g_assert(cr != NULL);

    draw_paths(screen, cr);

    cairo_destroy(cr);

    priv->source_overlay_redraw = 0;
    return FALSE;
}

static void
overlay_redraw_idle(MapScreen *screen)
{
    MapScreenPrivate *priv = screen->priv;

    if (priv->source_overlay_redraw == 0 && GTK_WIDGET_REALIZED(screen))
    {
        priv->source_overlay_redraw =
            g_idle_add((GSourceFunc)overlay_redraw_real, screen);
    }
}

static void
load_tiles_into_map(MapScreen *screen, RepoData *repo, gint zoom,
                    gint tx1, gint ty1, gint tx2, gint ty2)
{
    ClutterContainer *tile_group;
    ClutterActor *tile;
    gint tx, ty;

    tile_group = CLUTTER_CONTAINER(screen->priv->tile_group);

    /* hide all the existing tiles */
    clutter_container_foreach(tile_group,
                              (ClutterCallback)clutter_actor_hide, NULL);

    clutter_actor_set_position(screen->priv->tile_group,
                               tx1 * TILE_SIZE_PIXELS,
                               ty1 * TILE_SIZE_PIXELS);

    for (tx = tx1; tx <= tx2; tx++)
    {
        for (ty = ty1; ty <= ty2; ty++)
        {
            tile = map_tile_cached(repo, zoom, tx, ty);
            if (!tile)
            {
                gboolean new_tile;
                tile = map_tile_load(repo, zoom, tx, ty, &new_tile);
                if (new_tile)
                    clutter_container_add_actor(tile_group, tile);
            }

            clutter_actor_set_position(tile,
                                       (tx - tx1) * TILE_SIZE_PIXELS,
                                       (ty - ty1) * TILE_SIZE_PIXELS);
            clutter_actor_show(tile);
        }
    }
}

static void
create_compass(MapScreen *screen)
{
    MapScreenPrivate *priv = screen->priv;
    gint height, width;
    cairo_text_extents_t te;
    cairo_t *cr;

    width = 40, height = 75;
    priv->compass = clutter_cairo_texture_new(width, height);
    cr = clutter_cairo_texture_create(CLUTTER_CAIRO_TEXTURE(priv->compass));

    cairo_move_to(cr, 20, 57);
    cairo_line_to(cr, 40, 75);
    cairo_line_to(cr, 20, 0);
    cairo_line_to(cr, 0, 75);
    cairo_close_path(cr);
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_set_line_width(cr, 2);
    cairo_stroke(cr);
    cairo_destroy(cr);

    clutter_actor_set_anchor_point(priv->compass, 20, 45);

    /* create the "N" letter */
    width = 16, height = 16;
    priv->compass_north = clutter_cairo_texture_new(width, height);
    cr = clutter_cairo_texture_create
        (CLUTTER_CAIRO_TEXTURE(priv->compass_north));
    cairo_set_source_rgb (cr, 1, 1, 1);
    cairo_select_font_face (cr, "Sans Serif",
                            CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size (cr, 20);
    cairo_text_extents (cr, "N", &te);
    cairo_move_to (cr, 8 - te.width / 2 - te.x_bearing,
                       8 - te.height / 2 - te.y_bearing);
    cairo_show_text (cr, "N");
    cairo_destroy(cr);

    clutter_actor_set_anchor_point(priv->compass_north, 8, 8);
}

static void
compute_scale_text(gchar *buffer, gsize len, gint zoom)
{
    gfloat distance;
    gdouble lat1, lon1, lat2, lon2;

    unit2latlon(_center.unitx - pixel2zunit(SCALE_WIDTH / 2 - 4, zoom),
                _center.unity, lat1, lon1);
    unit2latlon(_center.unitx + pixel2zunit(SCALE_WIDTH / 2 - 4, zoom),
                _center.unity, lat2, lon2);
    distance = calculate_distance(lat1, lon1, lat2, lon2) *
        UNITS_CONVERT[_units];

    if(distance < 1.f)
        snprintf(buffer, len, "%0.2f %s", distance, UNITS_ENUM_TEXT[_units]);
    else if(distance < 10.f)
        snprintf(buffer, len, "%0.1f %s", distance, UNITS_ENUM_TEXT[_units]);
    else
        snprintf(buffer, len, "%0.f %s", distance, UNITS_ENUM_TEXT[_units]);
}

static void
update_scale_and_zoom(MapScreen *screen)
{
    MapScreenPrivate *priv = screen->priv;
    cairo_text_extents_t te;
    gchar buffer[16];
    cairo_t *cr;

    cr = clutter_cairo_texture_create(CLUTTER_CAIRO_TEXTURE(priv->scale));

    cairo_rectangle(cr, 0, 0, SCALE_WIDTH, SCALE_HEIGHT);
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_set_line_width(cr, 4);
    cairo_stroke(cr);

    /* text in scale box */
    compute_scale_text(buffer, sizeof(buffer), priv->zoom);
    cairo_set_source_rgb (cr, 1, 1, 1);
    cairo_select_font_face (cr, "Sans Serif",
                            CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size (cr, 16);
    cairo_text_extents (cr, buffer, &te);
    cairo_move_to (cr,
                   SCALE_WIDTH / 2  - te.width / 2 - te.x_bearing,
                   SCALE_HEIGHT / 2 - te.height / 2 - te.y_bearing);
    cairo_show_text (cr, buffer);

    /* arrows */
    cairo_set_line_width(cr, 2);
    cairo_move_to(cr, 4, SCALE_HEIGHT / 2 - 4);
    cairo_line_to(cr, 4, SCALE_HEIGHT / 2 + 4);
    cairo_move_to(cr, 4, SCALE_HEIGHT / 2);
    cairo_line_to(cr, (SCALE_WIDTH - te.width) / 2 - 4, SCALE_HEIGHT / 2);
    cairo_stroke(cr);
    cairo_move_to(cr, SCALE_WIDTH - 4, SCALE_HEIGHT / 2 - 4);
    cairo_line_to(cr, SCALE_WIDTH - 4, SCALE_HEIGHT / 2 + 4);
    cairo_move_to(cr, SCALE_WIDTH - 4, SCALE_HEIGHT / 2);
    cairo_line_to(cr, (SCALE_WIDTH + te.width) / 2 + 4, SCALE_HEIGHT / 2);
    cairo_stroke(cr);

    cairo_destroy(cr);

    /* zoom box */
    cr = clutter_cairo_texture_create(CLUTTER_CAIRO_TEXTURE(priv->zoom_box));

    cairo_rectangle(cr, 0, 0, ZOOM_WIDTH, ZOOM_HEIGHT);
    cairo_set_source_rgb(cr, 0, 0, 0);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_set_line_width(cr, 4);
    cairo_stroke(cr);

    snprintf(buffer, sizeof(buffer), "%d", priv->zoom);
    cairo_set_source_rgb (cr, 1, 1, 1);
    cairo_select_font_face (cr, "Sans Serif",
                            CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size (cr, 16);
    cairo_text_extents (cr, buffer, &te);
    cairo_move_to (cr,
                   ZOOM_WIDTH / 2  - te.width / 2 - te.x_bearing,
                   ZOOM_HEIGHT / 2 - te.height / 2 - te.y_bearing);
    cairo_show_text (cr, buffer);
    cairo_destroy(cr);
}

static void
create_scale_and_zoom(MapScreen *screen)
{
    MapScreenPrivate *priv = screen->priv;

    priv->scale = clutter_cairo_texture_new(SCALE_WIDTH, SCALE_HEIGHT);
    clutter_actor_set_anchor_point(priv->scale,
                                   SCALE_WIDTH / 2 + 1,
                                   SCALE_HEIGHT);

    priv->zoom_box = clutter_cairo_texture_new(ZOOM_WIDTH, ZOOM_HEIGHT);
    clutter_actor_set_anchor_point(priv->zoom_box,
                                   ZOOM_WIDTH - 1 + SCALE_WIDTH / 2,
                                   ZOOM_HEIGHT);
}

static void
update_mark(MapScreen *screen)
{
    MapScreenPrivate *priv = screen->priv;
    cairo_t *cr;
    Colorable color;
    gfloat x, y, sqrt_speed;

    clutter_actor_get_anchor_point(priv->mark, &x, &y);
    cr = clutter_cairo_texture_create(CLUTTER_CAIRO_TEXTURE(priv->mark));

    cairo_new_sub_path(cr);
    cairo_arc(cr, x, y, _draw_width, 0, 2 * M_PI);
    cairo_set_line_width(cr, _draw_width);
    color = (_gps_state == RCVR_FIXED) ? COLORABLE_MARK : COLORABLE_MARK_OLD;
    set_source_color(cr, &_color[color]);
    cairo_stroke(cr);

    /* draw the speed vector */
    sqrt_speed = VELVEC_SIZE_FACTOR * sqrtf(10 + _gps.speed);
    cairo_move_to(cr, x, y);
    cairo_line_to(cr, x, y - sqrt_speed);
    cairo_set_line_width(cr, _draw_width);
    cairo_set_line_cap(cr, CAIRO_LINE_JOIN_ROUND);
    color = (_gps_state == RCVR_FIXED) ?
        (_show_velvec ? COLORABLE_MARK_VELOCITY : COLORABLE_MARK) :
        COLORABLE_MARK_OLD;
    set_source_color(cr, &_color[color]);
    cairo_stroke(cr);

    cairo_destroy(cr);

    /* set position and angle */
    clutter_actor_set_position(priv->mark,
                               unit2zpixel(_pos.unitx, priv->zoom),
                               unit2zpixel(_pos.unity, priv->zoom));
    clutter_actor_set_rotation(priv->mark,
                               CLUTTER_Z_AXIS, _gps.heading, 0, 0, 0);
}

static void
create_mark(MapScreen *screen)
{
    MapScreenPrivate *priv = screen->priv;

    priv->mark = clutter_cairo_texture_new(MARK_WIDTH, MARK_HEIGHT);
    clutter_actor_set_anchor_point(priv->mark,
                                   MARK_WIDTH / 2,
                                   MARK_HEIGHT - MARK_WIDTH / 2);
}

static void
map_screen_size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
    MapScreenPrivate *priv = MAP_SCREEN_PRIV(widget);
    gint diagonal;

    GTK_WIDGET_CLASS(map_screen_parent_class)->size_allocate
        (widget, allocation);

    /* Position the map at the center of the widget */
    clutter_actor_set_position(priv->map,
                               allocation->width / 2,
                               allocation->height / 2);
    if (priv->compass)
    {
        gint x, y;

        x = allocation->width - 42;
        y = allocation->height - 42;
        clutter_actor_set_position(priv->compass, x, y);
        clutter_actor_set_position(priv->compass_north, x, y);
    }

    /* scale and zoom box */
    clutter_actor_set_position(priv->scale,
                               allocation->width / 2,
                               allocation->height);
    clutter_actor_set_position(priv->zoom_box,
                               allocation->width / 2,
                               allocation->height);

    /* resize the OSM */
    map_osm_set_screen_size(MAP_OSM(priv->osm),
                            allocation->width, allocation->height);

    /* Resize the map overlay */

    /* The overlayed texture used for drawing must be big enough to cover
     * the screen when the map rotates: so, it will be a square whose side
     * is as big as the diagonal of the widget */
    diagonal = gint_sqrt(allocation->width * allocation->width +
                         allocation->height * allocation->height);
    clutter_cairo_texture_set_surface_size
        (CLUTTER_CAIRO_TEXTURE(priv->overlay), diagonal, diagonal);
    priv->overlay_center_px = diagonal / 2;
    clutter_actor_set_anchor_point(priv->overlay,
                                   priv->overlay_center_px,
                                   priv->overlay_center_px);

    overlay_redraw_idle((MapScreen *)widget);

    /* message box */
    if (priv->message_box)
        clutter_actor_set_size(priv->message_box,
                               allocation->width - 2 * HILDON_MARGIN_DOUBLE,
                               allocation->height - 2 * HILDON_MARGIN_DOUBLE);
}

static void
map_screen_dispose(GObject *object)
{
    MapScreenPrivate *priv = MAP_SCREEN_PRIV(object);

    if (priv->is_disposed)
	return;

    priv->is_disposed = TRUE;

    if (priv->source_overlay_redraw != 0)
    {
        g_source_remove (priv->source_overlay_redraw);
        priv->source_overlay_redraw = 0;
    }

    G_OBJECT_CLASS(map_screen_parent_class)->dispose(object);
}

static void
map_screen_init(MapScreen *screen)
{
    MapScreenPrivate *priv;
    ClutterActor *stage;
    ClutterBackend *backend;

    priv = G_TYPE_INSTANCE_GET_PRIVATE(screen, MAP_TYPE_SCREEN,
                                       MapScreenPrivate);
    screen->priv = priv;

    backend = clutter_get_default_backend();
    clutter_backend_set_double_click_distance(backend, TOUCH_RADIUS);

    stage = gtk_clutter_embed_get_stage(GTK_CLUTTER_EMBED(screen));
    g_return_if_fail(stage != NULL);
    g_signal_connect(stage, "motion-event",
                     G_CALLBACK(on_pointer_event), screen);
    g_signal_connect(stage, "button-press-event",
                     G_CALLBACK(on_pointer_event), screen);
    g_signal_connect(stage, "button-release-event",
                     G_CALLBACK(on_pointer_event), screen);
    priv->btn_press_screen_x = -1;

    priv->map = clutter_group_new();
    g_return_if_fail(priv->map != NULL);
    clutter_container_add_actor(CLUTTER_CONTAINER(stage), priv->map);
    clutter_actor_show(priv->map);

    priv->tile_group = clutter_group_new();
    g_return_if_fail(priv->tile_group != NULL);
    clutter_container_add_actor(CLUTTER_CONTAINER(priv->map), priv->tile_group);
    clutter_actor_show(priv->tile_group);

    priv->poi_group = clutter_group_new();
    g_return_if_fail(priv->poi_group != NULL);
    clutter_container_add_actor(CLUTTER_CONTAINER(priv->map), priv->poi_group);
    clutter_actor_show(priv->poi_group);

    create_compass(screen);
    clutter_container_add_actor(CLUTTER_CONTAINER(stage), priv->compass);
    clutter_container_add_actor(CLUTTER_CONTAINER(stage), priv->compass_north);

    priv->overlay = clutter_cairo_texture_new(0, 0);
    clutter_container_add_actor(CLUTTER_CONTAINER(priv->map), priv->overlay);
    clutter_actor_show(priv->overlay);

    create_scale_and_zoom(screen);
    clutter_container_add_actor(CLUTTER_CONTAINER(stage), priv->scale);
    clutter_container_add_actor(CLUTTER_CONTAINER(stage), priv->zoom_box);

    create_mark(screen);
    clutter_container_add_actor(CLUTTER_CONTAINER(priv->map), priv->mark);

    /* On-screen Menu */
    priv->osm = g_object_new(MAP_TYPE_OSM,
                             "widget", screen,
                             NULL);
    clutter_container_add_actor(CLUTTER_CONTAINER(stage), priv->osm);
}

static void
map_screen_class_init(MapScreenClass * klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    g_type_class_add_private (object_class, sizeof (MapScreenPrivate));

    object_class->dispose = map_screen_dispose;

    widget_class->size_allocate = map_screen_size_allocate;
}

/**
 * map_screen_set_center:
 * @screen: the #MapScreen.
 * @x: the x coordinate of the new center.
 * @y: the y coordinate of the new center.
 * @zoom: the new zoom level, or -1 to keep the current one.
 */
void
map_screen_set_center(MapScreen *screen, gint x, gint y, gint zoom)
{
    MapScreenPrivate *priv;
    GtkAllocation *allocation;
    gint diag_halflength_units;
    gint start_tilex, start_tiley, stop_tilex, stop_tiley;
    gint px, py;
    gint cache_amount;
    gint new_zoom;
    RepoData *repo = _curr_repo;

    g_return_if_fail(MAP_IS_SCREEN(screen));
    priv = screen->priv;

    g_debug("%s, zoom %d", G_STRFUNC, zoom);
    allocation = &(GTK_WIDGET(screen)->allocation);
    clutter_actor_set_position(priv->map,
                               allocation->width / 2,
                               allocation->height / 2);

    new_zoom = (zoom > 0) ? zoom : priv->zoom;

    /* Calculate cache amount */
    if(repo->type != REPOTYPE_NONE && MAPDB_EXISTS(repo))
        cache_amount = _auto_download_precache;
    else
        cache_amount = 1; /* No cache. */

    diag_halflength_units =
        pixel2zunit(TILE_HALFDIAG_PIXELS +
                    MAX(allocation->width, allocation->height) / 2,
                    new_zoom);

    start_tilex = unit2ztile(x - diag_halflength_units + _map_correction_unitx,
                             new_zoom);
    start_tilex = MAX(start_tilex - (cache_amount - 1), 0);
    start_tiley = unit2ztile(y - diag_halflength_units + _map_correction_unity,
                             new_zoom);
    start_tiley = MAX(start_tiley - (cache_amount - 1), 0);
    stop_tilex = unit2ztile(x + diag_halflength_units + _map_correction_unitx,
                            new_zoom);
    stop_tilex = MIN(stop_tilex + (cache_amount - 1),
                     unit2ztile(WORLD_SIZE_UNITS, new_zoom));
    stop_tiley = unit2ztile(y + diag_halflength_units + _map_correction_unity,
                            new_zoom);
    stop_tiley = MIN(stop_tiley + (cache_amount - 1),
                     unit2ztile(WORLD_SIZE_UNITS, new_zoom));

    /* create the tiles */
    load_tiles_into_map(screen, repo, new_zoom,
                        start_tilex, start_tiley, stop_tilex, stop_tiley);

    /* Move the anchor point to the new center */
    px = unit2zpixel(x, new_zoom);
    py = unit2zpixel(y, new_zoom);
    clutter_actor_set_anchor_point(priv->map, px, py);

    /* if the zoom changed, update scale and zoom box */
    if (new_zoom != priv->zoom)
    {
        priv->zoom = new_zoom;
        update_scale_and_zoom(screen);
    }

    /* Update map data */
    priv->map_center_ux = x;
    priv->map_center_uy = y;

    /* draw the paths */
    overlay_redraw_idle(screen);
}

/**
 * map_screen_set_rotation:
 * @screen: the #MapScreen.
 * @angle: the new rotation angle.
 */
void
map_screen_set_rotation(MapScreen *screen, gint angle)
{
    MapScreenPrivate *priv;

    g_return_if_fail(MAP_IS_SCREEN(screen));
    priv = screen->priv;
    clutter_actor_set_rotation(priv->map,
                               CLUTTER_Z_AXIS, -angle, 0, 0, 0);

    if (priv->compass)
    {
        clutter_actor_set_rotation(priv->compass,
                                   CLUTTER_Z_AXIS, -angle, 0, 0, 0);
    }

    if (priv->poi_group)
        clutter_container_foreach(CLUTTER_CONTAINER(priv->poi_group),
                                  actor_set_rotation_cb,
                                  GINT_TO_POINTER(angle));
}

/**
 * map_screen_show_compass:
 * @screen: the #MapScreen.
 * @show: %TRUE if the compass should be shown.
 */
void
map_screen_show_compass(MapScreen *screen, gboolean show)
{
    MapScreenPrivate *priv;

    g_return_if_fail(MAP_IS_SCREEN(screen));
    priv = screen->priv;

    priv->show_compass = show;
    if (show)
    {
        clutter_actor_show(priv->compass);
        clutter_actor_show(priv->compass_north);
    }
    else
    {
        clutter_actor_hide(priv->compass);
        clutter_actor_hide(priv->compass_north);
    }
}

/**
 * map_screen_show_scale:
 * @screen: the #MapScreen.
 * @show: %TRUE if the scale should be shown.
 */
void
map_screen_show_scale(MapScreen *screen, gboolean show)
{
    MapScreenPrivate *priv;

    g_return_if_fail(MAP_IS_SCREEN(screen));
    priv = screen->priv;

    if (show)
        clutter_actor_show(priv->scale);
    else
        clutter_actor_hide(priv->scale);
}

/**
 * map_screen_show_message:
 * @screen: the #MapScreen.
 * @text: the message to be shown.
 *
 * Shows a message to the user.
 */
void
map_screen_show_message(MapScreen *screen, const gchar *text)
{
    MapScreenPrivate *priv = screen->priv;

    if (!priv->message_box)
    {
        ClutterActor *stage;
        GtkAllocation *allocation;

        priv->message_box = clutter_text_new();
        clutter_actor_set_position(priv->message_box,
                                   HILDON_MARGIN_DOUBLE,
                                   HILDON_MARGIN_DOUBLE);
        allocation = &(GTK_WIDGET(screen)->allocation);
        clutter_actor_set_size(priv->message_box,
                               allocation->width - 2 * HILDON_MARGIN_DOUBLE,
                               allocation->height - 2 * HILDON_MARGIN_DOUBLE);
        clutter_text_set_font_name(CLUTTER_TEXT(priv->message_box),
                                   "Sans Serif 20");
        clutter_text_set_line_wrap(CLUTTER_TEXT(priv->message_box), TRUE);
        clutter_text_set_line_alignment(CLUTTER_TEXT(priv->message_box),
                                        PANGO_ALIGN_CENTER);
        stage = gtk_clutter_embed_get_stage(GTK_CLUTTER_EMBED(screen));
        clutter_container_add_actor(CLUTTER_CONTAINER(stage),
                                    priv->message_box);
    }
    clutter_text_set_text(CLUTTER_TEXT(priv->message_box), text);
    clutter_actor_show(priv->message_box);
}

/**
 * map_screen_show_zoom_box:
 * @screen: the #MapScreen.
 * @show: %TRUE if the zoom box should be shown.
 */
void
map_screen_show_zoom_box(MapScreen *screen, gboolean show)
{
    MapScreenPrivate *priv;

    g_return_if_fail(MAP_IS_SCREEN(screen));
    priv = screen->priv;

    if (show)
        clutter_actor_show(priv->zoom_box);
    else
        clutter_actor_hide(priv->zoom_box);
}

/**
 * map_screen_update_mark:
 * @screen: the #MapScreen.
 */
void
map_screen_update_mark(MapScreen *screen)
{
    g_return_if_fail(MAP_IS_SCREEN(screen));
    update_mark(screen);
}

void
map_screen_zoom_in(MapScreen *screen)
{
    map_screen_change_zoom_by_step(screen, TRUE);
}

void
map_screen_zoom_out(MapScreen *screen)
{
    map_screen_change_zoom_by_step(screen, FALSE);
}

void
map_screen_action_point(MapScreen *screen)
{
    MapScreenPrivate *priv;
    ClutterActor *stage;

    g_return_if_fail(MAP_IS_SCREEN(screen));
    priv = screen->priv;

    clutter_actor_hide(priv->osm);
    priv->action_ongoing = TRUE;
    priv->allow_scrolling = TRUE;
    stage = gtk_clutter_embed_get_stage(GTK_CLUTTER_EMBED(screen));
    g_signal_connect(stage, "button-release-event",
                     G_CALLBACK(on_point_chosen), screen);
    map_screen_show_message(screen, _("Tap a point on the map"));
}

/**
 * map_screen_clear_pois:
 * @self: the #MapScreen.
 *
 * Removes all POIs from the map.
 */
void
map_screen_clear_pois(MapScreen *self)
{
    MapScreenPrivate *priv;

    g_return_if_fail(MAP_IS_SCREEN(self));
    priv = self->priv;

    clutter_group_remove_all(CLUTTER_GROUP(priv->poi_group));
}

/**
 * map_screen_show_poi:
 * @self: the #MapScreen.
 * @x: X coordinate, in units.
 * @y: Y coordinate, in units.
 * @pixbuf: #GdkPixbuf to show, or %NULL if standard icon.
 *
 * Show a POI in the map.
 */
void
map_screen_show_poi(MapScreen *self, gint x, gint y, GdkPixbuf *pixbuf)
{
    MapScreenPrivate *priv;
    ClutterActor *poi;
    gint angle;

    g_return_if_fail(MAP_IS_SCREEN(self));
    priv = self->priv;

    if (pixbuf)
        poi = gtk_clutter_texture_new_from_pixbuf(pixbuf);
    else
    {
        ClutterColor color;

        color.red = _color[COLORABLE_POI].red >> 8;
        color.green = _color[COLORABLE_POI].green >> 8;
        color.blue = _color[COLORABLE_POI].blue >> 8;
        color.alpha = 255;
        poi = clutter_rectangle_new_with_color(&color);
        clutter_actor_set_size(poi, 3 * _draw_width, 3 * _draw_width);
    }

    angle = clutter_actor_get_rotation(priv->map, CLUTTER_Z_AXIS,
                                       NULL, NULL, NULL);
    clutter_actor_set_rotation(poi, CLUTTER_Z_AXIS, -angle, 0, 0, 0);
    clutter_actor_set_anchor_point_from_gravity(poi, CLUTTER_GRAVITY_CENTER);
    x = unit2zpixel(x, priv->zoom);
    y = unit2zpixel(y, priv->zoom);
    clutter_actor_set_position(poi, x, y);
    clutter_container_add_actor(CLUTTER_CONTAINER(priv->poi_group), poi);
}

void
map_screen_get_tap_area_from_units(MapScreen *self, gint ux, gint uy,
                                   MapArea *area)
{
    gint radius;

    g_return_if_fail(MAP_IS_SCREEN(self));

    radius = pixel2zunit(TOUCH_RADIUS, self->priv->zoom);
    area->x1 = ux - radius;
    area->y1 = uy - radius;
    area->y2 = uy + radius;
    area->x2 = ux + radius;
}

void
map_screen_redraw_overlays(MapScreen *self)
{
    g_return_if_fail(MAP_IS_SCREEN(self));
    overlay_redraw_idle(self);
}

