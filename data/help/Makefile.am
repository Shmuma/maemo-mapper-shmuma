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
 
helpfile_enus_DATA = en_US/maemomapper.xml
helpfile_engb_DATA = en_GB/maemomapper.xml

EXTRA_DIST = $(helpfile_engb_DATA) $(helpfile_engb_DATA)

# Replace version in help files
%.xml: %.xml.in
	sed "s/__VERSION__/$(VERSION)/g" $< >$@

