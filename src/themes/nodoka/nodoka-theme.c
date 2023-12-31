/*
 * nodoka-theme.c
 * This file is part of notification-daemon-engine-nodoka
 *
 * Copyright (C) 2012 - Stefano Karapetsas <stefano@karapetsas.com>
 * Copyright (C) 2008 - Martin Sourada
 *
 * notification-daemon-engine-nodoka is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of the License,
 * or (at your option) any later version.
 *
 * notification-daemon-engine-nodoka is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with notification-daemon-engine-nodoka; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */



#include "config.h"

#include <glib/gi18n.h>
#include <ctk/ctk.h>
#include <gdk/gdkx.h>

#include <libxml/xpath.h>

/* Define basic nodoka types */
typedef void (*ActionInvokedCb)(CtkWindow *nw, const char *key);
typedef void (*UrlClickedCb)(CtkWindow *nw, const char *url);

typedef struct
{
	gboolean has_arrow;

	GdkPoint point_begin;
	GdkPoint point_middle;
	GdkPoint point_end;

	int offset;
	GdkPoint position;

} ArrowParameters;

typedef struct
{
	CtkWidget *win;
	CtkWidget *top_spacer;
	CtkWidget *bottom_spacer;
	CtkWidget *main_hbox;
	CtkWidget *iconbox;
	CtkWidget *icon;
	CtkWidget *content_hbox;
	CtkWidget *summary_label;
	CtkWidget *body_label;
	CtkWidget *actions_box;
	CtkWidget *last_sep;
	CtkWidget *stripe_spacer;
	CtkWidget *pie_countdown;

	ArrowParameters arrow;

	gboolean composited;
	gboolean action_icons;

	int width;
	int height;
	int last_width;
	int last_height;

	guchar urgency;
	glong timeout;
	glong remaining;

	UrlClickedCb url_clicked;

} WindowData;


enum
{
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

#define STRIPE_WIDTH  32
#define WIDTH         400
#define IMAGE_SIZE    32
#define IMAGE_PADDING 10
#define SPACER_LEFT   30
#define PIE_RADIUS    12
#define PIE_WIDTH     (2 * PIE_RADIUS)
#define PIE_HEIGHT    (2 * PIE_RADIUS)
#define BODY_X_OFFSET (IMAGE_SIZE + 8)
#define DEFAULT_ARROW_OFFSET  (SPACER_LEFT + 12)
#define DEFAULT_ARROW_HEIGHT  14
#define DEFAULT_ARROW_WIDTH   22
#define DEFAULT_ARROW_SKEW    -6
#define BACKGROUND_OPACITY    0.92
#define GRADIENT_CENTER 0.7

/* Support Nodoka Functions */

/* Handle clicking on link */
static gboolean
activate_link (CtkLabel *label, const char *url, WindowData *windata)
{
	windata->url_clicked (CTK_WINDOW (windata->win), url);
	return TRUE;
}

/* Set if we have arrow down or arrow up */
static CtkArrowType
get_notification_arrow_type(CtkWidget *nw)
{
	WindowData *windata = g_object_get_data(G_OBJECT(nw), "windata");
	int screen_height;

	screen_height = HeightOfScreen (gdk_x11_screen_get_xscreen (
		gdk_window_get_screen (ctk_widget_get_window (nw))));

	if (windata->arrow.position.y + windata->height + DEFAULT_ARROW_HEIGHT >
		screen_height)
	{
		return CTK_ARROW_DOWN;
	}
	else
	{
		return CTK_ARROW_UP;
	}
}

/* Set arrow parameters like offset and position */
static void
set_arrow_parameters (WindowData *windata)
{
	int screen_width;
	int x,y;
	CtkArrowType arrow_type;

	screen_width = WidthOfScreen (gdk_x11_screen_get_xscreen (
		gdk_window_get_screen (ctk_widget_get_window (windata->win))));

	/* Set arrow offset */
	CtkAllocation alloc;
	ctk_widget_get_allocation(windata->win, &alloc);

	if ((windata->arrow.position.x - DEFAULT_ARROW_SKEW -
		DEFAULT_ARROW_OFFSET + alloc.width) >
			screen_width)
	{
		windata->arrow.offset = windata->arrow.position.x -
					DEFAULT_ARROW_SKEW -
					(screen_width -
						alloc.width);
	}
	else if ((windata->arrow.position.x - DEFAULT_ARROW_SKEW -
		DEFAULT_ARROW_OFFSET < 0))
	{
		windata->arrow.offset = windata->arrow.position.x -
					DEFAULT_ARROW_SKEW;
	}
	else
	{
		windata->arrow.offset = DEFAULT_ARROW_OFFSET;
	}

	if (windata->arrow.offset < 6)
	{
		windata->arrow.offset = 6;
		windata->arrow.position.x += 6;
	}
	else if (windata->arrow.offset + DEFAULT_ARROW_WIDTH + 6 >
			alloc.width)
	{
		windata->arrow.offset = alloc.width - 6 -
					DEFAULT_ARROW_WIDTH;
		windata->arrow.position.x -= 6;
	}

	/* Set arrow points X position */
	windata->arrow.point_begin.x = windata->arrow.offset;
	windata->arrow.point_middle.x = windata->arrow.offset +
					DEFAULT_ARROW_SKEW;
	windata->arrow.point_end.x = windata->arrow.offset +
					DEFAULT_ARROW_WIDTH;
	x = windata->arrow.position.x - DEFAULT_ARROW_SKEW - windata->arrow.offset;

	/* Set arrow points Y position */
	arrow_type = get_notification_arrow_type(windata->win);

	switch (arrow_type)
	{
		case CTK_ARROW_UP:
			windata->arrow.point_begin.y = DEFAULT_ARROW_HEIGHT;
			windata->arrow.point_middle.y = 0;
			windata->arrow.point_end.y = DEFAULT_ARROW_HEIGHT;
			y = windata->arrow.position.y;
			break;
		case CTK_ARROW_DOWN:
			windata->arrow.point_begin.y =
				alloc.height -
					DEFAULT_ARROW_HEIGHT;
			windata->arrow.point_middle.y =
				alloc.height;
			windata->arrow.point_end.y =
				alloc.height -
					DEFAULT_ARROW_HEIGHT;
			y = windata->arrow.position.y - alloc.height;
			break;
		default:
			g_assert_not_reached();
	}

	/* Move window to requested position */
	ctk_window_move(CTK_WINDOW(windata->win), x, y);
}

static void
destroy_windata(WindowData *windata)
{
	g_free(windata);
}

static void
update_spacers(CtkWidget *nw)
{
	WindowData *windata = g_object_get_data(G_OBJECT(nw), "windata");

	if (windata->arrow.has_arrow)
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

static void
update_content_hbox_visibility(WindowData *windata)
{
	/*
	 * This is all a hack, but until we have a libview-style ContentBox,
	 * it'll just have to do.
	 */
	if (ctk_widget_get_visible(windata->icon) ||
		ctk_widget_get_visible(windata->body_label) ||
		ctk_widget_get_visible(windata->actions_box))
	{
		ctk_widget_show(windata->content_hbox);
	}
	else
	{
		ctk_widget_hide(windata->content_hbox);
	}
}

/* Draw fuctions */
/* Standard rounded rectangle */
static void
nodoka_rounded_rectangle (cairo_t * cr,
							  double x, double y, double w, double h,
							  int radius)
{
	cairo_move_to (cr, x + radius, y);
	cairo_arc (cr, x + w - radius, y + radius, radius, G_PI * 1.5, G_PI * 2);
	cairo_arc (cr, x + w - radius, y + h - radius, radius, 0, G_PI * 0.5);
	cairo_arc (cr, x + radius, y + h - radius, radius, G_PI * 0.5, G_PI);
	cairo_arc (cr, x + radius, y + radius, radius, G_PI, G_PI * 1.5);
}

/* Rounded rectangle with arrow */
static void
nodoka_rounded_rectangle_with_arrow (cairo_t * cr,
				double x, double y, double w, double h,
				int radius, ArrowParameters * arrow)
{
	gboolean arrow_up;
	arrow_up = (arrow->point_begin.y > arrow->point_middle.y);

	cairo_translate (cr, x, y);

	cairo_rectangle_int_t rect;
	rect.x = 0;
	rect.width = w;
	if (arrow_up)
		rect.y = 0 + DEFAULT_ARROW_HEIGHT;
	else
		rect.y = 0;
	rect.height = h - DEFAULT_ARROW_HEIGHT;

	cairo_move_to (cr, rect.x + radius, rect.y);

	if (arrow_up)
	{
		cairo_line_to (cr, rect.x + arrow->point_begin.x,
				rect.y);
		cairo_line_to (cr, rect.x + arrow->point_middle.x,
				rect.y - DEFAULT_ARROW_HEIGHT);
		cairo_line_to (cr, rect.x + arrow->point_end.x,
				rect.y);
	}

	cairo_arc (cr, rect.x + rect.width - radius, rect.y + radius, radius,
			G_PI * 1.5, G_PI * 2);
	cairo_arc (cr, rect.x + rect.width - radius,
			rect.y + rect.height - radius, radius, 0, G_PI * 0.5);

	if (!arrow_up)
	{
		cairo_line_to (cr, rect.x + arrow->point_end.x,
				rect.y + rect.height);
		cairo_line_to (cr, rect.x + arrow->point_middle.x,
				rect.y + rect.height + DEFAULT_ARROW_HEIGHT);
		cairo_line_to (cr, rect.x + arrow->point_begin.x,
				rect.y + rect.height);
	}

	cairo_arc (cr, rect.x + radius, rect.y + rect.height - radius,
			radius, G_PI * 0.5, G_PI);
	cairo_arc (cr, rect.x + radius, rect.y + radius, radius, G_PI,
			G_PI * 1.5);

	cairo_translate (cr, -x, -y);

}

/* Fill background */
static void
fill_background(CtkWidget *widget, WindowData *windata, cairo_t *cr)
{
	float alpha;
	if (windata->composited)
		alpha = BACKGROUND_OPACITY;
	else
		alpha = 1.0;

	cairo_pattern_t *pattern;
	pattern = cairo_pattern_create_linear (0, 0, 0, windata->height);
	cairo_pattern_add_color_stop_rgba (pattern, 0, 0.996, 0.996, 0.89, alpha);
	cairo_pattern_add_color_stop_rgba (pattern, GRADIENT_CENTER, 0.988, 0.988, 0.714, alpha);
	cairo_pattern_add_color_stop_rgba (pattern, 1, 0.984, 0.984, 0.663, alpha);
	cairo_set_source (cr, pattern);
	cairo_pattern_destroy (pattern);

	if (windata->arrow.has_arrow)
		nodoka_rounded_rectangle_with_arrow (cr, 0, 0,
			windata->width, windata->height, 6, & (windata->arrow));
	else
		nodoka_rounded_rectangle (cr, 0, 0, windata->width,
			windata->height, 6);
	cairo_fill (cr);
}


static void
draw_stripe(CtkWidget *widget, WindowData *windata, cairo_t *cr)
{
	cairo_save (cr);
	cairo_rectangle (cr, 0, 0, STRIPE_WIDTH, windata->height);
	cairo_clip (cr);

	gdouble  color_mult = 1.0;
	GdkRGBA  top_color;
	GdkRGBA  center_color;
	GdkRGBA  bottom_color;

	float alpha;
	if (windata->composited)
		alpha = BACKGROUND_OPACITY;
	else
		alpha = 1.0;

	switch (windata->urgency)
	{
		case URGENCY_LOW: // LOW
			alpha = alpha * 0.5;
			top_color.red = 221 / 255.0 * color_mult;
			top_color.green = top_color.red;
			top_color.blue = top_color.red;
			center_color.red = 192 / 255.0 * color_mult;
			center_color.green = center_color.red;
			center_color.blue = center_color.red;
			bottom_color.red = 167 / 255.0 * color_mult;
			bottom_color.green = center_color.red;
			bottom_color.blue = center_color.red;
			break;

		case URGENCY_CRITICAL: // CRITICAL
			top_color.red = 255 / 255.0 * color_mult;
			top_color.green = 11 / 255.0 * color_mult;
			top_color.blue = top_color.green;
			center_color.red = 205 / 255.0 * color_mult;
			center_color.green = 0;
			center_color.blue = 0;
			bottom_color.red = 145 / 255.0 * color_mult;
			bottom_color.green = 0;
			bottom_color.blue = 0;
			break;

		case URGENCY_NORMAL: // NORMAL
		default:
			top_color.red = 20 / 255.0 * color_mult;
			top_color.green = 175 / 255.0 * color_mult;
			top_color.blue = color_mult;
			center_color.red = 9 / 255.0 * color_mult;
			center_color.green = 139 / 255.0 * color_mult;
			center_color.blue = 207 / 255.0 * color_mult;
			bottom_color.red = 0;
			bottom_color.green = 97 / 255.0 * color_mult;
			bottom_color.blue = 147 / 255.0 * color_mult;
			break;
	}


	cairo_pattern_t *pattern;
	pattern = cairo_pattern_create_linear (0, 0, 0, windata->height);
	cairo_pattern_add_color_stop_rgba (pattern, 0, top_color.red / color_mult, top_color.green / color_mult, top_color.blue / color_mult, alpha);
	cairo_pattern_add_color_stop_rgba (pattern, GRADIENT_CENTER, bottom_color.red / color_mult, bottom_color.green / color_mult, bottom_color.blue / color_mult, alpha);
	cairo_pattern_add_color_stop_rgba (pattern, 1, bottom_color.red / color_mult, bottom_color.green / color_mult, bottom_color.blue / color_mult, alpha);
	cairo_set_source (cr, pattern);
	cairo_pattern_destroy (pattern);

	if (windata->arrow.has_arrow)
		nodoka_rounded_rectangle_with_arrow (cr, 1, 1,
			windata->width - 2, windata->height - 2, 5, & (windata->arrow));
	else
		nodoka_rounded_rectangle (cr, 1, 1, windata->width - 2,
			windata->height - 2, 5);
	cairo_fill (cr);

	cairo_restore (cr);
}

static void
draw_border(CtkWidget *widget, WindowData *windata, cairo_t *cr)
{
	float alpha;
	if (windata->composited)
		alpha = BACKGROUND_OPACITY;
	else
		alpha = 1.0;

	cairo_pattern_t *pattern;
	pattern = cairo_pattern_create_linear (0, 0, 0, windata->height);
	cairo_pattern_add_color_stop_rgba (pattern, 0, 0.62, 0.584, 0.341, alpha);
	cairo_pattern_add_color_stop_rgba (pattern, 1, 0.776, 0.757, 0.596, alpha);
	cairo_set_source (cr, pattern);
	cairo_pattern_destroy (pattern);

	if (windata->arrow.has_arrow)
		nodoka_rounded_rectangle_with_arrow (cr, 0.5, 0.5,
			windata->width -1, windata->height -1, 6, & (windata->arrow));
	else
		nodoka_rounded_rectangle (cr, 0.5, 0.5, windata->width -1,
			windata->height -1, 6);

	cairo_set_line_width (cr, 1.0);
	cairo_stroke (cr);
}

static void
draw_pie(CtkWidget *pie, WindowData *windata, cairo_t *cr)
{
	if (windata->timeout == 0)
		return;

	gdouble arc_angle = 1.0 - (gdouble)windata->remaining / (gdouble)windata->timeout;
	cairo_set_source_rgba (cr, 1.0, 0.4, 0.0, 0.3);
	cairo_move_to(cr, PIE_RADIUS, PIE_RADIUS);
	cairo_arc_negative(cr, PIE_RADIUS, PIE_RADIUS, PIE_RADIUS,
					-G_PI/2, (-0.25 + arc_angle)*2*G_PI);
	cairo_line_to(cr, PIE_RADIUS, PIE_RADIUS);

	cairo_fill (cr);
}

static void
update_shape_region (cairo_surface_t *surface,
		     WindowData      *windata)
{
	if (windata->width == windata->last_width && windata->height == windata->last_height)
	{
		return;
	}

	if (windata->width == 0 || windata->height == 0)
	{
		CtkAllocation allocation;
		ctk_widget_get_allocation (windata->win, &allocation);

		windata->width = MAX (allocation.width, 1);
		windata->height = MAX (allocation.height, 1);
	}

	if (!windata->composited) {
		cairo_region_t *region;

		region = gdk_cairo_region_create_from_surface (surface);
		ctk_widget_shape_combine_region (windata->win, region);
		cairo_region_destroy (region);
	} else {
		ctk_widget_shape_combine_region (windata->win, NULL);
		return;
	}

	windata->last_width = windata->width;
	windata->last_height = windata->height;
}

static void
paint_window (CtkWidget  *widget,
	      cairo_t    *cr,
	      WindowData *windata)
{
	cairo_t *cr2;
	cairo_surface_t *surface;
	CtkAllocation allocation;

	if (windata->width == 0 || windata->height == 0) {
		ctk_widget_get_allocation (windata->win, &allocation);
		windata->width = allocation.width;
		windata->height = allocation.height;
	}

	if (windata->arrow.has_arrow)
		set_arrow_parameters (windata);

	surface = cairo_surface_create_similar (cairo_get_target (cr),
						CAIRO_CONTENT_COLOR_ALPHA,
						windata->width,
						windata->height);

	cr2 = cairo_create (surface);

        /* transparent background */
        cairo_rectangle (cr2, 0, 0, windata->width, windata->height);
        cairo_set_source_rgba (cr2, 0.0, 0.0, 0.0, 0.0);
        cairo_fill (cr2);

	if (windata->arrow.has_arrow) {
		nodoka_rounded_rectangle_with_arrow (cr2, 0, 0,
						     windata->width,
						     windata->height,
						     6,
						     & (windata->arrow));
	} else {
		nodoka_rounded_rectangle (cr2, 0, 0,
					  windata->width,
					  windata->height,
					  6);
	}

	cairo_fill (cr2);

	fill_background(widget, windata, cr2);
	draw_border(widget, windata, cr2);
	draw_stripe(widget, windata, cr2);

	cairo_destroy (cr2);

	cairo_save (cr);
	cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_surface (cr, surface, 0, 0);
	cairo_paint (cr);
	cairo_restore (cr);

	update_shape_region (surface, windata);

	cairo_surface_destroy (surface);
}

static gboolean
on_draw (CtkWidget *widget, cairo_t *cr, WindowData *windata)
{
	paint_window (widget, cr, windata);

	return FALSE;
}

/* Event handlers */
static gboolean
configure_event_cb(CtkWidget *nw,
				   GdkEventConfigure *event,
				   WindowData *windata)
{
	windata->width = event->width;
	windata->height = event->height;

	update_spacers(nw);
	ctk_widget_queue_draw(nw);

	return FALSE;
}

static void on_composited_changed (CtkWidget* window, WindowData* windata)
{
	windata->composited = gdk_screen_is_composited (ctk_widget_get_screen(window));

	ctk_widget_queue_draw (window);
}

static gboolean
countdown_expose_cb(CtkWidget *pie,
		    cairo_t *cr,
		    WindowData *windata)
{
	cairo_t *cr2;
	cairo_surface_t *surface;
	CtkAllocation alloc;

	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);

	ctk_widget_get_allocation (pie, &alloc);

	surface = cairo_surface_create_similar (cairo_get_target (cr),
					        CAIRO_CONTENT_COLOR_ALPHA,
						alloc.width,
						alloc.height);
	cr2 = cairo_create (surface);

	cairo_translate (cr2, -alloc.x, -alloc.y);
	fill_background (pie, windata, cr2);
	cairo_translate (cr2, alloc.x, alloc.y);
	draw_pie (pie, windata, cr2);
	cairo_fill (cr2);

	cairo_destroy (cr2);

	cairo_save (cr);
	cairo_set_source_surface (cr, surface, 0, 0);
	cairo_paint (cr);
	cairo_restore (cr);

	cairo_surface_destroy (surface);
	return TRUE;
}

static void
action_clicked_cb(CtkWidget *w, GdkEventButton *event,
				  ActionInvokedCb action_cb)
{
	CtkWindow *nw   = g_object_get_data(G_OBJECT(w), "_nw");
	const char *key = g_object_get_data(G_OBJECT(w), "_action_key");

	action_cb(nw, key);
}



/* Required functions */

/* Checking if we support this notification daemon version */
gboolean
theme_check_init(unsigned int major_ver, unsigned int minor_ver,
				 unsigned int micro_ver)
{
	return major_ver == NOTIFICATION_DAEMON_MAJOR_VERSION && minor_ver == NOTIFICATION_DAEMON_MINOR_VERSION && micro_ver == NOTIFICATION_DAEMON_MICRO_VERSION;
}

/* Sending theme info to the notification daemon */
void
get_theme_info(char **theme_name,
			   char **theme_ver,
			   char **author,
			   char **homepage)
{
	*theme_name = g_strdup("Nodoka");
	*theme_ver  = g_strdup_printf("%d.%d.%d", NOTIFICATION_DAEMON_MAJOR_VERSION,
                                                  NOTIFICATION_DAEMON_MINOR_VERSION,
						  NOTIFICATION_DAEMON_MICRO_VERSION);
	*author = g_strdup("Martin Sourada");
	*homepage = g_strdup("https://nodoka.fedorahosted.org/");
}

/* Create new notification */
CtkWindow *
create_notification(UrlClickedCb url_clicked)
{
	CtkWidget *spacer;
	CtkWidget *win;
	CtkWidget *main_vbox;
	CtkWidget *hbox;
	CtkWidget *vbox;
	CtkWidget *close_button;
	CtkWidget *image;
	AtkObject *atkobj;
	WindowData *windata;
	GdkVisual *visual;
	GdkScreen *screen;

	windata = g_new0(WindowData, 1);
	windata->urgency = URGENCY_NORMAL;
	windata->url_clicked = url_clicked;

	win = ctk_window_new(CTK_WINDOW_POPUP);
	ctk_window_set_resizable(CTK_WINDOW(win), FALSE);
	windata->win = win;

	windata->composited = FALSE;
	screen = ctk_window_get_screen(CTK_WINDOW(win));
	visual = gdk_screen_get_rgba_visual(screen);

	if (visual != NULL)
	{
		ctk_widget_set_visual(win, visual);
		if (gdk_screen_is_composited(screen))
			windata->composited = TRUE;
	}

	ctk_window_set_title(CTK_WINDOW(win), "Notification");
	ctk_window_set_type_hint(CTK_WINDOW(win), GDK_WINDOW_TYPE_HINT_NOTIFICATION);
	ctk_widget_add_events(win, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
	ctk_widget_realize(win);
	ctk_widget_set_size_request(win, WIDTH, -1);

	g_object_set_data_full(G_OBJECT(win), "windata", windata,
						   (GDestroyNotify)destroy_windata);
	atk_object_set_role(ctk_widget_get_accessible(win), ATK_ROLE_ALERT);

	g_signal_connect(G_OBJECT(win), "configure_event",
					 G_CALLBACK(configure_event_cb), windata);

	g_signal_connect (G_OBJECT (win), "composited-changed", G_CALLBACK (on_composited_changed), windata);

	main_vbox = ctk_box_new(CTK_ORIENTATION_VERTICAL, 0);
	ctk_widget_show(main_vbox);
	ctk_container_add(CTK_CONTAINER(win), main_vbox);

	g_signal_connect (G_OBJECT (main_vbox), "draw", G_CALLBACK (on_draw), windata);

	windata->top_spacer = ctk_image_new();
	ctk_box_pack_start(CTK_BOX(main_vbox), windata->top_spacer,
					   FALSE, FALSE, 0);
	ctk_widget_set_size_request(windata->top_spacer, -1, DEFAULT_ARROW_HEIGHT);

	windata->main_hbox = ctk_box_new(CTK_ORIENTATION_HORIZONTAL, 0);
	ctk_widget_show(windata->main_hbox);
	ctk_box_pack_start(CTK_BOX(main_vbox), windata->main_hbox,
					   FALSE, FALSE, 0);

	windata->bottom_spacer = ctk_image_new();
	ctk_box_pack_start(CTK_BOX(main_vbox), windata->bottom_spacer,
					   FALSE, FALSE, 0);
	ctk_widget_set_size_request(windata->bottom_spacer, -1,
								DEFAULT_ARROW_HEIGHT);

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
	ctk_widget_show(close_button);
	ctk_box_pack_start(CTK_BOX(hbox), close_button, FALSE, FALSE, 0);
	ctk_button_set_relief(CTK_BUTTON(close_button), CTK_RELIEF_NONE);
	ctk_container_set_border_width(CTK_CONTAINER(close_button), 0);
	ctk_widget_set_size_request(close_button, 24, 24);
	g_signal_connect_swapped(G_OBJECT(close_button), "clicked",
							 G_CALLBACK(ctk_widget_destroy), win);

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
	ctk_box_pack_start(CTK_BOX(windata->content_hbox), windata->iconbox,
					   FALSE, FALSE, 0);
	ctk_widget_set_size_request(windata->iconbox, BODY_X_OFFSET, -1);

	windata->icon = ctk_image_new();
	ctk_box_pack_start(CTK_BOX(windata->iconbox), windata->icon,
					   TRUE, TRUE, 0);
	ctk_widget_set_halign (image, CTK_ALIGN_CENTER);
	ctk_widget_set_valign (image, CTK_ALIGN_CENTER);

	vbox = ctk_box_new(CTK_ORIENTATION_VERTICAL, 6);
	ctk_widget_show(vbox);
	ctk_box_pack_start(CTK_BOX(windata->content_hbox), vbox, TRUE, TRUE, 0);

	windata->body_label = ctk_label_new(NULL);
	ctk_box_pack_start(CTK_BOX(vbox), windata->body_label, TRUE, TRUE, 0);
	ctk_label_set_xalign (CTK_LABEL (windata->body_label), 0.0);
	ctk_label_set_yalign (CTK_LABEL (windata->body_label), 0.0);
	ctk_label_set_line_wrap(CTK_LABEL(windata->body_label), TRUE);
	ctk_label_set_line_wrap_mode (CTK_LABEL (windata->body_label), PANGO_WRAP_WORD_CHAR);
    ctk_label_set_max_width_chars (CTK_LABEL (windata->body_label), 50);

	g_signal_connect(G_OBJECT(windata->body_label), "activate-link",
                         G_CALLBACK(activate_link), windata);

	atkobj = ctk_widget_get_accessible(windata->body_label);
	atk_object_set_description (atkobj, _("Notification body text."));

	windata->actions_box = ctk_box_new(CTK_ORIENTATION_HORIZONTAL, 6);
	ctk_widget_set_halign (windata->actions_box, CTK_ALIGN_END);
	ctk_widget_show(windata->actions_box);
	ctk_box_pack_start(CTK_BOX(vbox), windata->actions_box, FALSE, TRUE, 0);

	return CTK_WINDOW(win);
}

/* Set the notification text */
void
set_notification_text(CtkWindow *nw, const char *summary, const char *body)
{
	char *str;
	char* quoted;
	WindowData *windata = g_object_get_data(G_OBJECT(nw), "windata");
	g_assert(windata != NULL);

	quoted = g_markup_escape_text(summary, -1);
	str = g_strdup_printf(
		"<span color=\"#000000\"><b><big>%s</big></b></span>", quoted);
	g_free(quoted);
	ctk_label_set_markup(CTK_LABEL(windata->summary_label), str);
	g_free(str);

	/* body */
	xmlDocPtr doc;
	xmlInitParser();
	str = g_strconcat ("<markup>", "<span color=\"#000000\">", body, "</span>", "</markup>", NULL);
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
	str = g_strconcat ("<span color=\"#000000\">", quoted, "</span>", NULL);
	ctk_label_set_markup (CTK_LABEL (windata->body_label), str);
	g_free (quoted);
	g_free (str);

renrer_ok:
	xmlCleanupParser ();

	if (body == NULL || *body == '\0')
		ctk_widget_hide(windata->body_label);
	else
		ctk_widget_show(windata->body_label);

	update_content_hbox_visibility(windata);

	ctk_widget_set_size_request(
		((body != NULL && *body == '\0')
		 ? windata->body_label : windata->summary_label),
		WIDTH - (IMAGE_SIZE + IMAGE_PADDING) - 10,
		-1);
}

/* Set notification icon */
void
set_notification_icon(CtkWindow *nw, GdkPixbuf *pixbuf)
{
	WindowData *windata = g_object_get_data(G_OBJECT(nw), "windata");
	g_assert(windata != NULL);

	ctk_image_set_from_pixbuf(CTK_IMAGE(windata->icon), pixbuf);

	if (pixbuf != NULL)
	{
		int pixbuf_width = gdk_pixbuf_get_width(pixbuf);

		ctk_widget_show(windata->icon);
		ctk_widget_set_size_request(windata->iconbox,
									MAX(BODY_X_OFFSET, pixbuf_width), -1);
	}
	else
	{
		ctk_widget_hide(windata->icon);
		ctk_widget_set_size_request(windata->iconbox, BODY_X_OFFSET, -1);
	}

	update_content_hbox_visibility(windata);
}

/* Set notification arrow */
void
set_notification_arrow(CtkWidget *nw, gboolean visible, int x, int y)
{
	WindowData *windata = g_object_get_data(G_OBJECT(nw), "windata");
	g_assert(windata != NULL);

	windata->arrow.has_arrow = visible;
	windata->arrow.position.x = x;
	windata->arrow.position.y = y;

	update_spacers(nw);
}

/* Add notification action */
void
add_notification_action(CtkWindow *nw, const char *text, const char *key,
						ActionInvokedCb cb)
{
	WindowData *windata = g_object_get_data(G_OBJECT(nw), "windata");
	CtkWidget *label;
	CtkWidget *button;
	CtkWidget *hbox;
	GdkPixbuf *pixbuf;
	char *buf;

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
					 G_CALLBACK(countdown_expose_cb), windata);
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
	pixbuf = ctk_icon_theme_load_icon(
		ctk_icon_theme_get_for_screen(
			gdk_window_get_screen(ctk_widget_get_window(CTK_WIDGET(nw)))),
		buf, 16, CTK_ICON_LOOKUP_USE_BUILTIN, NULL);
	g_free(buf);

	if (pixbuf != NULL)
	{
		CtkWidget *image = ctk_image_new_from_pixbuf(pixbuf);
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
	g_object_set_data_full(G_OBJECT(button),
						   "_action_key", g_strdup(key), g_free);
	g_signal_connect(G_OBJECT(button), "button-release-event",
					 G_CALLBACK(action_clicked_cb), cb);

	ctk_widget_show_all(windata->actions_box);
}

/* Clear notification actions */
void
clear_notification_actions(CtkWindow *nw)
{
	WindowData *windata = g_object_get_data(G_OBJECT(nw), "windata");

	windata->pie_countdown = NULL;

	ctk_widget_hide(windata->actions_box);
	ctk_container_foreach(CTK_CONTAINER(windata->actions_box),
						  (CtkCallback)ctk_widget_destroy, NULL);
}

/* Move notification window */
void
move_notification(CtkWidget *nw, int x, int y)
{
	WindowData *windata = g_object_get_data(G_OBJECT(nw), "windata");
	g_assert(windata != NULL);

	if (windata->arrow.has_arrow)
	{
		ctk_widget_queue_resize(nw);
	}
	else
	{
		ctk_window_move(CTK_WINDOW(nw), x, y);
	}
}


/* Optional Functions */

/* Destroy notification */

/* Show notification */

/* Hide notification */

/* Set notification timeout */
void
set_notification_timeout(CtkWindow *nw, glong timeout)
{
	WindowData *windata = g_object_get_data(G_OBJECT(nw), "windata");
	g_assert(windata != NULL);

	windata->timeout = timeout;
}

/* Set notification hints */
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

/* Notification tick */
void
notification_tick(CtkWindow *nw, glong remaining)
{
	WindowData *windata = g_object_get_data(G_OBJECT(nw), "windata");
	windata->remaining = remaining;

	if (windata->pie_countdown != NULL)
	{
		ctk_widget_queue_draw_area(windata->pie_countdown, 0, 0,
								   PIE_WIDTH, PIE_HEIGHT);
	}
}
