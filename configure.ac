dnl Process this file with autoconf to create configure.

dnl ################################################################
dnl # Initialize autoconf
dnl ################################################################
AC_INIT([cafe-notification-daemon], [1.25.0], [https://cafe-desktop.org])
AC_PREREQ(2.63)

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_MACRO_DIR([m4])

dnl ################################################################
dnl # Version information
dnl ################################################################
NOTIFICATION_DAEMON_MAJOR_VERSION=1
NOTIFICATION_DAEMON_MINOR_VERSION=3
NOTIFICATION_DAEMON_MICRO_VERSION=0
NOTIFICATION_DAEMON_DEVEL_VERSION=0

NOTIFICATION_DAEMON_VERSION=$NOTIFICATION_DAEMON_MAJOR_VERSION.$NOTIFICATION_DAEMON_MINOR_VERSION.$NOTIFICATION_DAEMON_MICRO_VERSION

if test "x$NOTIFICATION_DAEMON_DEVEL_VERSION" != "x0"; then
	if test "x$NOTIFICATION_DAEMON_MICRO_VERSION" = "x0"; then
		NOTIFICATION_DAEMON_MICRO_VERSION=$NOTIFICATION_DAEMON_DEVEL_VERSION
		NOTIFICATION_DAEMON_VERSION=$NOTIFICATION_DAEMON_MAJOR_VERSION.$NOTIFICATION_DAEMON_MINOR_VERSION.$NOTIFICATION_DAEMON_DEVEL_VERSION
	else
		NOTIFICATION_DAEMON_VERSION=$NOTIFICATION_DAEMON_VERSION.$NOTIFICATION_DAEMON_DEVEL_VERSION
	fi
fi

AC_DEFINE_UNQUOTED(NOTIFICATION_DAEMON_MAJOR_VERSION, $NOTIFICATION_DAEMON_MAJOR_VERSION,
	[cafe-notification-daemon major version.])
AC_DEFINE_UNQUOTED(NOTIFICATION_DAEMON_MINOR_VERSION, $NOTIFICATION_DAEMON_MINOR_VERSION,
	[cafe-notification-daemon minor version.])
AC_DEFINE_UNQUOTED(NOTIFICATION_DAEMON_MICRO_VERSION, $NOTIFICATION_DAEMON_MICRO_VERSION,
	[cafe-notification-daemon micro version.])
AC_DEFINE_UNQUOTED(NOTIFICATION_DAEMON_VERSION, "$NOTIFICATION_DAEMON_VERSION",
	[cafe-notification-daemon version.])

dnl ################################################################
dnl # Initialize automake
dnl ################################################################
AM_INIT_AUTOMAKE([1.10 no-dist-gzip dist-xz foreign check-news])
AM_MAINTAINER_MODE
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

dnl ################################################################
dnl # Check for some standard stuff.
dnl ################################################################
# Checks for programs
AC_PROG_CC
AC_PROG_CPP
AC_PROG_MAKE_SET
AC_PROG_LN_S
AC_PROG_INSTALL

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T

dnl ---------------------------------------------------------------------------
dnl Warning flags
dnl ---------------------------------------------------------------------------
CAFE_MAINTAINER_MODE_DEFINES
CAFE_COMPILE_WARNINGS([yes])

dnl ---------------------------------------------------------------------------
dnl Initialize Libtool
dnl ---------------------------------------------------------------------------
LT_PREREQ([2.2.6])
LT_INIT


dnl ---------------------------------------------------------------------------
dnl i18n stuff
dnl ---------------------------------------------------------------------------

AM_GNU_GETTEXT_VERSION([0.19.8])
AM_GNU_GETTEXT_REQUIRE_VERSION([0.19.8])
AM_GNU_GETTEXT([external])

AC_SUBST([GETTEXT_PACKAGE], [cafe-notification-daemon])
AC_DEFINE([GETTEXT_PACKAGE], [PACKAGE_TARNAME], [Define to the gettext package name.])


dnl ---------------------------------------------------------------------------
dnl GLIB_COMPILE_RESOURCES
dnl ---------------------------------------------------------------------------
AC_ARG_VAR([GLIB_COMPILE_RESOURCES],[glib-compile-resources bin])
AC_PATH_PROG([GLIB_COMPILE_RESOURCES],[glib-compile-resources],[])
if test -z "$GLIB_COMPILE_RESOURCES"; then
  AC_MSG_ERROR([glib-compile-resources not found])
fi

dnl ---------------------------------------------------------------------------
dnl XMLLINT
dnl ---------------------------------------------------------------------------
AC_ARG_VAR([XMLLINT],[xmllint bin])
AC_PATH_PROG([XMLLINT],[xmllint],[])
if test -z "$XMLLINT"; then
  AC_MSG_ERROR([xmllint not found])
fi

dnl ---------------------------------------------------------------------------
dnl Requirements for the themes
dnl ---------------------------------------------------------------------------

CTK_REQUIRED=3.22.0
XML_REQUIRED=2.9.0

PKG_CHECK_MODULES(THEME,
  ctk+-3.0 >= $CTK_REQUIRED
  libxml-2.0 >= $XML_REQUIRED
)
AC_SUBST(THEME_CFLAGS)
AC_SUBST(THEME_LIBS)

dnl ---------------------------------------------------------------------------
dnl Requirements for the daemon
dnl ---------------------------------------------------------------------------
REQ_GLIB_VERSION=2.50.0
REQ_LIBKANBERRA_CTK_VERSION=0.4
PKG_CHECK_MODULES(GMODULE,gmodule-2.0,[GMODULE_ADD="gmodule-2.0"],[GMODULE_ADD=""])
pkg_modules="
	ctk+-3.0 >= $CTK_REQUIRED, \
	glib-2.0 >= $REQ_GLIB_VERSION, \
	gio-2.0 >= $REQ_GLIB_VERSION, \
        $GMODULE_ADD \
        libkanberra-ctk3 >= $REQ_LIBKANBERRA_CTK_VERSION, \
	libvnck-3.0 \
        x11 \
"
PKG_CHECK_MODULES(NOTIFICATION_DAEMON, $pkg_modules)
AC_SUBST(NOTIFICATION_DAEMON_CFLAGS)
AC_SUBST(NOTIFICATION_DAEMON_LIBS)

dnl ---------------------------------------------------------------------------
dnl Requirements for the setup tool
dnl ---------------------------------------------------------------------------
PKG_CHECK_MODULES(NOTIFICATION_CAPPLET, glib-2.0 >= $REQ_GLIB_VERSION gio-2.0 >= $REQ_GLIB_VERSION ctk+-3.0 >= $CTK_REQUIRED libnotify)
AC_SUBST(NOTIFICATION_CAPPLET_CFLAGS)
AC_SUBST(NOTIFICATION_CAPPLET_LIBS)

cdk_modules="
	cdk-3.0 >= $CTK_REQUIRED, \
	gdk-pixbuf-2.0 >= 2.22.0 \
"
PKG_CHECK_MODULES(CDK, $cdk_modules, have_cdk=yes,
[
	have_cdk=no
	AC_MSG_WARN("Some test apps will not be built")
])

AM_CONDITIONAL(HAVE_CDK, test "x$have_cdk" = "xyes")

AC_SUBST(CDK_CFLAGS)
AC_SUBST(CDK_LIBS)

dnl
dnl Enable gsettings schema macros
dnl

GLIB_GSETTINGS

dnl
dnl Check the D-BUS version.
dnl

AC_MSG_CHECKING([dbus version])
DBUS_VERSION=`$PKG_CONFIG --modversion dbus-1`
DBUS_MAJOR_VER=`echo $DBUS_VERSION | cut -d. -f 1`
DBUS_MINOR_VER=`echo $DBUS_VERSION | cut -d. -f 2`
DBUS_MICRO_VER=`echo $DBUS_VERSION | cut -d. -f 3`

AC_MSG_RESULT($DBUS_VERSION)
AC_DEFINE_UNQUOTED(DBUS_MAJOR_VER, $DBUS_MAJOR_VER, [D-BUS major version.])
AC_DEFINE_UNQUOTED(DBUS_MINOR_VER, $DBUS_MINOR_VER, [D-BUS minor version.])
AC_DEFINE_UNQUOTED(DBUS_MICRO_VER, $DBUS_MICRO_VER, [D-BUS micro version.])

AC_DEFINE_UNQUOTED(DBUS_API_SUBJECT_TO_CHANGE, ,DBUS API is subject to change)

AS_AC_EXPAND(SYSCONFDIR, $sysconfdir)
AS_AC_EXPAND(LIBDIR, $libdir)
AS_AC_EXPAND(DATADIR, $datadir)
AS_AC_EXPAND(LIBEXECDIR, $libexecdir)

AC_ARG_WITH(dbus-sys, [  --with-dbus-sys=<dir>   where D-BUS system.d directory is])
AC_ARG_WITH(dbus-services, [  --with-dbus-services=<dir>   where D-BUS services directory is])

if ! test -z "$with_dbus_sys" ; then
	DBUS_SYS_DIR="$with_dbus_sys"
else
	DBUS_SYS_DIR="$SYSCONFDIR/dbus-1/system.d"
fi

if ! test -z "$with_dbus_services" ; then
	DBUS_SERVICES_DIR="$with_dbus_services"
else
	DBUS_SERVICES_DIR="$DATADIR/dbus-1/services"
fi

AC_SUBST(DBUS_SYS_DIR)
AC_SUBST(DBUS_SERVICES_DIR)
AC_DEFINE_UNQUOTED(DBUS_SYSTEMD_DIR, "$DBUS_SYS_DIR", [Where system.d dir for DBUS is])
AC_DEFINE_UNQUOTED(DBUS_SERVICES_DIR, "$DBUS_SERVICES_DIR", [Where services dir for DBUS is])

dnl ---------------------------------------------------------------------------
dnl Enabel/disable icon cache generation
dnl ---------------------------------------------------------------------------
AC_ARG_ENABLE(icon-update, AS_HELP_STRING([--disable-icon-update],
                                        [Disable icon cache update]))
if (test "$enable_icon_update" != no); then
        AC_PATH_PROG(UPDATE_ICON_CACHE, [ctk-update-icon-cache])
fi
AM_CONDITIONAL([ICON_UPDATE], [test -n "$UPDATE_ICON_CACHE"])

AC_CONFIG_FILES([
Makefile
data/Makefile
data/org.freedesktop.cafe.Notifications.service
data/org.cafe.NotificationDaemon.gschema.xml
po/Makefile.in
src/Makefile
src/daemon/Makefile
src/capplet/Makefile
src/themes/Makefile
src/themes/coco/Makefile
src/themes/nodoka/Makefile
src/themes/slider/Makefile
src/themes/standard/Makefile
])

AC_OUTPUT

echo "
			$PACKAGE_NAME $PACKAGE_VERSION
			================================

	prefix:                   ${prefix}
	exec_prefix:              ${exec_prefix}
	bindir:                   ${bindir}
	datadir:                  ${datadir}
	source code location:     ${srcdir}
	compiler:                 ${CC}
	cflags:	                  ${CFLAGS}
	warning flags:            ${WARN_CFLAGS}

	dbus-1 system.d           $DBUS_SYS_DIR
	dbus-1 services           $DBUS_SERVICES_DIR

"
