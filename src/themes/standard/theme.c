/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2006-2007 Christian Hammond <chipx86@chipx86.com>
 * Copyright (C) 2009 Red Hat, Inc.
 * Copyright (C) 2011 Perberos <perberos@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */
#include "config.h"

#include <glib/gi18n.h>
#include <ctk/ctk.h>

#include <libxml/xpath.h>

typedef void (*ActionInvokedCb) (CtkWindow* nw, const char* key);
typedef void (*UrlClickedCb) (CtkWindow* nw, const char* url);

typedef struct {
	CtkWidget* win;
	CtkWidget* top_spacer;
	CtkWidget* bottom_spacer;
	CtkWidget* main_hbox;
	CtkWidget* iconbox;
	CtkWidget* icon;
	CtkWidget* content_hbox;
	CtkWidget* summary_label;
	CtkWidget* close_button;
	CtkWidget* body_label;
	CtkWidget* actions_box;
	CtkWidget* last_sep;
	CtkWidget* stripe_spacer;
	CtkWidget* pie_countdown;

	gboolean has_arrow;
	gboolean composited;
	gboolean action_icons;

	int point_x;
	int point_y;

	int drawn_arrow_begin_x;
	int drawn_arrow_begin_y;
	int drawn_arrow_middle_x;
	int drawn_arrow_middle_y;
	int drawn_arrow_end_x;
	int drawn_arrow_end_y;

	int width;
	int height;

	CdkPoint* border_points;
	size_t num_border_points;

	cairo_region_t *window_region;

	guchar urgency;
	glong timeout;
	glong remaining;

	UrlClickedCb url_clicked;

} WindowData;

enum {
	URGENCY_LOW,
	URGENCY_NORMAL,
	URGENCY_CRITICAL
};

gboolean theme_check_init(unsigned int major_ver, unsigned int minor_ver,
			  unsigned int micro_ver);
void get_theme_info(char **theme_name, char **theme_ver, char **author,
		    char **homepage);
CtkWindow* create_notification(UrlClickedCb url_clicked);
void set_notification_text(CtkWindow *nw, const char *summary,
			   const char *body);
void set_notification_icon(CtkWindow *nw, GdkPixbuf *pixbuf);
void set_notification_arrow(CtkWidget *nw, gboolean visible, int x, int y);
void add_notification_action(CtkWindow *nw, const char *text, const char *key,
			     ActionInvokedCb cb);
void clear_notification_actions(CtkWindow *nw);
void move_notification(CtkWidget *nw, int x, int y);
void set_notification_timeout(CtkWindow *nw, glong timeout);
void set_notification_hints(CtkWindow *nw, GVariant *hints);
void notification_tick(CtkWindow *nw, glong remaining);

//#define ENABLE_GRADIENT_LOOK

#ifdef ENABLE_GRADIENT_LOOK
	#define STRIPE_WIDTH 45
#else
	#define STRIPE_WIDTH 30
#endif

#define WIDTH         400
#define IMAGE_SIZE    32
#define IMAGE_PADDING 10
#define SPACER_LEFT   30
#define PIE_RADIUS    12
#define PIE_WIDTH     (2 * PIE_RADIUS)
#define PIE_HEIGHT    (2 * PIE_RADIUS)
#define BODY_X_OFFSET (IMAGE_SIZE + 8)
#define DEFAULT_ARROW_OFFSET  (SPACER_LEFT + 2)
#define DEFAULT_ARROW_HEIGHT  14
#define DEFAULT_ARROW_WIDTH   28
#define BACKGROUND_OPACITY    0.92
#define BOTTOM_GRADIENT_HEIGHT 30

static void
get_background_color (CtkStyleContext *context,
                      CtkStateFlags    state,
                      CdkRGBA         *color)
{
        CdkRGBA *c;

        g_return_if_fail (color != NULL);
        g_return_if_fail (CTK_IS_STYLE_CONTEXT (context));

        ctk_style_context_get (context, state,
                               "background-color", &c,
                               NULL);

        *color = *c;
        cdk_rgba_free (c);
}

static void fill_background(CtkWidget* widget, WindowData* windata, cairo_t* cr)
{
    CtkStyleContext *context;
    CdkRGBA bg;

    CtkAllocation allocation;

    ctk_widget_get_allocation(widget, &allocation);

    #ifdef ENABLE_GRADIENT_LOOK

        cairo_pattern_t *gradient;
        int              gradient_y;

        gradient_y = allocation.height - BOTTOM_GRADIENT_HEIGHT;

    #endif

    context = ctk_widget_get_style_context (windata->win);

    ctk_style_context_save (context);
    ctk_style_context_set_state (context, CTK_STATE_FLAG_NORMAL);

    get_background_color (context, CTK_STATE_FLAG_NORMAL, &bg);

    ctk_style_context_restore (context);

    if (windata->composited)
    {
        cairo_set_source_rgba(cr, bg.red, bg.green, bg.blue, BACKGROUND_OPACITY);
    }
    else
    {
        cdk_cairo_set_source_rgba (cr, &bg);
    }

    cairo_rectangle(cr, 0, 0, allocation.width, allocation.height);

    cairo_fill(cr);

    #ifdef ENABLE_GRADIENT_LOOK
        /* Add a very subtle gradient to the bottom of the notification */
        gradient = cairo_pattern_create_linear(0, gradient_y, 0, allocation.height);
        cairo_pattern_add_color_stop_rgba(gradient, 0, 0, 0, 0, 0);
        cairo_pattern_add_color_stop_rgba(gradient, 1, 0, 0, 0, 0.15);
        cairo_rectangle(cr, 0, gradient_y, allocation.width, BOTTOM_GRADIENT_HEIGHT);

        cairo_set_source(cr, gradient);
        cairo_fill(cr);
        cairo_pattern_destroy(gradient);
    #endif
}

static void draw_stripe(CtkWidget* widget, WindowData* windata, cairo_t* cr)
{
	CtkStyleContext* context;
	CdkRGBA bg;
	int              stripe_x;
	int              stripe_y;
	int              stripe_height;
	#ifdef ENABLE_GRADIENT_LOOK
		cairo_pattern_t* gradient;
		double           r, g, b;
	#endif

	context = ctk_widget_get_style_context (widget);

	ctk_style_context_save (context);

	CtkAllocation alloc;
	ctk_widget_get_allocation(windata->main_hbox, &alloc);

	stripe_x = alloc.x + 1;

	if (ctk_widget_get_direction(widget) == CTK_TEXT_DIR_RTL)
	{
		stripe_x = windata->width - STRIPE_WIDTH - stripe_x;
	}

	stripe_y = alloc.y + 1;
	stripe_height = alloc.height - 2;

	switch (windata->urgency)
	{
		case URGENCY_LOW: // LOW
			ctk_style_context_set_state (context, CTK_STATE_FLAG_NORMAL);
			ctk_style_context_add_class (context, CTK_STYLE_CLASS_VIEW);
			get_background_color (context, CTK_STATE_FLAG_NORMAL, &bg);
			cdk_cairo_set_source_rgba (cr, &bg);
			break;

		case URGENCY_CRITICAL: // CRITICAL
			cdk_rgba_parse (&bg, "#CC0000");
			break;

		case URGENCY_NORMAL: // NORMAL
		default:
			ctk_style_context_set_state (context, CTK_STATE_FLAG_SELECTED);
			ctk_style_context_add_class (context, CTK_STYLE_CLASS_VIEW);
			get_background_color (context, CTK_STATE_FLAG_SELECTED, &bg);
			cdk_cairo_set_source_rgba (cr, &bg);
			break;
	}

	ctk_style_context_restore (context);

	cairo_rectangle(cr, stripe_x, stripe_y, STRIPE_WIDTH, stripe_height);

	#ifdef ENABLE_GRADIENT_LOOK
		r = color.red / 65535.0;
		g = color.green / 65535.0;
		b = color.blue / 65535.0;

		gradient = cairo_pattern_create_linear(stripe_x, 0, STRIPE_WIDTH, 0);
		cairo_pattern_add_color_stop_rgba(gradient, 0, r, g, b, 1);
		cairo_pattern_add_color_stop_rgba(gradient, 1, r, g, b, 0);
		cairo_set_source(cr, gradient);
		cairo_fill(cr);
		cairo_pattern_destroy(gradient);
	#else
		cdk_cairo_set_source_rgba (cr, &bg);
		cairo_fill(cr);
	#endif
}

static CtkArrowType get_notification_arrow_type(CtkWidget* nw)
{
	WindowData*     windata;
	CdkScreen*      screen;
	CdkRectangle    monitor_geometry;
	CdkDisplay*     display;
	CdkMonitor*     monitor;

	windata = g_object_get_data(G_OBJECT(nw), "windata");

	screen = cdk_window_get_screen(CDK_WINDOW( ctk_widget_get_window(nw)));
	display = cdk_screen_get_display (screen);
	monitor = cdk_display_get_monitor_at_point (display, windata->point_x, windata->point_y);
	cdk_monitor_get_geometry (monitor, &monitor_geometry);

	if (windata->point_y - monitor_geometry.y + windata->height + DEFAULT_ARROW_HEIGHT > monitor_geometry.height)
	{
		return CTK_ARROW_DOWN;
	}
	else
	{
		return CTK_ARROW_UP;
	}
}

#define ADD_POINT(_x, _y, shapeoffset_x, shapeoffset_y) \
	G_STMT_START { \
		windata->border_points[i].x = (_x); \
		windata->border_points[i].y = (_y); \
		shape_points[i].x = (_x) + (shapeoffset_x); \
		shape_points[i].y = (_y) + (shapeoffset_y); \
		i++;\
	} G_STMT_END

static void create_border_with_arrow(CtkWidget* nw, WindowData* windata)
{
	int             width;
	int             height;
	int             y;
	int             norm_point_x;
	int             norm_point_y;
	CtkArrowType    arrow_type;
	CdkScreen*      screen;
	int             arrow_side1_width = DEFAULT_ARROW_WIDTH / 2;
	int             arrow_side2_width = DEFAULT_ARROW_WIDTH / 2;
	int             arrow_offset = DEFAULT_ARROW_OFFSET;
	CdkPoint*       shape_points = NULL;
	int             i = 0;
	CdkMonitor*     monitor;
	CdkDisplay*     display;
	CdkRectangle    monitor_geometry;

	width = windata->width;
	height = windata->height;

	screen = cdk_window_get_screen(CDK_WINDOW(ctk_widget_get_window(nw)));
	display = cdk_screen_get_display (screen);
	monitor = cdk_display_get_monitor_at_point (display, windata->point_x, windata->point_y);
	cdk_monitor_get_geometry (monitor, &monitor_geometry);

	windata->num_border_points = 5;

	arrow_type = get_notification_arrow_type(windata->win);

	norm_point_x = windata->point_x - monitor_geometry.x;
	norm_point_y = windata->point_y - monitor_geometry.y;

	/* Handle the offset and such */
	switch (arrow_type)
	{
		case CTK_ARROW_UP:
		case CTK_ARROW_DOWN:

			if (norm_point_x < arrow_side1_width)
			{
				arrow_side1_width = 0;
				arrow_offset = 0;
			}
			else if (norm_point_x > monitor_geometry.width - arrow_side2_width)
			{
				arrow_side2_width = 0;
				arrow_offset = width - arrow_side1_width;
			}
			else
			{
				if (norm_point_x - arrow_side2_width + width >= monitor_geometry.width)
				{
					arrow_offset = width - monitor_geometry.width + norm_point_x;
				}
				else
				{
					arrow_offset = MIN(norm_point_x - arrow_side1_width, DEFAULT_ARROW_OFFSET);
				}

				if (arrow_offset == 0 || arrow_offset == width - arrow_side1_width)
				{
					windata->num_border_points++;
				}
				else
				{
					windata->num_border_points += 2;
				}
			}

			/*
			 * Why risk this for official builds? If it's somehow off the
			 * screen, it won't horribly impact the user. Definitely less
			 * than an assertion would...
			 */
			#if 0
				g_assert(arrow_offset + arrow_side1_width >= 0);
				g_assert(arrow_offset + arrow_side1_width + arrow_side2_width <= width);
			#endif

			windata->border_points = g_new0(CdkPoint, windata->num_border_points);
			shape_points = g_new0(CdkPoint, windata->num_border_points);

			windata->drawn_arrow_begin_x = arrow_offset;
			windata->drawn_arrow_middle_x = arrow_offset + arrow_side1_width;
			windata->drawn_arrow_end_x = arrow_offset + arrow_side1_width + arrow_side2_width;

			if (arrow_type == CTK_ARROW_UP)
			{
				windata->drawn_arrow_begin_y = DEFAULT_ARROW_HEIGHT;
				windata->drawn_arrow_middle_y = 0;
				windata->drawn_arrow_end_y = DEFAULT_ARROW_HEIGHT;

				if (arrow_side1_width == 0)
				{
					ADD_POINT(0, 0, 0, 0);
				}
				else
				{
					ADD_POINT(0, DEFAULT_ARROW_HEIGHT, 0, 0);

					if (arrow_offset > 0)
					{
						ADD_POINT(arrow_offset - (arrow_side2_width > 0 ? 0 : 1), DEFAULT_ARROW_HEIGHT, 0, 0);
					}

					ADD_POINT(arrow_offset + arrow_side1_width - (arrow_side2_width > 0 ? 0 : 1), 0, 0, 0);
				}

				if (arrow_side2_width > 0)
				{
					ADD_POINT(windata->drawn_arrow_end_x, windata->drawn_arrow_end_y, 1, 0);
					ADD_POINT(width - 1, DEFAULT_ARROW_HEIGHT, 1, 0);
				}

				ADD_POINT(width - 1, height - 1, 1, 1);
				ADD_POINT(0, height - 1, 0, 1);

				y = windata->point_y;
			}
			else
			{
				windata->drawn_arrow_begin_y = height - DEFAULT_ARROW_HEIGHT;
				windata->drawn_arrow_middle_y = height;
				windata->drawn_arrow_end_y = height - DEFAULT_ARROW_HEIGHT;

				ADD_POINT(0, 0, 0, 0);
				ADD_POINT(width - 1, 0, 1, 0);

				if (arrow_side2_width == 0)
				{
					ADD_POINT(width - 1, height, (arrow_side1_width > 0 ? 0 : 1), 0);
				}
				else
				{
					ADD_POINT(width - 1, height - DEFAULT_ARROW_HEIGHT, 1, 1);

					if (arrow_offset < width - arrow_side1_width)
					{
						ADD_POINT(arrow_offset + arrow_side1_width + arrow_side2_width, height - DEFAULT_ARROW_HEIGHT, 0, 1);
					}

					ADD_POINT(arrow_offset + arrow_side1_width, height, 0, 1);
				}

				if (arrow_side1_width > 0)
				{
					ADD_POINT(windata->drawn_arrow_begin_x - (arrow_side2_width > 0 ? 0 : 1), windata->drawn_arrow_begin_y, 0, 0);
					ADD_POINT(0, height - DEFAULT_ARROW_HEIGHT, 0, 1);
				}

				y = windata->point_y - height;
			}

			#if 0
				g_assert(i == windata->num_border_points);
				g_assert(windata->point_x - arrow_offset - arrow_side1_width >= 0);
			#endif

			ctk_window_move(CTK_WINDOW(windata->win), windata->point_x - arrow_offset - arrow_side1_width, y);

			break;

		case CTK_ARROW_LEFT:
		case CTK_ARROW_RIGHT:

			if (norm_point_y < arrow_side1_width)
			{
				arrow_side1_width = 0;
				arrow_offset = norm_point_y;
			}
			else if (norm_point_y > monitor_geometry.height - arrow_side2_width)
			{
				arrow_side2_width = 0;
				arrow_offset = norm_point_y - arrow_side1_width;
			}
			break;

		default:
			g_assert_not_reached();
	}

	g_assert(shape_points != NULL);

	/* FIXME won't work with CTK+3, need a replacement */
	/*windata->window_region = cdk_region_polygon(shape_points, windata->num_border_points, CDK_EVEN_ODD_RULE);*/
	g_free(shape_points);
}

static void draw_border(CtkWidget* widget, WindowData *windata, cairo_t* cr)
{
	cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0);
	cairo_set_line_width(cr, 1.0);

	if (windata->has_arrow)
	{
		size_t i;

		create_border_with_arrow(windata->win, windata);

		cairo_move_to(cr, windata->border_points[0].x + 0.5, windata->border_points[0].y + 0.5);

		for (i = 1; i < windata->num_border_points; i++)
		{
				cairo_line_to(cr, windata->border_points[i].x + 0.5, windata->border_points[i].y + 0.5);
		}

		cairo_close_path(cr);
		/* FIXME window_region is not set up anyway, see previous fixme */
		/*cdk_window_shape_combine_region (ctk_widget_get_window (windata->win), windata->window_region, 0, 0);*/
		g_free(windata->border_points);
		windata->border_points = NULL;
	}
	else
	{
		cairo_rectangle(cr, 0.5, 0.5, windata->width - 0.5, windata->height - 0.5);
	}

	cairo_stroke(cr);
}

static void
paint_window (CtkWidget  *widget,
	      cairo_t    *cr,
	      WindowData *windata)
{
	cairo_t*         cr2;
	cairo_surface_t* surface;
	CtkAllocation    allocation;

	ctk_widget_get_allocation(windata->win, &allocation);

	if (windata->width == 0)
	{
			windata->width = allocation.width;
			windata->height = allocation.height;
	}

	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);

	ctk_widget_get_allocation(widget, &allocation);

	surface = cairo_surface_create_similar (cairo_get_target (cr),
						CAIRO_CONTENT_COLOR_ALPHA,
						allocation.width,
						allocation.height);

	cr2 = cairo_create (surface);

	fill_background(widget, windata, cr2);
	draw_border(widget, windata, cr2);
	draw_stripe(widget, windata, cr2);
	cairo_fill (cr2);
	cairo_destroy (cr2);

	cairo_set_source_surface (cr, surface, 0, 0);
	cairo_paint(cr);
	cairo_surface_destroy(surface);
}

static gboolean
on_draw (CtkWidget *widget, cairo_t *cr, WindowData *windata)
{
	paint_window (widget, cr, windata);

	return FALSE;
}

static void destroy_windata(WindowData* windata)
{
	if (windata->window_region != NULL)
	{
		cairo_region_destroy(windata->window_region);
	}

	g_free(windata);
}

static void update_spacers(CtkWidget* nw)
{
	WindowData* windata;

	windata = g_object_get_data(G_OBJECT(nw), "windata");

	if (windata->has_arrow)
	{
		switch (get_notification_arrow_type(CTK_WIDGET(nw)))
		{
			case CTK_ARROW_UP:
				ctk_widget_show(windata->top_spacer);
				ctk_widget_hide(windata->bottom_spacer);
				break;

			case CTK_ARROW_DOWN:
				ctk_widget_hide(windata->top_spacer);
				ctk_widget_show(windata->bottom_spacer);
				break;

			default:
				g_assert_not_reached();
		}
	}
	else
	{
		ctk_widget_hide(windata->top_spacer);
		ctk_widget_hide(windata->bottom_spacer);
	}
}

static void update_content_hbox_visibility(WindowData* windata)
{
	/*
	 * This is all a hack, but until we have a libview-style ContentBox,
	 * it'll just have to do.
	 */
	if (ctk_widget_get_visible(windata->icon) || ctk_widget_get_visible(windata->body_label) || ctk_widget_get_visible(windata->actions_box))
	{
		ctk_widget_show(windata->content_hbox);
	}
	else
	{
		ctk_widget_hide(windata->content_hbox);
	}
}

static gboolean configure_event_cb(CtkWidget* nw, CdkEventConfigure* event, WindowData* windata)
{
	windata->width = event->width;
	windata->height = event->height;

	update_spacers(nw);
	ctk_widget_queue_draw(nw);

	return FALSE;
}

static gboolean activate_link(CtkLabel* label, const char* url, WindowData* windata)
{
	windata->url_clicked(CTK_WINDOW(windata->win), url);

	return TRUE;
}

CtkWindow* create_notification(UrlClickedCb url_clicked)
{
	CtkWidget* spacer;
	CtkWidget* win;
	CtkWidget* main_vbox;
	CtkWidget* hbox;
	CtkWidget* vbox;
	CtkWidget* close_button;
	CtkWidget* image;
	AtkObject* atkobj;
	WindowData* windata;

	CdkVisual *visual;
	CdkScreen* screen;

	windata = g_new0(WindowData, 1);
	windata->urgency = URGENCY_NORMAL;
	windata->url_clicked = url_clicked;

	win = ctk_window_new(CTK_WINDOW_POPUP);
	ctk_window_set_resizable(CTK_WINDOW(win), FALSE);
	windata->win = win;

	windata->composited = FALSE;


	screen = ctk_window_get_screen(CTK_WINDOW(win));

	visual = cdk_screen_get_rgba_visual(screen);

	if (visual != NULL)
	{
		ctk_widget_set_visual(win, visual);

		if (cdk_screen_is_composited(screen))
		{
			windata->composited = TRUE;
		}
	}

	ctk_window_set_title(CTK_WINDOW(win), "Notification");
	ctk_window_set_type_hint(CTK_WINDOW(win), CDK_WINDOW_TYPE_HINT_NOTIFICATION);
	ctk_widget_add_events(win, CDK_BUTTON_PRESS_MASK | CDK_BUTTON_RELEASE_MASK);
	ctk_widget_realize(win);
	ctk_widget_set_size_request(win, WIDTH, -1);

	g_object_set_data_full(G_OBJECT(win), "windata", windata, (GDestroyNotify) destroy_windata);
	atk_object_set_role(ctk_widget_get_accessible(win), ATK_ROLE_ALERT);

	g_signal_connect(G_OBJECT(win), "configure_event", G_CALLBACK(configure_event_cb), windata);

	main_vbox = ctk_box_new(CTK_ORIENTATION_VERTICAL, 0);
	ctk_widget_show(main_vbox);
	ctk_container_add (CTK_CONTAINER (win), main_vbox);
	ctk_container_set_border_width(CTK_CONTAINER(main_vbox), 1);

	g_signal_connect (G_OBJECT (main_vbox), "draw", G_CALLBACK (on_draw), windata);

	windata->top_spacer = ctk_image_new();
	ctk_box_pack_start(CTK_BOX(main_vbox), windata->top_spacer, FALSE, FALSE, 0);
	ctk_widget_set_size_request(windata->top_spacer, -1, DEFAULT_ARROW_HEIGHT);

	windata->main_hbox = ctk_box_new(CTK_ORIENTATION_HORIZONTAL, 0);
	ctk_widget_show(windata->main_hbox);
	ctk_box_pack_start(CTK_BOX(main_vbox), windata->main_hbox, FALSE, FALSE, 0);

	windata->bottom_spacer = ctk_image_new();
	ctk_box_pack_start(CTK_BOX(main_vbox), windata->bottom_spacer, FALSE, FALSE, 0);
	ctk_widget_set_size_request(windata->bottom_spacer, -1, DEFAULT_ARROW_HEIGHT);

	vbox = ctk_box_new(CTK_ORIENTATION_VERTICAL, 6);
	ctk_widget_show(vbox);
	ctk_box_pack_start(CTK_BOX(windata->main_hbox), vbox, TRUE, TRUE, 0);
	ctk_container_set_border_width(CTK_CONTAINER(vbox), 10);

	hbox = ctk_box_new(CTK_ORIENTATION_HORIZONTAL, 6);
	ctk_widget_show(hbox);
	ctk_box_pack_start(CTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	spacer = ctk_image_new();
	ctk_widget_show(spacer);
	ctk_box_pack_start(CTK_BOX(hbox), spacer, FALSE, FALSE, 0);
	ctk_widget_set_size_request(spacer, SPACER_LEFT, -1);

	windata->summary_label = ctk_label_new(NULL);
	ctk_widget_show(windata->summary_label);
	ctk_box_pack_start(CTK_BOX(hbox), windata->summary_label, TRUE, TRUE, 0);
	ctk_label_set_xalign (CTK_LABEL (windata->summary_label), 0.0);
	ctk_label_set_yalign (CTK_LABEL (windata->summary_label), 0.0);
	ctk_label_set_line_wrap(CTK_LABEL(windata->summary_label), TRUE);
	ctk_label_set_line_wrap_mode (CTK_LABEL (windata->summary_label), PANGO_WRAP_WORD_CHAR);

	atkobj = ctk_widget_get_accessible(windata->summary_label);
	atk_object_set_description (atkobj, _("Notification summary text."));

	/* Add the close button */
	close_button = ctk_button_new();
	windata->close_button = close_button;
	ctk_widget_set_halign (close_button, CTK_ALIGN_END);
	ctk_widget_set_valign (close_button, CTK_ALIGN_START);
	ctk_widget_show(close_button);
	ctk_box_pack_start(CTK_BOX(hbox), close_button, FALSE, FALSE, 0);
	ctk_button_set_relief(CTK_BUTTON(close_button), CTK_RELIEF_NONE);
	ctk_container_set_border_width(CTK_CONTAINER(close_button), 0);
	//ctk_widget_set_size_request(close_button, 20, 20);
	g_signal_connect_swapped(G_OBJECT(close_button), "clicked", G_CALLBACK(ctk_widget_destroy), win);

	atkobj = ctk_widget_get_accessible(close_button);
	atk_action_set_description(ATK_ACTION(atkobj), 0,
                               _("Closes the notification."));
	atk_object_set_name(atkobj, "");
	atk_object_set_description (atkobj, _("Closes the notification."));

	image = ctk_image_new_from_icon_name ("window-close", CTK_ICON_SIZE_MENU);
	ctk_widget_show(image);
	ctk_container_add(CTK_CONTAINER(close_button), image);

	windata->content_hbox = ctk_box_new(CTK_ORIENTATION_HORIZONTAL, 6);
	ctk_box_pack_start(CTK_BOX(vbox), windata->content_hbox, FALSE, FALSE, 0);

	windata->iconbox = ctk_box_new(CTK_ORIENTATION_HORIZONTAL, 0);
	ctk_widget_show(windata->iconbox);
	ctk_box_pack_start(CTK_BOX(windata->content_hbox), windata->iconbox, FALSE, FALSE, 0);
	ctk_widget_set_size_request(windata->iconbox, BODY_X_OFFSET, -1);

	windata->icon = ctk_image_new();
	ctk_box_pack_start(CTK_BOX(windata->iconbox), windata->icon, TRUE, TRUE, 0);
	ctk_widget_set_halign (windata->icon, CTK_ALIGN_CENTER);
	ctk_widget_set_valign (windata->icon, CTK_ALIGN_START);

	vbox = ctk_box_new(CTK_ORIENTATION_VERTICAL, 6);
	ctk_widget_show(vbox);
	ctk_box_pack_start(CTK_BOX(windata->content_hbox), vbox, TRUE, TRUE, 0);

	windata->body_label = ctk_label_new(NULL);
	ctk_widget_show(windata->body_label);
	ctk_box_pack_start(CTK_BOX(vbox), windata->body_label, TRUE, TRUE, 0);
	ctk_label_set_xalign (CTK_LABEL (windata->body_label), 0.0);
	ctk_label_set_yalign (CTK_LABEL (windata->body_label), 0.0);
	ctk_label_set_line_wrap(CTK_LABEL(windata->body_label), TRUE);
	ctk_label_set_line_wrap_mode (CTK_LABEL (windata->body_label), PANGO_WRAP_WORD_CHAR);
    ctk_label_set_max_width_chars (CTK_LABEL (windata->body_label), 50);
	g_signal_connect(G_OBJECT(windata->body_label), "activate-link", G_CALLBACK(activate_link), windata);

	atkobj = ctk_widget_get_accessible(windata->body_label);
	atk_object_set_description (atkobj, _("Notification body text."));

	windata->actions_box = ctk_box_new(CTK_ORIENTATION_HORIZONTAL, 6);
	ctk_widget_set_halign (windata->actions_box, CTK_ALIGN_END);
	ctk_widget_show(windata->actions_box);
	ctk_box_pack_start(CTK_BOX(vbox), windata->actions_box, FALSE, TRUE, 0);

	return CTK_WINDOW(win);
}

void set_notification_hints(CtkWindow *nw, GVariant *hints)
{
	WindowData *windata = g_object_get_data(G_OBJECT(nw), "windata");
	guint8 urgency;
	gboolean action_icons;

	g_assert(windata != NULL);

	if (g_variant_lookup(hints, "urgency", "y", &urgency))
	{
		windata->urgency = urgency;

		if (windata->urgency == URGENCY_CRITICAL) {
			ctk_window_set_title(CTK_WINDOW(nw), "Critical Notification");
		} else {
			ctk_window_set_title(CTK_WINDOW(nw), "Notification");
		}
	}

	/* Determine if action-icons have been requested */
	if (g_variant_lookup(hints, "action-icons", "b", &action_icons))
	{
		windata->action_icons = action_icons;
	}
}

void set_notification_timeout(CtkWindow* nw, glong timeout)
{
	WindowData* windata = g_object_get_data(G_OBJECT(nw), "windata");

	g_assert(windata != NULL);

	windata->timeout = timeout;
}

void notification_tick(CtkWindow* nw, glong remaining)
{
	WindowData* windata = g_object_get_data(G_OBJECT(nw), "windata");

	windata->remaining = remaining;

	if (windata->pie_countdown != NULL)
	{
		ctk_widget_queue_draw_area(windata->pie_countdown, 0, 0, PIE_WIDTH, PIE_HEIGHT);
	}
}

void set_notification_text(CtkWindow* nw, const char* summary, const char* body)
{
	char* str;
	char* quoted;
	CtkRequisition req;
	WindowData* windata;

	windata = g_object_get_data(G_OBJECT(nw), "windata");
	g_assert(windata != NULL);

	quoted = g_markup_escape_text(summary, -1);
	str = g_strdup_printf("<b><big>%s</big></b>", quoted);
	g_free(quoted);

	ctk_label_set_markup(CTK_LABEL(windata->summary_label), str);
	g_free(str);

	/* body */
	xmlDocPtr doc;
	xmlInitParser();
	str = g_strconcat ("<markup>", body, "</markup>", NULL);
	/* parse notification body */
	doc = xmlReadMemory(str, strlen (str), "noname.xml", NULL, 0);
	g_free (str);
	if (doc != NULL) {
		xmlXPathContextPtr xpathCtx;
		xmlXPathObjectPtr  xpathObj;
		xmlNodeSetPtr      nodes;
		const char        *body_label_text;
		int i, size;

		/* filterout img nodes */
		xpathCtx = xmlXPathNewContext(doc);
		xpathObj = xmlXPathEvalExpression((unsigned char *)"//img", xpathCtx);
		nodes = xpathObj->nodesetval;
		size = (nodes) ? nodes->nodeNr : 0;
		for(i = size - 1; i >= 0; i--) {
			xmlUnlinkNode (nodes->nodeTab[i]);
			xmlFreeNode (nodes->nodeTab[i]);
		}

		/* write doc to string */
		xmlBufferPtr buf = xmlBufferCreate();
		(void) xmlNodeDump(buf, doc, xmlDocGetRootElement (doc), 0, 0);
		str = (char *)buf->content;
		ctk_label_set_markup (CTK_LABEL (windata->body_label), str);

		/* cleanup */
		xmlBufferFree (buf);
		xmlXPathFreeObject (xpathObj);
		xmlXPathFreeContext (xpathCtx);
		xmlFreeDoc (doc);

		/* Does it render properly? */
		body_label_text = ctk_label_get_text (CTK_LABEL (windata->body_label));
		if ((body_label_text == NULL) || (strlen (body_label_text) == 0)) {
			goto render_fail;
		}
		goto renrer_ok;
	}

render_fail:
	/* could not parse notification body */
	quoted = g_markup_escape_text(body, -1);
	ctk_label_set_markup (CTK_LABEL (windata->body_label), quoted);
	g_free (quoted);

renrer_ok:
	xmlCleanupParser ();

	if (body == NULL || *body == '\0')
		ctk_widget_hide(windata->body_label);
	else
		ctk_widget_show(windata->body_label);

	update_content_hbox_visibility(windata);

	if (body != NULL && *body != '\0')
	{
		ctk_widget_get_preferred_size (windata->iconbox, NULL, &req);
		/* -1: border width for
		 * -6: spacing for hbox */
		ctk_widget_set_size_request(windata->body_label, WIDTH - (1 * 2) - (10 * 2) - req.width - 6, -1);
	}

		ctk_widget_get_preferred_size (windata->close_button, NULL, &req);
	/* -1: main_vbox border width
	 * -10: vbox border width
	 * -6: spacing for hbox */
	ctk_widget_set_size_request(windata->summary_label, WIDTH - (1 * 2) - (10 * 2) - SPACER_LEFT - req.width - (6 * 2), -1);
}

void set_notification_icon(CtkWindow* nw, GdkPixbuf* pixbuf)
{
	WindowData* windata = g_object_get_data(G_OBJECT(nw), "windata");

	g_assert(windata != NULL);

	ctk_image_set_from_pixbuf(CTK_IMAGE(windata->icon), pixbuf);

	if (pixbuf != NULL)
	{
		int pixbuf_width = gdk_pixbuf_get_width(pixbuf);

		ctk_widget_show(windata->icon);
		ctk_widget_set_size_request(windata->iconbox, MAX(BODY_X_OFFSET, pixbuf_width), -1);
	}
	else
	{
		ctk_widget_hide(windata->icon);
		ctk_widget_set_size_request(windata->iconbox, BODY_X_OFFSET, -1);
	}

	update_content_hbox_visibility(windata);
}

void set_notification_arrow(CtkWidget* nw, gboolean visible, int x, int y)
{
	WindowData* windata = g_object_get_data(G_OBJECT(nw), "windata");

	g_assert(windata != NULL);

	windata->has_arrow = visible;
	windata->point_x = x;
	windata->point_y = y;

	update_spacers(nw);
}

static void
paint_countdown (CtkWidget  *pie,
                 cairo_t    *cr,
                 WindowData *windata)
{
    CtkStyleContext *context;
    CdkRGBA bg;
    CtkAllocation alloc;
    cairo_t* cr2;
    cairo_surface_t* surface;

    context = ctk_widget_get_style_context (windata->win);

    ctk_style_context_save (context);
    ctk_style_context_set_state (context, CTK_STATE_FLAG_SELECTED);

    get_background_color (context, CTK_STATE_FLAG_SELECTED, &bg);

    ctk_style_context_restore (context);

    ctk_widget_get_allocation(pie, &alloc);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    surface = cairo_surface_create_similar (cairo_get_target(cr),
                                            CAIRO_CONTENT_COLOR_ALPHA,
                                            alloc.width,
                                            alloc.height);

    cr2 = cairo_create (surface);

    fill_background (pie, windata, cr2);

    if (windata->timeout > 0)
    {
        gdouble pct = (gdouble) windata->remaining / (gdouble) windata->timeout;

        cdk_cairo_set_source_rgba (cr2, &bg);

        cairo_move_to (cr2, PIE_RADIUS, PIE_RADIUS);
        cairo_arc_negative (cr2, PIE_RADIUS, PIE_RADIUS, PIE_RADIUS, -G_PI_2, -(pct * G_PI * 2) - G_PI_2);
        cairo_line_to (cr2, PIE_RADIUS, PIE_RADIUS);
        cairo_fill (cr2);
    }

    cairo_destroy(cr2);

    cairo_save (cr);
    cairo_set_source_surface (cr, surface, 0, 0);
    cairo_paint (cr);
    cairo_restore (cr);

    cairo_surface_destroy(surface);
}

static gboolean
on_countdown_draw (CtkWidget *widget, cairo_t *cr, WindowData *windata)
{
	paint_countdown (widget, cr, windata);

	return FALSE;
}

static void action_clicked_cb(CtkWidget* w, CdkEventButton* event, ActionInvokedCb action_cb)
{
	CtkWindow* nw;
	const char* key;
	nw = g_object_get_data(G_OBJECT(w), "_nw");
	key = g_object_get_data(G_OBJECT(w), "_action_key");
	action_cb(nw, key);
}

void add_notification_action(CtkWindow* nw, const char* text, const char* key, ActionInvokedCb cb)
{
	WindowData* windata;
	CtkWidget* label;
	CtkWidget* button;
	CtkWidget* hbox;
	GdkPixbuf* pixbuf;
	char* buf;

	windata = g_object_get_data(G_OBJECT(nw), "windata");

	g_assert(windata != NULL);

	if (ctk_widget_get_visible(windata->actions_box))
	{
		ctk_widget_show(windata->actions_box);
		update_content_hbox_visibility(windata);

		/* Don't try to re-add a pie_countdown */
		if (!windata->pie_countdown) {
			windata->pie_countdown = ctk_drawing_area_new();
			ctk_widget_set_halign (windata->pie_countdown, CTK_ALIGN_END);
			ctk_widget_show(windata->pie_countdown);

			ctk_box_pack_end (CTK_BOX (windata->actions_box), windata->pie_countdown, FALSE, TRUE, 0);
			ctk_widget_set_size_request(windata->pie_countdown,
						    PIE_WIDTH, PIE_HEIGHT);
			g_signal_connect(G_OBJECT(windata->pie_countdown), "draw",
					 G_CALLBACK(on_countdown_draw), windata);
		}
	}

	if (windata->action_icons) {
		button = ctk_button_new_from_icon_name(key, CTK_ICON_SIZE_BUTTON);
		goto add_button;
	}

	button = ctk_button_new();
	hbox = ctk_box_new(CTK_ORIENTATION_HORIZONTAL, 6);
	ctk_widget_show(hbox);
	ctk_container_add(CTK_CONTAINER(button), hbox);

	/* Try to be smart and find a suitable icon. */
	buf = g_strdup_printf("stock_%s", key);
	pixbuf = ctk_icon_theme_load_icon(ctk_icon_theme_get_for_screen(cdk_window_get_screen(ctk_widget_get_window(CTK_WIDGET(nw)))),
																	buf, 16, CTK_ICON_LOOKUP_USE_BUILTIN, NULL);
	g_free(buf);

	if (pixbuf != NULL)
	{
		CtkWidget* image = ctk_image_new_from_pixbuf(pixbuf);
		ctk_widget_show(image);
		ctk_box_pack_start(CTK_BOX(hbox), image, FALSE, FALSE, 0);
		ctk_widget_set_halign (image, CTK_ALIGN_CENTER);
		ctk_widget_set_valign (image, CTK_ALIGN_CENTER);
	}

	label = ctk_label_new(NULL);
	ctk_widget_show(label);
	ctk_box_pack_start(CTK_BOX(hbox), label, FALSE, FALSE, 0);
	ctk_label_set_xalign (CTK_LABEL (label), 0.0);
	ctk_label_set_yalign (CTK_LABEL (label), 0.5);
	buf = g_strdup_printf("<small>%s</small>", text);
	ctk_label_set_markup(CTK_LABEL(label), buf);
	g_free(buf);

add_button:
	ctk_widget_show(button);
	ctk_box_pack_start(CTK_BOX(windata->actions_box), button, FALSE, FALSE, 0);

	g_object_set_data(G_OBJECT(button), "_nw", nw);
	g_object_set_data_full(G_OBJECT(button), "_action_key", g_strdup(key), g_free);
	g_signal_connect(G_OBJECT(button), "button-release-event", G_CALLBACK(action_clicked_cb), cb);

	ctk_widget_show_all(windata->actions_box);
}

void clear_notification_actions(CtkWindow* nw)
{
	WindowData* windata = g_object_get_data(G_OBJECT(nw), "windata");

	windata->pie_countdown = NULL;

	ctk_widget_hide(windata->actions_box);
	ctk_container_foreach(CTK_CONTAINER(windata->actions_box), (CtkCallback) ctk_widget_destroy, NULL);
}

void move_notification(CtkWidget* nw, int x, int y)
{
	WindowData* windata = g_object_get_data(G_OBJECT(nw), "windata");

	g_assert(windata != NULL);

	if (windata->has_arrow)
	{
		ctk_widget_queue_resize(nw);
	}
	else
	{
		ctk_window_move(CTK_WINDOW(nw), x, y);
	}
}

void get_theme_info(char** theme_name, char** theme_ver, char** author, char** homepage)
{
	*theme_name = g_strdup("Standard");

	/* If they are constants, maybe we can remove printf and use G_STRINGIFY() */
	*theme_ver = g_strdup_printf("%d.%d.%d", NOTIFICATION_DAEMON_MAJOR_VERSION, NOTIFICATION_DAEMON_MINOR_VERSION, NOTIFICATION_DAEMON_MICRO_VERSION);
	*author = g_strdup("Christian Hammond");
	*homepage = g_strdup("http://www.galago-project.org/");
}

gboolean theme_check_init(unsigned int major_ver, unsigned int minor_ver, unsigned int micro_ver)
{
	return major_ver == NOTIFICATION_DAEMON_MAJOR_VERSION && minor_ver == NOTIFICATION_DAEMON_MINOR_VERSION && micro_ver == NOTIFICATION_DAEMON_MICRO_VERSION;
}
