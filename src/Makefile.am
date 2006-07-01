#
# This file is part of maemo-mapper
#
# Copyright (C) 2006 John Costigan.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#

bin_PROGRAMS = maemo-mapper

maemo_mapper_CFLAGS = $(GTK_CFLAGS) $(OSSO_CFLAGS) $(HILDON_CFLAGS) $(GNOME_VFS_CFLAGS) $(GCONF_CFLAGS) $(LIBXML2_CFLAGS)

maemo_mapper_LDADD = $(GTK_LIBS) $(OSSO_LIBS) $(HILDON_LIBS) $(GNOME_VFS_LIBS) $(GCONF_LIBS) $(LIBXML2_LIBS) -lbluetooth

maemo_mapper_SOURCES = maemo-mapper.c
