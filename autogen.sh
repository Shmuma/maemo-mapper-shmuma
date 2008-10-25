#!/bin/sh

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
 
set -x
glib-gettextize --copy --force
libtoolize --automake --copy --force
aclocal
intltoolize --automake --copy --force
autoconf --force
autoheader --force
automake --add-missing --copy --force-missing --foreign
