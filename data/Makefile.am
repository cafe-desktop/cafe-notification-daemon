icon16dir = $(datadir)/icons/hicolor/16x16/apps
icon22dir = $(datadir)/icons/hicolor/22x22/apps
icon24dir = $(datadir)/icons/hicolor/24x24/apps
icon32dir = $(datadir)/icons/hicolor/32x32/apps
icon48dir = $(datadir)/icons/hicolor/48x48/apps
iconscalabledir = $(datadir)/icons/hicolor/scalable/apps

icon16_DATA = icons/16x16/cafe-notification-properties.png
icon22_DATA = icons/22x22/cafe-notification-properties.png
icon24_DATA = icons/24x24/cafe-notification-properties.png
icon32_DATA = icons/32x32/cafe-notification-properties.png
icon48_DATA = icons/48x48/cafe-notification-properties.png
iconscalable_DATA = icons/scalable/cafe-notification-properties.svg


servicedir   = $(DBUS_SERVICES_DIR)
service_DATA = org.freedesktop.cafe.Notifications.service

gsettingsschema_in_files = org.cafe.NotificationDaemon.gschema.xml.in
gsettings_SCHEMAS = $(gsettingsschema_in_files:.xml.in=.xml)
@GSETTINGS_RULES@

EXTRA_DIST = \
	$(gsettingsschema_in_files) \
	$(service_DATA) \
	$(icon16_DATA) \
	$(icon22_DATA) \
	$(icon24_DATA) \
	$(icon32_DATA) \
	$(icon48_DATA) \
	$(iconscalable_DATA)

CLEANFILES = $(gsettings_SCHEMAS)

gtk_update_icon_cache = $(UPDATE_ICON_CACHE) -f -t $(datadir)/icons/hicolor

install-data-hook: update-icon-cache
uninstall-hook: update-icon-cache
update-icon-cache:
	@-if test -z "$(DESTDIR)" && ICON_UPDATE; then \
		echo "Updating Gtk icon cache."; \
		$(gtk_update_icon_cache); \
	else \
		echo "*** Icon cache not updated.  After (un)install, run this:"; \
		echo "***   $(gtk_update_icon_cache)"; \
	fi

-include $(top_srcdir)/git.mk
