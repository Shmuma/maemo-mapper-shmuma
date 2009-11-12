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
 
servicedir = $(datadir)/dbus-1/services
service_DATA = com.gnuite.maemo_mapper.service

desktopdir = $(datadir)/applications/hildon
desktop_DATA = maemo-mapper.desktop

icon_26x26_DATA = icons/26x26/maemo-mapper.png 
icon_40x40_DATA = icons/40x40/maemo-mapper.png
icon_scalable_DATA = icons/scalable/maemo-mapper.png icons/scalable/maemo-mapper-wait.png \
	icons/scalable/maemo-mapper-point.png \
	icons/scalable/maemo-mapper-fullscreen.png \
	icons/scalable/maemo-mapper-gps-enable.png \
	icons/scalable/maemo-mapper-gps-disable.png \
	icons/scalable/maemo-mapper-path.png \
	icons/scalable/maemo-mapper-rotate.png \
	icons/scalable/maemo-mapper-route.png \
	icons/scalable/maemo-mapper-settings.png \
	icons/scalable/maemo-mapper-zoom-in.png \
	icons/scalable/maemo-mapper-zoom-out.png
icon_svg = \
	icons/scalable/svg/maemo-mapper-point.svg \
	icons/scalable/svg/maemo-mapper-fullscreen.svg \
	icons/scalable/svg/maemo-mapper-path.svg \
	icons/scalable/svg/maemo-mapper-settings.svg \
	icons/scalable/svg/maemo-mapper-rotate.svg \
	icons/scalable/svg/maemo-mapper-route.svg \
	icons/scalable/svg/maemo-mapper-zoom-in.svg \
	icons/scalable/svg/maemo-mapper-zoom-out.svg
icon_aprs_DATA = icons/aprs/allicons.png icons/aprs/allicon2.png  
EXTRA_DIST = $(serviceentry_DATA) $(desktopentry_DATA) $(icon_26x26_DATA) $(icon_40x40_DATA) $(icon_scalable_DATA) $(icon_aprs_DATA) \
	$(icon_svg)

SUBDIRS = help
