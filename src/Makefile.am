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

bin_PROGRAMS = maemo-mapper

maemo_mapper_CFLAGS = $(GTK_CFLAGS) $(OSSO_CFLAGS) $(HILDON_CFLAGS) $(GNOME_VFS_CFLAGS) $(GCONF_CFLAGS) $(LIBXML2_CFLAGS) $(SQLITE_CFLAGS) $(CONIC_CFLAGS) $(LOCATION_CFLAGS) $(CLUTTER_CFLAGS) $(CAIRO_CFLAGS)

maemo_mapper_LDADD = $(GTK_LIBS) $(OSSO_LIBS) $(HILDON_LIBS) $(GNOME_VFS_LIBS) $(GCONF_LIBS) $(LIBXML2_LIBS) $(SQLITE_LIBS) $(CONIC_LIBS) $(LOCATION_LIBS) $(CLUTTER_LIBS) $(CAIRO_LIBS)

maemo_mapper_SOURCES = \
	cmenu.c \
	controller.c \
	data.c \
	dbus-ifc.c \
	display.c \
	gdk-pixbuf-rotate.c \
	gps.c \
	gpsbt.c \
	gpx.c \
	input.c \
	main.c \
	maps.c \
	marshal.c \
	menu.c \
	osm.c \
	path.c \
	poi.c \
	settings.c \
	screen.c \
	tile.c \
	util.c \
	aprs_decode.c \
	aprs.c \
	hashtable.c \
	hashtable_itr.c \
	aprs_display.c \
	aprs_message.c \
	aprs_kiss.c

# bin_SCRIPTS = maemo-mapper.sh
