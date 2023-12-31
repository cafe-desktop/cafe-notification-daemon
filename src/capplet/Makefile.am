bin_PROGRAMS = cafe-notification-properties

BUILT_SOURCES = cafe-notification-properties-resources.h cafe-notification-properties-resources.c

cafe_notification_properties_LDADD   = $(NOTIFICATION_CAPPLET_LIBS)
cafe_notification_properties_SOURCES = $(BUILT_SOURCES) cafe-notification-properties.c
cafe_notification_properties_CFLAGS = $(WARN_CFLAGS)

cafe-notification-properties-resources.h cafe-notification-properties-resources.c: org.cafe.notifications.properties.gresource.xml Makefile $(shell $(GLIB_COMPILE_RESOURCES) --generate-dependencies --sourcedir $(srcdir) $(srcdir)/org.cafe.notifications.properties.gresource.xml)
	$(AM_V_GEN) XMLLINT=$(XMLLINT) $(GLIB_COMPILE_RESOURCES) --target $@ --sourcedir $(srcdir) --generate --c-name notification_properties $<

desktopdir = $(datadir)/applications
desktop_in_files = cafe-notification-properties.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
$(desktop_DATA): $(desktop_in_files)
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

man_MANS = cafe-notification-properties.1

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src/daemon \
	$(NOTIFICATION_CAPPLET_CFLAGS) \
	-DENGINES_DIR=\"$(libdir)/cafe-notification-daemon/engines\" \
	-DNOTIFICATION_LOCALEDIR=\"$(datadir)/locale\"

CLEANFILES = $(desktop_DATA) $(BUILT_SOURCES)
EXTRA_DIST = cafe-notification-properties.ui $(pixmap_DATA) $(desktop_in_files) $(man_MANS) org.cafe.notifications.properties.gresource.xml

-include $(top_srcdir)/git.mk
