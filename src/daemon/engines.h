/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2006 Christian Hammond <chipx86@chipx86.com>
 * Copyright (C) 2005 John (J5) Palmieri <johnp@redhat.com>
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

#ifndef _ENGINES_H_
#define _ENGINES_H_

#include <ctk/ctk.h>

typedef void    (*UrlClickedCb) (CtkWindow * nw, const char *url);

CtkWindow      *theme_create_notification        (UrlClickedCb url_clicked_cb);
void            theme_destroy_notification       (CtkWindow   *nw);
void            theme_show_notification          (CtkWindow   *nw);
void            theme_hide_notification          (CtkWindow   *nw);
void            theme_set_notification_hints     (CtkWindow   *nw,
						  GVariant *hints);
void            theme_set_notification_timeout   (CtkWindow   *nw,
                                                  glong        timeout);
void            theme_notification_tick          (CtkWindow   *nw,
                                                  glong        remaining);
void            theme_set_notification_text      (CtkWindow   *nw,
                                                  const char  *summary,
                                                  const char  *body);
void            theme_set_notification_icon      (CtkWindow   *nw,
                                                  GdkPixbuf   *pixbuf);
void            theme_set_notification_arrow     (CtkWindow   *nw,
                                                  gboolean     visible,
                                                  int          x,
                                                  int          y);
void            theme_add_notification_action    (CtkWindow   *nw,
                                                  const char  *label,
                                                  const char  *key,
                                                  GCallback    cb);
void            theme_clear_notification_actions (CtkWindow   *nw);
void            theme_move_notification          (CtkWindow   *nw,
                                                  int          x,
                                                  int          y);
gboolean        theme_get_always_stack           (CtkWindow   *nw);

#endif /* _ENGINES_H_ */
