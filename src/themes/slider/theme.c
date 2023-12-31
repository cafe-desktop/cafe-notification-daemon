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
	CtkWidget* main_hbox;
	CtkWidget* icon;
	CtkWidget* content_hbox;
	CtkWidget* summary_label;
	CtkWidget* close_button;
	CtkWidget* body_label;
	CtkWidget* actions_box;
	CtkWidget* last_sep;
	CtkWidget* pie_countdown;

	gboolean has_arrow;
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
gboolean get_always_stack(CtkWidget* nw);

#define WIDTH          400
#define DEFAULT_X0     0
#define DEFAULT_Y0     0
#define DEFAULT_RADIUS 8
#define IMAGE_SIZE     48
#define PIE_RADIUS     8
#define PIE_WIDTH      (2 * PIE_RADIUS)
#define PIE_HEIGHT     (2 * PIE_RADIUS)
#define BODY_X_OFFSET  (IMAGE_SIZE + 4)
#define BACKGROUND_ALPHA    0.90

#define MAX_ICON_SIZE IMAGE_SIZE

static void draw_round_rect(cairo_t* cr, gdouble aspect, gdouble x, gdouble y, gdouble corner_radius, gdouble width, gdouble height)
{
	gdouble radius = corner_radius / aspect;

	cairo_move_to(cr, x + radius, y);

	// top-right, left of the corner
	cairo_line_to(cr, x + width - radius, y);

	// top-right, below the corner
	cairo_arc(cr, x + width - radius, y + radius, radius, -90.0f * G_PI / 180.0f, 0.0f * G_PI / 180.0f);

	// bottom-right, above the corner
	cairo_line_to(cr, x + width, y + height - radius);

	// bottom-right, left of the corner
	cairo_arc(cr, x + width - radius, y + height - radius, radius, 0.0f * G_PI / 180.0f, 90.0f * G_PI / 180.0f);

	// bottom-left, right of the corner
	cairo_line_to(cr, x + radius, y + height);

	// bottom-left, above the corner
	cairo_arc(cr, x + radius, y + height - radius, radius, 90.0f * G_PI / 180.0f, 180.0f * G_PI / 180.0f);

	// top-left, below the corner
	cairo_line_to(cr, x, y + radius);

	// top-left, right of the corner
	cairo_arc(cr, x + radius, y + radius, radius, 180.0f * G_PI / 180.0f, 270.0f * G_PI / 180.0f);
}

static void
get_background_color (CtkStyleContext *context,
                      CtkStateFlags    state,
                      CdkRGBA         *color)
{
    CdkRGBA *c;

    g_return_if_fail (color != NULL);
    g_return_if_fail (CTK_IS_STYLE_CONTEXT (context));

    ctk_style_context_get (context,
                           state,
                           "background-color", &c,
                           NULL);
    *color = *c;
    cdk_rgba_free (c);
}

static void fill_background(CtkWidget* widget, WindowData* windata, cairo_t* cr)
{
	CtkAllocation allocation;
	CtkStyleContext *context;
	CdkRGBA fg;
	CdkRGBA bg;

	ctk_widget_get_allocation(widget, &allocation);

	draw_round_rect(cr, 1.0f, DEFAULT_X0 + 1, DEFAULT_Y0 + 1, DEFAULT_RADIUS, allocation.width - 2, allocation.height - 2);

	context = ctk_widget_get_style_context(widget);

	ctk_style_context_save (context);
	ctk_style_context_set_state (context, CTK_STATE_FLAG_NORMAL);

	get_background_color (context, CTK_STATE_FLAG_NORMAL, &bg);
	ctk_style_context_get_color (context, CTK_STATE_FLAG_NORMAL, &fg);

	ctk_style_context_restore (context);

	cairo_set_source_rgba(cr, bg.red, bg.green, bg.blue, BACKGROUND_ALPHA);
	cairo_fill_preserve(cr);

	/* Should we show urgency somehow?  Probably doesn't
	 * have any meaningful value to the user... */

	cairo_set_source_rgba(cr, fg.red, fg.green, fg.blue, BACKGROUND_ALPHA);
	cairo_set_line_width(cr, 1);
	cairo_stroke(cr);
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

		region = cdk_cairo_region_create_from_surface (surface);
		ctk_widget_shape_combine_region (windata->win, region);
		cairo_region_destroy (region);
	} else {
		ctk_widget_shape_combine_region (windata->win, NULL);
		return;
	}

	windata->last_width = windata->width;
	windata->last_height = windata->height;
}

static void paint_window (CtkWidget  *widget,
			  cairo_t    *cr,
			  WindowData *windata)
{
	cairo_surface_t *surface;
	cairo_t *cr2;
		CtkAllocation allocation;

		ctk_widget_get_allocation (windata->win, &allocation);

	if (windata->width == 0 || windata->height == 0)
	{
		windata->width = MAX (allocation.width, 1);
		windata->height = MAX (allocation.height, 1);
	}

	surface = cairo_surface_create_similar (cairo_get_target (cr),
						CAIRO_CONTENT_COLOR_ALPHA,
						windata->width,
						windata->height);

	cr2 = cairo_create (surface);

	/* transparent background */
	cairo_rectangle (cr2, 0, 0, windata->width, windata->height);
	cairo_set_source_rgba (cr2, 0.0, 0.0, 0.0, 0.0);
	cairo_fill (cr2);

	fill_background (widget, windata, cr2);

	cairo_destroy(cr2);

	cairo_save (cr);
	cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_surface(cr, surface, 0, 0);
	cairo_paint(cr);
	update_shape_region (surface, windata);
	cairo_restore (cr);

	cairo_surface_destroy(surface);
}

static gboolean on_window_map(CtkWidget* widget, CdkEvent* event, WindowData* windata)
{
	return FALSE;
}

static gboolean
on_draw (CtkWidget  *widget,
	 cairo_t    *cr,
	 WindowData *windata)
{
	paint_window (widget, cr, windata);

	return FALSE;
}

static void destroy_windata(WindowData* windata)
{
	g_free(windata);
}

static void update_content_hbox_visibility(WindowData* windata)
{
	if (ctk_widget_get_visible(windata->icon) || ctk_widget_get_visible(windata->body_label) || ctk_widget_get_visible(windata->actions_box))
	{
		ctk_widget_show(windata->content_hbox);
	}
	else
	{
		ctk_widget_hide(windata->content_hbox);
	}
}

static gboolean on_configure_event(CtkWidget* widget, CdkEventConfigure* event, WindowData* windata)
{
	windata->width = event->width;
	windata->height = event->height;

	ctk_widget_queue_draw(widget);

	return FALSE;
}

static void on_window_realize(CtkWidget* widget, WindowData* windata)
{
	/* Nothing */
}

static void on_composited_changed(CtkWidget* window, WindowData* windata)
{
	windata->composited = cdk_screen_is_composited(ctk_widget_get_screen(window));

	ctk_widget_queue_draw (windata->win);
}

CtkWindow* create_notification(UrlClickedCb url_clicked)
{
	CtkWidget* win;
	CtkWidget* main_vbox;
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
	ctk_widget_set_app_paintable(win, TRUE);
	g_signal_connect(G_OBJECT(win), "map-event", G_CALLBACK(on_window_map), windata);
	g_signal_connect(G_OBJECT(win), "draw", G_CALLBACK(on_draw), windata);
	g_signal_connect(G_OBJECT(win), "realize", G_CALLBACK(on_window_realize), windata);

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

	g_signal_connect(win, "composited-changed", G_CALLBACK(on_composited_changed), windata);

	ctk_window_set_title(CTK_WINDOW(win), "Notification");
	ctk_window_set_type_hint(CTK_WINDOW(win), CDK_WINDOW_TYPE_HINT_NOTIFICATION);
	ctk_widget_add_events(win, CDK_BUTTON_PRESS_MASK | CDK_BUTTON_RELEASE_MASK);

	g_object_set_data_full(G_OBJECT(win), "windata", windata, (GDestroyNotify) destroy_windata);
	atk_object_set_role(ctk_widget_get_accessible(win), ATK_ROLE_ALERT);

	g_signal_connect(G_OBJECT(win), "configure-event", G_CALLBACK(on_configure_event), windata);

	main_vbox = ctk_box_new(CTK_ORIENTATION_VERTICAL, 0);
	ctk_widget_show(main_vbox);
	ctk_container_add(CTK_CONTAINER(win), main_vbox);
	ctk_container_set_border_width(CTK_CONTAINER(main_vbox), 12);

	windata->main_hbox = ctk_box_new(CTK_ORIENTATION_HORIZONTAL, 0);
	ctk_widget_show(windata->main_hbox);
	ctk_box_pack_start(CTK_BOX(main_vbox), windata->main_hbox, FALSE, FALSE, 0);

	/* Add icon */
	windata->icon = ctk_image_new();
	ctk_widget_set_valign (windata->icon, CTK_ALIGN_START);
	ctk_widget_set_margin_top (windata->icon, 5);
	ctk_widget_set_size_request (windata->icon, BODY_X_OFFSET, -1);
	ctk_widget_show(windata->icon);
	ctk_box_pack_start (CTK_BOX(windata->main_hbox), windata->icon, FALSE, FALSE, 0);

	/* Add vbox */
	vbox = ctk_box_new(CTK_ORIENTATION_VERTICAL, 6);
	ctk_widget_show(vbox);
	ctk_box_pack_start(CTK_BOX(windata->main_hbox), vbox, TRUE, TRUE, 0);
	ctk_container_set_border_width(CTK_CONTAINER(vbox), 10);

	/* Add the close button */
	close_button = ctk_button_new();
	ctk_widget_set_valign (close_button, CTK_ALIGN_START);
	ctk_widget_show(close_button);

	windata->close_button = close_button;
	ctk_box_pack_start (CTK_BOX (windata->main_hbox),
                        windata->close_button,
                        FALSE, FALSE, 0);

	ctk_button_set_relief(CTK_BUTTON(close_button), CTK_RELIEF_NONE);
	ctk_container_set_border_width(CTK_CONTAINER(close_button), 0);
	g_signal_connect_swapped(G_OBJECT(close_button), "clicked", G_CALLBACK(ctk_widget_destroy), win);

	atkobj = ctk_widget_get_accessible(close_button);
	atk_action_set_description(ATK_ACTION(atkobj), 0,
                                          _("Closes the notification."));
	atk_object_set_name(atkobj, "");
	atk_object_set_description (atkobj, _("Closes the notification."));

	image = ctk_image_new_from_icon_name ("window-close", CTK_ICON_SIZE_MENU);
	ctk_widget_show(image);
	ctk_container_add(CTK_CONTAINER(close_button), image);

	/* center vbox */
	windata->summary_label = ctk_label_new(NULL);
	ctk_widget_show(windata->summary_label);
	ctk_box_pack_start(CTK_BOX(vbox), windata->summary_label, TRUE, TRUE, 0);
	ctk_label_set_xalign (CTK_LABEL (windata->summary_label), 0.0);
	ctk_label_set_yalign (CTK_LABEL (windata->summary_label), 0.0);
	ctk_label_set_line_wrap(CTK_LABEL(windata->summary_label), TRUE);
	ctk_label_set_line_wrap_mode (CTK_LABEL (windata->summary_label), PANGO_WRAP_WORD_CHAR);

	atkobj = ctk_widget_get_accessible(windata->summary_label);
	atk_object_set_description (atkobj, _("Notification summary text."));

	windata->content_hbox = ctk_box_new(CTK_ORIENTATION_HORIZONTAL, 6);
	ctk_widget_show(windata->content_hbox);
	ctk_box_pack_start(CTK_BOX(vbox), windata->content_hbox, FALSE, FALSE, 0);

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
	g_signal_connect_swapped(G_OBJECT(windata->body_label), "activate-link", G_CALLBACK(windata->url_clicked), win);

	atkobj = ctk_widget_get_accessible(windata->body_label);
	atk_object_set_description (atkobj, _("Notification body text."));

	windata->actions_box = ctk_box_new(CTK_ORIENTATION_HORIZONTAL, 6);
	ctk_widget_set_halign (windata->actions_box, CTK_ALIGN_END);
	ctk_widget_show(windata->actions_box);

	ctk_box_pack_start (CTK_BOX (vbox), windata->actions_box, FALSE, TRUE, 0);

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

void set_notification_timeout(CtkWindow *nw, glong timeout)
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
	int summary_width;

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

	ctk_widget_get_preferred_size (windata->close_button, NULL, &req);
	/* -1: main_vbox border width
	   -10: vbox border width
	   -6: spacing for hbox */
	summary_width = WIDTH - (1 * 2) - (10 * 2) - BODY_X_OFFSET - req.width - (6 * 2);

	if (body != NULL && *body != '\0')
	{
		ctk_widget_set_size_request(windata->body_label, summary_width, -1);
	}

	ctk_widget_set_size_request(windata->summary_label, summary_width, -1);
}

static GdkPixbuf* scale_pixbuf(GdkPixbuf* pixbuf, int max_width, int max_height, gboolean no_stretch_hint)
{
	float scale_factor_x = 1.0;
	float scale_factor_y = 1.0;
	float scale_factor = 1.0;

	int pw = gdk_pixbuf_get_width(pixbuf);
	int ph = gdk_pixbuf_get_height(pixbuf);

	/* Determine which dimension requires the smallest scale. */
	scale_factor_x = (float) max_width / (float) pw;
	scale_factor_y = (float) max_height / (float) ph;

	if (scale_factor_x > scale_factor_y)
	{
		scale_factor = scale_factor_y;
	}
	else
	{
		scale_factor = scale_factor_x;
	}

	/* always scale down, allow to disable scaling up */
	if (scale_factor < 1.0 || !no_stretch_hint)
	{
		int scale_x;
		int scale_y;

		scale_x = (int) (pw * scale_factor);
		scale_y = (int) (ph * scale_factor);

		return gdk_pixbuf_scale_simple(pixbuf, scale_x, scale_y, GDK_INTERP_BILINEAR);
	}
	else
	{
		return g_object_ref(pixbuf);
	}
}

void set_notification_icon(CtkWindow* nw, GdkPixbuf* pixbuf)
{
	WindowData* windata = g_object_get_data(G_OBJECT(nw), "windata");

	g_assert(windata != NULL);

	GdkPixbuf* scaled = NULL;

	if (pixbuf != NULL)
	{
		scaled = scale_pixbuf(pixbuf, MAX_ICON_SIZE, MAX_ICON_SIZE, TRUE);
	}

	ctk_image_set_from_pixbuf(CTK_IMAGE(windata->icon), scaled);

	if (scaled != NULL)
	{
		int pixbuf_width = gdk_pixbuf_get_width(scaled);

		ctk_widget_show(windata->icon);
		ctk_widget_set_size_request(windata->icon, MAX(BODY_X_OFFSET, pixbuf_width), -1);
		g_object_unref(scaled);
	}
	else
	{
		ctk_widget_hide(windata->icon);

		ctk_widget_set_size_request(windata->icon, BODY_X_OFFSET, -1);
	}

	update_content_hbox_visibility(windata);
}

void set_notification_arrow(CtkWidget* nw, gboolean visible, int x, int y)
{
	WindowData* windata = g_object_get_data(G_OBJECT(nw), "windata");

	g_assert(windata != NULL);
}

static void
paint_countdown (CtkWidget  *pie,
                 cairo_t* cr,
                 WindowData* windata)
{
	CtkStyleContext* context;
	CdkRGBA bg;
	CtkAllocation allocation;
	cairo_t* cr2;
	cairo_surface_t* surface;

	context = ctk_widget_get_style_context(windata->win);

	ctk_style_context_save (context);
	ctk_style_context_set_state (context, CTK_STATE_FLAG_SELECTED);

	get_background_color (context, CTK_STATE_FLAG_SELECTED, &bg);

	ctk_style_context_restore (context);

	ctk_widget_get_allocation(pie, &allocation);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	surface = cairo_surface_create_similar(cairo_get_target(cr),
                                           CAIRO_CONTENT_COLOR_ALPHA,
                                           allocation.width,
                                           allocation.height);

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

static void on_action_clicked(CtkWidget* w, CdkEventButton *event, ActionInvokedCb action_cb)
{
	CtkWindow* nw = g_object_get_data(G_OBJECT(w), "_nw");
	const char* key = g_object_get_data(G_OBJECT(w), "_action_key");

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

	if (!ctk_widget_get_visible(windata->actions_box))
	{
		ctk_widget_show(windata->actions_box);
		update_content_hbox_visibility(windata);

		/* Don't try to re-add a pie_countdown */
		if (!windata->pie_countdown) {
			windata->pie_countdown = ctk_drawing_area_new();
			ctk_widget_set_halign (windata->pie_countdown, CTK_ALIGN_END);
			ctk_widget_set_valign (windata->pie_countdown, CTK_ALIGN_CENTER);
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
	ctk_widget_show(button);

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
	ctk_box_pack_start(CTK_BOX(windata->actions_box), button, FALSE, FALSE, 0);
	ctk_button_set_relief(CTK_BUTTON(button), CTK_RELIEF_NONE);
	ctk_container_set_border_width(CTK_CONTAINER(button), 0);

	g_object_set_data(G_OBJECT(button), "_nw", nw);
	g_object_set_data_full(G_OBJECT(button), "_action_key", g_strdup(key), g_free);
	g_signal_connect(G_OBJECT(button), "button-release-event", G_CALLBACK(on_action_clicked), cb);

	ctk_widget_show_all(windata->actions_box);
}

void clear_notification_actions(CtkWindow* nw)
{
	WindowData* windata = g_object_get_data(G_OBJECT(nw), "windata");

	windata->pie_countdown = NULL;

	ctk_widget_hide(windata->actions_box);
	ctk_container_foreach(CTK_CONTAINER(windata->actions_box), (CtkCallback) ctk_widget_destroy, NULL);
}

void move_notification(CtkWidget* widget, int x, int y)
{
	WindowData* windata = g_object_get_data(G_OBJECT(widget), "windata");

	g_assert(windata != NULL);

	ctk_window_move(CTK_WINDOW(windata->win), x, y);
}

void get_theme_info(char** theme_name, char** theme_ver, char** author, char** homepage)
{
	*theme_name = g_strdup("Slider");
	*theme_ver  = g_strdup_printf("%d.%d.%d", NOTIFICATION_DAEMON_MAJOR_VERSION, NOTIFICATION_DAEMON_MINOR_VERSION, NOTIFICATION_DAEMON_MICRO_VERSION);
	*author = g_strdup("William Jon McCann");
	*homepage = g_strdup("http://www.gnome.org/");
}

gboolean get_always_stack(CtkWidget* nw)
{
	return TRUE;
}

gboolean theme_check_init(unsigned int major_ver, unsigned int minor_ver, unsigned int micro_ver)
{
	return major_ver == NOTIFICATION_DAEMON_MAJOR_VERSION && minor_ver == NOTIFICATION_DAEMON_MINOR_VERSION && micro_ver == NOTIFICATION_DAEMON_MICRO_VERSION;
}
