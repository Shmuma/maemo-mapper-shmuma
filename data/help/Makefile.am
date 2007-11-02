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
 
helpfile_enus_DATA = en_US/maemomapper.xml
helpfile_engb_DATA = en_GB/maemomapper.xml
helpfile_fifi_DATA = fi_FI/maemomapper.xml

EXTRA_DIST = $(helpfile_engb_DATA) $(helpfile_engb_DATA) $(helpfile_fifi_DATA)

# Replace version in help files
%.xml: %.xml.in
	sed "s/__VERSION__/$(VERSION)/g" $< >$@

