#
# Copyright (C) 2006, 2007 John Costigan.
#
# This file is part of Maemo Mapper.
#
# Maemo Mapper is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# Maemo Mapper is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Maemo Mapper.  If not, see <http://www.gnu.org/licenses/>.
#

AC_INIT(Makefile.am)
AM_INIT_AUTOMAKE(maemo-mapper, 3.0+alpha3)

AC_PROG_CPP
AC_CONFIG_HEADERS(src/config.h)
AC_PROG_INSTALL
AC_PROG_LIBTOOL

PKG_CHECK_MODULES(GTK, gtk+-2.0)
AC_SUBST(GTK_LIBS)
AC_SUBST(GTK_CFLAGS)

PKG_CHECK_MODULES(GNOME_VFS, gnome-vfs-2.0 >= 2.8.4.11 gnome-vfs-module-2.0 >= 2.8.4.11)
AC_SUBST(GNOME_VFS_LIBS)
AC_SUBST(GNOME_VFS_CFLAGS)

PKG_CHECK_MODULES(GCONF, gconf-2.0 >= 2.6.2)
AC_SUBST(GCONF_LIBS)
AC_SUBST(GCONF_CFLAGS)

PKG_CHECK_MODULES(LIBXML2, libxml-2.0 >= 2.6.16)
AC_SUBST(LIBXML2_LIBS)
AC_SUBST(LIBXML2_CFLAGS)

PKG_CHECK_MODULES(SQLITE, sqlite3)
AC_SUBST(SQLITE_LIBS)
AC_SUBST(SQLITE_CFLAGS)

PKG_CHECK_MODULES(CONIC, conic, AC_DEFINE(CONIC, 1, Use the conic library), AC_MSG_WARN(conic not found. Compiling without conic support.))
AC_SUBST(CONIC_LIBS)
AC_SUBST(CONIC_CFLAGS)

AC_CHECK_HEADER(gdbm.h, , AC_MSG_ERROR(gdbm.h not found. Please install libgdbm-dev.))
AC_CHECK_LIB(gdbm, gdbm_open, , AC_MSG_ERROR(libgdbm.so not found. Please install libgdbm.), -ldbus-glib-1 -ldbus-1 -lgobject-2.0 -lglib-2.0)

AC_CHECK_HEADER(gpsmgr.h, , AC_MSG_WARN(gpsmgr.h not found. Compiling without gpsmgr support.))
AC_CHECK_LIB(gpsmgr, gpsmgr_start, , AC_MSG_WARN(libgpsmgr.so not found. Compiling without gpsmgr support.), -ldbus-glib-1 -ldbus-1 -lgobject-2.0 -lglib-2.0)

AC_ARG_ENABLE(legacy, 
        AS_HELP_STRING([--enable-legacy],[Enable building for Chinook]),
        [case "${enableval}" in
                yes) HAVE_LEGACY=yes ;;
                no)  HAVE_LEGACY=no ;;
                *) AC_MSG_ERROR(bad value ${enableval} for --enable-legacy) ;;
        esac],
        [HAVE_LEGACY=no])

AS_IF([test "x$HAVE_LEGACY" = xyes], 
        [
                PKG_CHECK_MODULES(OSSO, libosso >= 1 libossohelp)
                PKG_CHECK_MODULES(HILDON, hildon-libs >= 0.9.50 hildon-fm)
                AC_DEFINE(LEGACY, 1, Build for OS2006/OS2007)
        ],
        [
                PKG_CHECK_MODULES(OSSO, libosso >= 1)
                PKG_CHECK_MODULES(HILDON, hildon-1 hildon-fm-2)
                PKG_CHECK_MODULES(LOCATION, liblocation)
        ])

AC_SUBST(OSSO_LIBS)
AC_SUBST(OSSO_CFLAGS)

AC_SUBST(HILDON_LIBS)
AC_SUBST(HILDON_CFLAGS)

AC_SUBST(LOCATION_LIBS)
AC_SUBST(LOCATION_CFLAGS)

PKG_CHECK_MODULES(CLUTTER, clutter-1.0 clutter-gtk-0.10)
AC_SUBST(CLUTTER_LIBS)
AC_SUBST(CLUTTER_CFLAGS)

PKG_CHECK_MODULES(CAIRO, cairo)
AC_SUBST(CAIRO_LIBS)
AC_SUBST(CAIRO_CFLAGS)

# Localisation
localedir=`$PKG_CONFIG osso-af-settings --variable=localedir`
AC_PROG_INTLTOOL([0.23])
GETTEXT_PACKAGE=$PACKAGE
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], "${GETTEXT_PACKAGE}", [Name of gettext package])
ALL_LINGUAS="bg_BG de_DE en_US en_GB es_ES fi_FI it_IT nl_NL sv_SE ru_RU"
AM_GLIB_GNU_GETTEXT


# To make application visible in maemo Task Navigator it needs a Desktop
# file for the application.
# D-BUS service file is needed to be able to launch the maemo application
# and connect it to D-BUS services.
# The following line defines install directories for these files.
desktopentrydir=`$PKG_CONFIG osso-af-settings --variable=desktopentrydir`
serviceentrydir=`$PKG_CONFIG osso-af-settings --variable=dbusservicedir`

# Application pixmaps install directory
pixmapdir=`$PKG_CONFIG osso-af-settings --variable=hildonpixmapdir`

# Application icon install directories
icon_26x26dir=$datadir/icons/hicolor/26x26/hildon
icon_34x34dir=$datadir/icons/hicolor/34x34/hildon
icon_40x40dir=$datadir/icons/hicolor/40x40/hildon
icon_50x50dir=$datadir/icons/hicolor/50x50/hildon
icon_scalabledir=$datadir/icons/hicolor/scalable/hildon
icon_aprsdir=$datadir/icons/hicolor/scalable/hildon

# Help file install directory
helpfile_enusdir=$datadir/osso-help/en_US
AC_SUBST(helpfile_enusdir)
helpfile_engbdir=$datadir/osso-help/en_GB
AC_SUBST(helpfile_engbdir)
helpfile_fifidir=$datadir/osso-help/fi_FI
AC_SUBST(helpfile_fifidir)

# Hildon control panel plugin install directories
pluginlibdir=`$PKG_CONFIG hildon-control-panel --variable=plugindir`
plugindesktopentrydir=`$PKG_CONFIG hildon-control-panel --variable=plugindesktopentrydir`

# Define as variables in Makefiles
AC_SUBST(desktopentrydir)
AC_SUBST(serviceentrydir)
AC_SUBST(localedir)
AC_SUBST(pixmapdir)
AC_SUBST(icon_26x26dir)
AC_SUBST(icon_34x34dir)
AC_SUBST(icon_40x40dir)
AC_SUBST(icon_50x50dir)
AC_SUBST(icon_aprsdir)
AC_SUBST(icon_scalabledir)
AC_SUBST(pluginlibdir)
AC_SUBST(plugindesktopentrydir)

AC_DEFINE_UNQUOTED([LOCALEDIR], "${localedir}", [Runtime locale catalog files path])
AC_DEFINE_UNQUOTED([PIXMAPDIR], "${pixmapdir}", [Runtime pixmap files path])


# Produce output files
AC_OUTPUT(
          Makefile \
          src/Makefile \
          data/Makefile \
          data/help/Makefile \
          po/Makefile.in \
          po/Makefile
)

