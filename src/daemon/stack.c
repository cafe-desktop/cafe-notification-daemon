/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2006 Christian Hammond <chipx86@chipx86.com>
 * Copyright (C) 2010 Red Hat, Inc.
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
#include "engines.h"
#include "stack.h"

#include <X11/Xproto.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <cdk/cdkx.h>

#define NOTIFY_STACK_SPACING 2
#define WORKAREA_PADDING 6

struct _NotifyStack {
	NotifyDaemon* daemon;
	CdkScreen* screen;
	CdkMonitor *monitor;
	NotifyStackLocation location;
	GList* windows;
	guint update_id;
};

GList* notify_stack_get_windows(NotifyStack *stack)
{
	return stack->windows;
}

static gboolean
get_work_area (NotifyStack  *stack,
               CdkRectangle *rect)
{
        Atom            workarea;
        Atom            type;
        Window          win;
        int             format;
        gulong          num;
        gulong          leftovers;
        gulong          max_len = 4 * 32;
        guchar         *ret_workarea;
        long           *workareas;
        int             result;
        int             disp_screen;

	workarea = XInternAtom(CDK_DISPLAY_XDISPLAY(cdk_display_get_default()), "_NET_WORKAREA", True);


        disp_screen = CDK_SCREEN_XNUMBER (stack->screen);

        /* Defaults in case of error */
        rect->x = 0;
        rect->y = 0;
        rect->width = WidthOfScreen (cdk_x11_screen_get_xscreen (stack->screen));
        rect->height = HeightOfScreen (cdk_x11_screen_get_xscreen (stack->screen));

        if (workarea == None)
                return FALSE;


	win = XRootWindow(CDK_DISPLAY_XDISPLAY(cdk_display_get_default()), disp_screen);

	result = XGetWindowProperty(CDK_DISPLAY_XDISPLAY(cdk_display_get_default()),
		win,
		workarea,
		0,
		max_len,
		False,
		AnyPropertyType,
		&type,
		&format,
		&num,
		&leftovers,
		&ret_workarea);


        if (result != Success
            || type == None
            || format == 0
            || leftovers
            || num % 4) {
                return FALSE;
        }

        workareas = (long *) ret_workarea;
        rect->x = workareas[disp_screen * 4];
        rect->y = workareas[disp_screen * 4 + 1];
        rect->width = workareas[disp_screen * 4 + 2];
        rect->height = workareas[disp_screen * 4 + 3];

        XFree (ret_workarea);

        return TRUE;
}

static void
get_origin_coordinates (NotifyStackLocation stack_location,
                        CdkRectangle       *workarea,
                        gint               *x,
                        gint               *y,
                        gint               *shiftx,
                        gint               *shifty,
                        gint                width,
                        gint                height)
{
        switch (stack_location) {
        case NOTIFY_STACK_LOCATION_TOP_LEFT:
                *x = workarea->x;
                *y = workarea->y;
                *shifty = height;
                break;

        case NOTIFY_STACK_LOCATION_TOP_RIGHT:
                *x = workarea->x + workarea->width - width;
                *y = workarea->y;
                *shifty = height;
                break;

        case NOTIFY_STACK_LOCATION_BOTTOM_LEFT:
                *x = workarea->x;
                *y = workarea->y + workarea->height - height;
                break;

        case NOTIFY_STACK_LOCATION_BOTTOM_RIGHT:
                *x = workarea->x + workarea->width - width;
                *y = workarea->y + workarea->height - height;
                break;

        default:
                g_assert_not_reached ();
        }
}

static void
translate_coordinates (NotifyStackLocation stack_location,
                       CdkRectangle       *workarea,
                       gint               *x,
                       gint               *y,
                       gint               *shiftx,
                       gint               *shifty,
                       gint                width,
                       gint                height)
{
        switch (stack_location) {
        case NOTIFY_STACK_LOCATION_TOP_LEFT:
                *x = workarea->x;
                *y += *shifty;
                *shifty = height;
                break;

        case NOTIFY_STACK_LOCATION_TOP_RIGHT:
                *x = workarea->x + workarea->width - width;
                *y += *shifty;
                *shifty = height;
                break;

        case NOTIFY_STACK_LOCATION_BOTTOM_LEFT:
                *x = workarea->x;
                *y -= height;
                break;

        case NOTIFY_STACK_LOCATION_BOTTOM_RIGHT:
                *x = workarea->x + workarea->width - width;
                *y -= height;
                break;

        default:
                g_assert_not_reached ();
        }
}

static int
_ctk_get_monitor_num (CdkMonitor *monitor)
{
        CdkDisplay *display;
        int n_monitors, i;

        display = cdk_monitor_get_display(monitor);
        n_monitors = cdk_display_get_n_monitors(display);

        for(i = 0; i < n_monitors; i++)
        {
                if (cdk_display_get_monitor(display, i) == monitor) return i;
        }

        return -1;
}

NotifyStack *
notify_stack_new (NotifyDaemon       *daemon,
                  CdkScreen          *screen,
                  CdkMonitor         *monitor,
                  NotifyStackLocation location)
{
        NotifyStack    *stack;
        CdkDisplay     *display;

        display = cdk_screen_get_display (screen);
        g_assert (daemon != NULL);
        g_assert (screen != NULL && CDK_IS_SCREEN (screen));
        g_assert ((guint)_ctk_get_monitor_num (monitor) < (guint)cdk_display_get_n_monitors (display));
        g_assert (location != NOTIFY_STACK_LOCATION_UNKNOWN);

        stack = g_new0 (NotifyStack, 1);
        stack->daemon = daemon;
        stack->screen = screen;
        stack->monitor = monitor;
        stack->location = location;

        return stack;
}

void
notify_stack_destroy (NotifyStack *stack)
{
        GList* l;

        g_assert (stack != NULL);

        if (stack->update_id != 0) {
                g_source_remove (stack->update_id);
        }

        for (l = stack->windows; l != NULL; l = l->next) {
                CtkWindow *nw = CTK_WINDOW (l->data);
                g_signal_handlers_disconnect_by_data(G_OBJECT(nw), stack);
        }

        g_list_free (stack->windows);
        g_free (stack);
}

void
notify_stack_set_location (NotifyStack        *stack,
                           NotifyStackLocation location)
{
        stack->location = location;
}

static void
add_padding_to_rect (CdkRectangle *rect)
{
        rect->x += WORKAREA_PADDING;
        rect->y += WORKAREA_PADDING;
        rect->width -= WORKAREA_PADDING * 2;
        rect->height -= WORKAREA_PADDING * 2;

        if (rect->width < 0)
                rect->width = 0;
        if (rect->height < 0)
                rect->height = 0;
}

static void
notify_stack_shift_notifications (NotifyStack *stack,
                                  CtkWindow   *nw,
                                  GList      **nw_l,
                                  gint         init_width,
                                  gint         init_height,
                                  gint        *nw_x,
                                  gint        *nw_y)
{
        CdkRectangle    workarea;
        CdkRectangle    monitor;
        CdkRectangle   *positions;
        GList          *l;
        gint            x, y;
        gint            shiftx = 0;
        gint            shifty = 0;
        int             i;
        int             n_wins;

        get_work_area (stack, &workarea);
        cdk_monitor_get_geometry (stack->monitor, &monitor);
        cdk_rectangle_intersect (&monitor, &workarea, &workarea);

        add_padding_to_rect (&workarea);

        n_wins = g_list_length (stack->windows);
        positions = g_new0 (CdkRectangle, n_wins);

        get_origin_coordinates (stack->location,
                                &workarea,
                                &x, &y,
                                &shiftx,
                                &shifty,
                                init_width,
                                init_height);

        if (nw_x != NULL)
                *nw_x = x;

        if (nw_y != NULL)
                *nw_y = y;

        for (i = 0, l = stack->windows; l != NULL; i++, l = l->next) {
                CtkWindow      *nw2 = CTK_WINDOW (l->data);
                CtkRequisition  req;

                if (nw == NULL || nw2 != nw) {
                        ctk_widget_get_preferred_size (CTK_WIDGET (nw2), NULL, &req);

                        translate_coordinates (stack->location,
                                               &workarea,
                                               &x,
                                               &y,
                                               &shiftx,
                                               &shifty,
                                               req.width,
                                               req.height + NOTIFY_STACK_SPACING);
                        positions[i].x = x;
                        positions[i].y = y;
                } else if (nw_l != NULL) {
                        *nw_l = l;
                        positions[i].x = -1;
                        positions[i].y = -1;
                }
        }

        /* move bubbles at the bottom of the stack first
           to avoid overlapping */
        for (i = n_wins - 1, l = g_list_last (stack->windows); l != NULL; i--, l = l->prev) {
                CtkWindow *nw2 = CTK_WINDOW (l->data);

                if (nw == NULL || nw2 != nw) {
                        theme_move_notification (nw2, positions[i].x, positions[i].y);
                }
        }

        g_free (positions);
}

static void update_position(NotifyStack* stack)
{
	/* notify_stack_shift_notifications(stack, window, list pointer, init width, init height, out window x, out window y) */
	notify_stack_shift_notifications (stack, NULL, NULL, 0, 0, NULL, NULL);
}

static gboolean update_position_idle(NotifyStack* stack)
{
	update_position(stack);

	stack->update_id = 0;
	return FALSE;
}

void notify_stack_queue_update_position(NotifyStack* stack)
{
	if (stack->update_id != 0)
	{
		return;
	}

	stack->update_id = g_idle_add((GSourceFunc) update_position_idle, stack);
}

void notify_stack_add_window(NotifyStack* stack, CtkWindow* nw, gboolean new_notification)
{
	CtkRequisition  req;
	gint            x, y;

	ctk_widget_get_preferred_size(CTK_WIDGET(nw), NULL, &req);
	notify_stack_shift_notifications(stack, nw, NULL, req.width, req.height + NOTIFY_STACK_SPACING, &x, &y);
	theme_move_notification(nw, x, y);

	if (new_notification)
	{
		g_signal_connect_swapped(G_OBJECT(nw), "destroy", G_CALLBACK(notify_stack_remove_window), stack);
		stack->windows = g_list_prepend(stack->windows, nw);
	}
}

void notify_stack_remove_window(NotifyStack* stack, CtkWindow* nw)
{
	GList* remove_l = NULL;

	notify_stack_shift_notifications(stack, nw, &remove_l, 0, 0, NULL, NULL);

	if (remove_l != NULL)
	{
		stack->windows = g_list_delete_link(stack->windows, remove_l);
	}

	if (ctk_widget_get_realized(CTK_WIDGET(nw)))
		ctk_widget_unrealize(CTK_WIDGET(nw));
}
