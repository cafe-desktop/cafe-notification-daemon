/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007 Jonh Wendell <wendell@bani.com.br>
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
#include <glib.h>
#include <gmodule.h>
#include <ctk/ctk.h>
#include <cdk/cdk.h>
#include <gio/gio.h>
#include <string.h>
#include <libnotify/notify.h>

#include "stack.h"

#define GSETTINGS_SCHEMA "org.cafe.NotificationDaemon"
#define GSETTINGS_KEY_THEME "theme"
#define GSETTINGS_KEY_POPUP_LOCATION "popup-location"
#define GSETTINGS_KEY_MONITOR_NUMBER "monitor-number"
#define GSETTINGS_KEY_USE_ACTIVE_MONITOR "use-active-monitor"
#define GSETTINGS_KEY_DO_NOT_DISTURB "do-not-disturb"

typedef struct {
	GSettings* gsettings;

	CtkWidget* dialog;
	CtkWidget* position_combo;
    CtkWidget* monitor_combo;
	CtkWidget* theme_combo;
	CtkWidget* preview_button;
	CtkWidget* active_checkbox;
	CtkWidget* dnd_checkbox;
    CtkWidget* monitor_label;

	NotifyNotification* preview;
} NotificationAppletDialog;

enum {
	NOTIFY_POSITION_LABEL,
	NOTIFY_POSITION_NAME,
	N_COLUMNS_POSITION
};

enum {
    NOTIFY_MONITOR_NUMBER,
    N_COLUMNS_MONITOR
};

enum {
	NOTIFY_THEME_LABEL,
	NOTIFY_THEME_NAME,
	NOTIFY_THEME_FILENAME,
	N_COLUMNS_THEME
};

static void notification_properties_position_notify(GSettings *settings, gchar *key, NotificationAppletDialog* dialog)
{
	CtkTreeModel* model;
	CtkTreeIter iter;
	const char* location;
	gboolean valid;

	location = g_settings_get_string(dialog->gsettings, key);

	model = ctk_combo_box_get_model(CTK_COMBO_BOX(dialog->position_combo));
	valid = ctk_tree_model_get_iter_first(model, &iter);

	for (valid = ctk_tree_model_get_iter_first(model, &iter); valid; valid = ctk_tree_model_iter_next(model, &iter))
	{
		gchar* key;

		ctk_tree_model_get(model, &iter, NOTIFY_POSITION_NAME, &key, -1);

		if (g_str_equal(key, location))
		{
			ctk_combo_box_set_active_iter(CTK_COMBO_BOX(dialog->position_combo), &iter);
			g_free(key);
			break;
		}

		g_free(key);
	}
}

static void notification_properties_monitor_changed(CtkComboBox* widget, NotificationAppletDialog* dialog)
{
	gint monitor;
	CtkTreeIter iter;

	CtkTreeModel *model = ctk_combo_box_get_model(CTK_COMBO_BOX(dialog->monitor_combo));

	if (!ctk_combo_box_get_active_iter(CTK_COMBO_BOX(dialog->monitor_combo), &iter))
	{
		return;
	}

	ctk_tree_model_get(model, &iter, NOTIFY_MONITOR_NUMBER, &monitor, -1);

	g_settings_set_int(dialog->gsettings, GSETTINGS_KEY_MONITOR_NUMBER, monitor);
}

static void notification_properties_monitor_notify(GSettings *settings, gchar *key, NotificationAppletDialog* dialog)
{
	CtkTreeModel* model;
	CtkTreeIter iter;
	gint monitor_number;
	gint monitor_number_at_iter;
	gboolean valid;

	model = ctk_combo_box_get_model(CTK_COMBO_BOX(dialog->monitor_combo));

	monitor_number = g_settings_get_int(dialog->gsettings, GSETTINGS_KEY_MONITOR_NUMBER);

	for (valid = ctk_tree_model_get_iter_first(model, &iter); valid; valid = ctk_tree_model_iter_next(model, &iter))
	{
		ctk_tree_model_get(model, &iter, NOTIFY_MONITOR_NUMBER, &monitor_number_at_iter, -1);

		if (monitor_number_at_iter == monitor_number)
		{
			ctk_combo_box_set_active_iter(CTK_COMBO_BOX(dialog->monitor_combo), &iter);
			break;
		}
	}
}

static void notification_properties_location_changed(CtkComboBox* widget, NotificationAppletDialog* dialog)
{
	char* location;
	CtkTreeIter iter;

	CtkTreeModel* model = ctk_combo_box_get_model(CTK_COMBO_BOX(dialog->position_combo));

	if (!ctk_combo_box_get_active_iter(CTK_COMBO_BOX(dialog->position_combo), &iter))
	{
		return;
	}

	ctk_tree_model_get(model, &iter, NOTIFY_POSITION_NAME, &location, -1);

	g_settings_set_string (dialog->gsettings, GSETTINGS_KEY_POPUP_LOCATION, location);
	g_free(location);
}

static void notification_properties_dialog_setup_positions(NotificationAppletDialog* dialog)
{
	char* location;
	gboolean valid;
	CtkTreeModel* model;
	CtkTreeIter iter;

	g_signal_connect(dialog->gsettings, "changed::" GSETTINGS_KEY_POPUP_LOCATION, G_CALLBACK (notification_properties_position_notify), dialog);
	g_signal_connect(dialog->position_combo, "changed", G_CALLBACK(notification_properties_location_changed), dialog);

	model = ctk_combo_box_get_model(CTK_COMBO_BOX(dialog->position_combo));
	location = g_settings_get_string(dialog->gsettings, GSETTINGS_KEY_POPUP_LOCATION);

	for (valid = ctk_tree_model_get_iter_first(model, &iter); valid; valid = ctk_tree_model_iter_next(model, &iter))
	{
		gchar* key;

		ctk_tree_model_get(model, &iter, NOTIFY_POSITION_NAME, &key, -1);

		if (g_str_equal(key, location))
		{
			ctk_combo_box_set_active_iter(CTK_COMBO_BOX(dialog->position_combo), &iter);
			g_free(key);
			break;
		}

		g_free(key);
	}

	g_free(location);
}

static void notification_properties_dialog_setup_monitors(NotificationAppletDialog* dialog)
{
	CtkListStore *store;
	CdkDisplay *display;
	CdkScreen *screen;
	CtkTreeIter iter;
	gint num_monitors;
	gint cur_monitor_number;
	gint cur_monitor_number_at_iter;
	gboolean valid;

	// Assumes the user has only one display.
	// TODO: add support for multiple displays.
	display = cdk_display_get_default();
	g_assert(display != NULL);

	// Assumes the user has only one screen.
	// TODO: add support for mulitple screens.
	screen = cdk_display_get_default_screen(display);
	g_assert(screen != NULL);

	num_monitors = cdk_display_get_n_monitors(display);
	g_assert(num_monitors >= 1);

	store = ctk_list_store_new(N_COLUMNS_MONITOR, G_TYPE_INT);

    gint i;
	for (i = 0; i < num_monitors; i++)
	{
        ctk_list_store_append(store, &iter);
		ctk_list_store_set(store, &iter, NOTIFY_MONITOR_NUMBER, i, -1);
	}

	ctk_combo_box_set_model(CTK_COMBO_BOX (dialog->monitor_combo), CTK_TREE_MODEL (store));

	g_signal_connect(dialog->gsettings, "changed::" GSETTINGS_KEY_MONITOR_NUMBER, G_CALLBACK (notification_properties_monitor_notify), dialog);
	cur_monitor_number = g_settings_get_int(dialog->gsettings, GSETTINGS_KEY_MONITOR_NUMBER);

	for (valid = ctk_tree_model_get_iter_first(CTK_TREE_MODEL (store), &iter); valid; valid = ctk_tree_model_iter_next(CTK_TREE_MODEL (store), &iter))
	{
		ctk_tree_model_get(CTK_TREE_MODEL (store), &iter, NOTIFY_MONITOR_NUMBER, &cur_monitor_number_at_iter, -1);

		if (cur_monitor_number_at_iter == cur_monitor_number)
		{
			ctk_combo_box_set_active_iter(CTK_COMBO_BOX(dialog->monitor_combo), &iter);
			break;
		}
	}

	g_object_unref(store);

	g_signal_connect(dialog->monitor_combo, "changed", G_CALLBACK(notification_properties_monitor_changed), dialog);
}

static void notification_properties_theme_notify(GSettings *settings, gchar *key, NotificationAppletDialog* dialog)
{
	const char* theme = g_settings_get_string(dialog->gsettings, key);

	CtkTreeModel* model = ctk_combo_box_get_model(CTK_COMBO_BOX(dialog->theme_combo));

	CtkTreeIter iter;
	gboolean valid;

	for (valid = ctk_tree_model_get_iter_first(model, &iter); valid; valid = ctk_tree_model_iter_next(model, &iter))
	{
		gchar* theme_name;

		ctk_tree_model_get(model, &iter, NOTIFY_THEME_NAME, &theme_name, -1);

		if (g_str_equal(theme_name, theme))
		{
			ctk_combo_box_set_active_iter(CTK_COMBO_BOX(dialog->theme_combo), &iter);
			g_free(theme_name);
			break;
		}

		g_free(theme_name);
	}
}

static void notification_properties_theme_changed(CtkComboBox* widget, NotificationAppletDialog* dialog)
{
	char* theme;
	CtkTreeIter iter;

	CtkTreeModel* model = ctk_combo_box_get_model(CTK_COMBO_BOX(dialog->theme_combo));

	if (!ctk_combo_box_get_active_iter(CTK_COMBO_BOX(dialog->theme_combo), &iter))
	{
		return;
	}

	ctk_tree_model_get(model, &iter, NOTIFY_THEME_NAME, &theme, -1);
	g_settings_set_string(dialog->gsettings, GSETTINGS_KEY_THEME, theme);
	g_free(theme);
}

static gchar* get_theme_name(const gchar* filename)
{
	/* TODO: Remove magic numbers. Strip "lib" and ".so" */
	gchar* result = g_strdup(filename + 3);
	result[strlen(result) - strlen("." G_MODULE_SUFFIX)] = '\0';
	return result;
}

static void notification_properties_dialog_setup_themes(NotificationAppletDialog* dialog)
{
	GDir* dir;
	const gchar* filename;
	char* theme;
	gboolean valid;
	CtkListStore* store;
	CtkTreeIter iter;

	store = ctk_list_store_new(N_COLUMNS_THEME, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

	ctk_combo_box_set_model(CTK_COMBO_BOX(dialog->theme_combo), CTK_TREE_MODEL(store));

	g_signal_connect(dialog->gsettings, "changed::" GSETTINGS_KEY_THEME, G_CALLBACK (notification_properties_theme_notify), dialog);
	g_signal_connect(dialog->theme_combo, "changed", G_CALLBACK(notification_properties_theme_changed), dialog);

	if ((dir = g_dir_open(ENGINES_DIR, 0, NULL)))
	{
		while ((filename = g_dir_read_name(dir)))
		{
			if (g_str_has_suffix(filename, "." G_MODULE_SUFFIX))
			{
				char* theme_name;
				char* theme_label;

				theme_name = get_theme_name(filename);

				/* FIXME: other solution than hardcode? */
				if (g_str_equal(theme_name, "coco"))
				{
					theme_label = g_strdup(_("Coco"));
				}
				else if (g_str_equal(theme_name, "nodoka"))
				{
					theme_label = g_strdup(_("Nodoka"));
				}
				else if (g_str_equal(theme_name, "slider"))
				{
					theme_label = g_strdup(_("Slider"));
				}
				else if (g_str_equal(theme_name, "standard"))
				{
					theme_label = g_strdup(_("Standard theme"));
				}
				else
				{
					theme_label = g_strdup(theme_name);
				}

				ctk_list_store_append(store, &iter);
				ctk_list_store_set(store, &iter, NOTIFY_THEME_LABEL, theme_label, NOTIFY_THEME_NAME, theme_name, NOTIFY_THEME_FILENAME, filename, -1);
				g_free(theme_name);
				g_free(theme_label);
			}
		}

		g_dir_close(dir);
	}
	else
	{
		g_warning("Error opening themes dir");
	}

	theme = g_settings_get_string(dialog->gsettings, GSETTINGS_KEY_THEME);

	for (valid = ctk_tree_model_get_iter_first(CTK_TREE_MODEL(store), &iter); valid; valid = ctk_tree_model_iter_next(CTK_TREE_MODEL(store), &iter))
	{
		gchar* key;

		ctk_tree_model_get(CTK_TREE_MODEL(store), &iter, NOTIFY_THEME_NAME, &key, -1);

		if (g_str_equal(key, theme))
		{
			ctk_combo_box_set_active_iter(CTK_COMBO_BOX(dialog->theme_combo), &iter);
			g_free(key);
			break;
		}

		g_free(key);
	}

	g_free(theme);
}

static void notification_properties_checkbox_toggled(CtkWidget* widget, NotificationAppletDialog* dialog)
{
    gboolean is_active;

    is_active = ctk_toggle_button_get_active(CTK_TOGGLE_BUTTON (widget));

    // This was called as a result of notification_properties_checkbox_notify being called.
    // Stop here instead of doing redundant work.
    if (is_active == g_settings_get_boolean(dialog->gsettings, GSETTINGS_KEY_USE_ACTIVE_MONITOR))
    {
        return;
    }

	if (is_active)
	{
		g_settings_set_boolean(dialog->gsettings, GSETTINGS_KEY_USE_ACTIVE_MONITOR, TRUE);
		ctk_widget_set_sensitive(dialog->monitor_combo, FALSE);
		ctk_widget_set_sensitive(dialog->monitor_label, FALSE);
	}
	else
	{
		g_settings_set_boolean(dialog->gsettings, GSETTINGS_KEY_USE_ACTIVE_MONITOR, FALSE);
		ctk_widget_set_sensitive(dialog->monitor_combo, TRUE);
		ctk_widget_set_sensitive(dialog->monitor_label, TRUE);
	}
}

static void notification_properties_checkbox_notify(GSettings *settings, gchar *key, NotificationAppletDialog* dialog)
{
    gboolean is_set;

    is_set = g_settings_get_boolean(settings, key);

    // This was called as a result of notification_properties_checkbox_toggled being called.
    // Stop here instead of doing redundant work.
    if(is_set == ctk_toggle_button_get_active(CTK_TOGGLE_BUTTON (dialog->active_checkbox)))
    {
        return;
    }

	if (is_set)
	{
		ctk_widget_set_sensitive(dialog->monitor_combo, FALSE);
		ctk_widget_set_sensitive(dialog->monitor_label, FALSE);
		ctk_toggle_button_set_active(CTK_TOGGLE_BUTTON (dialog->active_checkbox), TRUE);
	}
	else
	{
		ctk_widget_set_sensitive(dialog->monitor_combo, TRUE);
		ctk_widget_set_sensitive(dialog->monitor_label, TRUE);
		ctk_toggle_button_set_active(CTK_TOGGLE_BUTTON (dialog->active_checkbox), FALSE);
	}
}

static void notification_properties_dnd_toggled(CtkWidget* widget, NotificationAppletDialog* dialog)
{
	gboolean is_active;

	is_active = ctk_toggle_button_get_active(CTK_TOGGLE_BUTTON (widget));

	// This was called as a result of notification_properties_dnd_notify being called.
	// Stop here instead of doing redundant work.
	if (is_active == g_settings_get_boolean(dialog->gsettings, GSETTINGS_KEY_DO_NOT_DISTURB))
	{
		return;
	}

	g_settings_set_boolean(dialog->gsettings, GSETTINGS_KEY_DO_NOT_DISTURB, is_active);
}

static void notification_properties_dnd_notify(GSettings *settings, gchar *key, NotificationAppletDialog* dialog)
{
	gboolean is_set;

	is_set = g_settings_get_boolean(settings, key);

	// This was called as a result of notification_properties_dnd_toggled being called.
	// Stop here instead of doing redundant work.
	if(is_set == ctk_toggle_button_get_active(CTK_TOGGLE_BUTTON (dialog->dnd_checkbox)))
	{
		return;
	}

	ctk_toggle_button_set_active(CTK_TOGGLE_BUTTON (dialog->dnd_checkbox), is_set);
}

static void show_message(NotificationAppletDialog* dialog, const gchar* message)
{
	CtkWidget* d = ctk_message_dialog_new(CTK_WINDOW(dialog->dialog), CTK_DIALOG_DESTROY_WITH_PARENT, CTK_MESSAGE_ERROR, CTK_BUTTONS_CLOSE, "%s", message);
	ctk_dialog_run(CTK_DIALOG(d));
	ctk_widget_destroy(d);
}

static void notification_properties_dialog_preview_closed(NotifyNotification* preview, NotificationAppletDialog* dialog)
{
	if (preview == dialog->preview)
	{
		dialog->preview = NULL;
	}

	g_object_unref(preview);
}

static void notification_properties_dialog_preview(NotificationAppletDialog* dialog)
{
	if (!notify_is_initted() && !notify_init("n-d"))
	{
		show_message(dialog, _("Error initializing libcafenotify"));
		return;
	}

	GError* error = NULL;

	if (dialog->preview)
	{
		notify_notification_close(dialog->preview, NULL);
		g_object_unref(dialog->preview);
		dialog->preview = NULL;
	}

	dialog->preview = notify_notification_new(_("Notification Test"), _("Just a test"), "dialog-information");

	if (!notify_notification_show(dialog->preview, &error))
	{
		char* message = g_strdup_printf(_("Error while displaying notification: %s"), error->message);
		show_message(dialog, message);
		g_error_free(error);
		g_free(message);
	}

	g_signal_connect(dialog->preview, "closed", G_CALLBACK(notification_properties_dialog_preview_closed), dialog);
}

static void notification_properties_dialog_response(CtkWidget* widget, int response, NotificationAppletDialog* dialog)
{
	switch (response)
	{
		case CTK_RESPONSE_ACCEPT:
			notification_properties_dialog_preview(dialog);
			break;

		case CTK_RESPONSE_CLOSE:
		default:
			ctk_widget_destroy(widget);
			break;
	}
}

static void notification_properties_dialog_destroyed(CtkWidget* widget, NotificationAppletDialog* dialog)
{
	dialog->dialog = NULL;

	ctk_main_quit();
}

static gboolean notification_properties_dialog_init(NotificationAppletDialog* dialog)
{
	CtkBuilder* builder = ctk_builder_new();
	GError* error = NULL;

	ctk_builder_add_from_resource (builder, "/org/cafe/notifications/properties/cafe-notification-properties.ui", &error);

	if (error != NULL)
	{
		g_warning(_("Could not load user interface: %s"), error->message);
		g_error_free(error);
		return FALSE;
	}

	dialog->dialog = CTK_WIDGET(ctk_builder_get_object(builder, "dialog"));
	g_assert(dialog->dialog != NULL);

	dialog->position_combo = CTK_WIDGET(ctk_builder_get_object(builder, "position_combo"));
	g_assert(dialog->position_combo != NULL);

	dialog->monitor_combo = CTK_WIDGET(ctk_builder_get_object(builder, "monitor_combo"));
	g_assert(dialog->monitor_combo != NULL);

	dialog->theme_combo = CTK_WIDGET(ctk_builder_get_object(builder, "theme_combo"));
	g_assert(dialog->theme_combo != NULL);

	dialog->active_checkbox = CTK_WIDGET(ctk_builder_get_object(builder, "use_active_check"));
	g_assert(dialog->active_checkbox != NULL);

	dialog->dnd_checkbox = CTK_WIDGET(ctk_builder_get_object(builder, "do_not_disturb_check"));
	g_assert(dialog->dnd_checkbox != NULL);

    dialog->monitor_label = CTK_WIDGET(ctk_builder_get_object(builder, "monitor_label"));
    g_assert(dialog->monitor_label != NULL);

	g_object_unref(builder);

	dialog->gsettings = g_settings_new (GSETTINGS_SCHEMA);

	g_signal_connect(dialog->dialog, "response", G_CALLBACK(notification_properties_dialog_response), dialog);
	g_signal_connect(dialog->dialog, "destroy", G_CALLBACK(notification_properties_dialog_destroyed), dialog);

	g_signal_connect(dialog->gsettings, "changed::" GSETTINGS_KEY_USE_ACTIVE_MONITOR, G_CALLBACK (notification_properties_checkbox_notify), dialog);
	g_signal_connect(dialog->active_checkbox, "toggled", G_CALLBACK(notification_properties_checkbox_toggled), dialog);

	g_signal_connect(dialog->gsettings, "changed::" GSETTINGS_KEY_DO_NOT_DISTURB, G_CALLBACK (notification_properties_dnd_notify), dialog);
	g_signal_connect(dialog->dnd_checkbox, "toggled", G_CALLBACK(notification_properties_dnd_toggled), dialog);

	notification_properties_dialog_setup_themes(dialog);
	notification_properties_dialog_setup_positions(dialog);
	notification_properties_dialog_setup_monitors(dialog);

	if (g_settings_get_boolean(dialog->gsettings, GSETTINGS_KEY_USE_ACTIVE_MONITOR))
	{
		ctk_widget_set_sensitive(dialog->monitor_combo, FALSE);
		ctk_widget_set_sensitive(dialog->monitor_label, FALSE);
	}
	else
	{
		ctk_toggle_button_set_active(CTK_TOGGLE_BUTTON (dialog->active_checkbox), FALSE);
        ctk_widget_set_sensitive(dialog->monitor_combo, TRUE);
        ctk_widget_set_sensitive(dialog->monitor_label, TRUE);
	}

	ctk_toggle_button_set_active(CTK_TOGGLE_BUTTON (dialog->dnd_checkbox), g_settings_get_boolean(dialog->gsettings, GSETTINGS_KEY_DO_NOT_DISTURB));

	ctk_widget_show_all(dialog->dialog);

	dialog->preview = NULL;

	return TRUE;
}

static void notification_properties_dialog_finalize(NotificationAppletDialog* dialog)
{
	if (dialog->dialog != NULL)
	{
		ctk_widget_destroy(dialog->dialog);
		dialog->dialog = NULL;
	}

	if (dialog->preview)
	{
		notify_notification_close(dialog->preview, NULL);
		dialog->preview = NULL;
	}
}

int main(int argc, char** argv)
{
	NotificationAppletDialog dialog = {NULL, }; /* <- ? */

	bindtextdomain(GETTEXT_PACKAGE, NOTIFICATION_LOCALEDIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);

	ctk_init(&argc, &argv);

	notify_init("cafe-notification-properties");

	if (!notification_properties_dialog_init(&dialog))
	{
		notification_properties_dialog_finalize(&dialog);
		return 1;
	}

	ctk_main();

	notification_properties_dialog_finalize(&dialog);

	return 0;
}
