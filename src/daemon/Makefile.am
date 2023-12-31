libexec_PROGRAMS = cafe-notification-daemon

cafe_notification_daemon_SOURCES = \
	$(cafe_notification_daemon_built_sources) \
	daemon.c \
	daemon.h \
	engines.c \
	engines.h \
	stack.c \
	stack.h \
	sound.c \
	sound.h \
	mnd-daemon.c

cafe_notification_daemon_LDADD = $(NOTIFICATION_DAEMON_LIBS)

cafe_notification_daemon_CFLAGS = $(WARN_CFLAGS)

cafe_notification_daemon_built_sources =	\
	mnd-dbus-generated.h	\
	mnd-dbus-generated.c	\
	$(NULL)

$(cafe_notification_daemon_built_sources) : Makefile.am $(srcdir)/notificationdaemon.xml
	$(AM_V_GEN) gdbus-codegen \
		--interface-prefix org.freedesktop. \
		--c-namespace NotifyDaemon \
		--generate-c-code mnd-dbus-generated \
		$(srcdir)/notificationdaemon.xml

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	$(NOTIFICATION_DAEMON_CFLAGS) \
	-DENGINES_DIR=\"$(libdir)/cafe-notification-daemon/engines\"

EXTRA_DIST = notificationdaemon.xml
DISTCLEANFILES = \
	$(cafe_notification_daemon_built_sources)

-include $(top_srcdir)/git.mk
