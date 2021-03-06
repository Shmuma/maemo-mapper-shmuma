#!/bin/sh
#
# This little script builds an appropriately-versioned .deb file depending
# on your exact architecture (as defined in CFLAGS).
# On an N800 (-mcpu=arm1136jf-s), the version is suffixed with "-3".
# On a 770 (-mcpu=arm926ej-s), the version is suffixed with "-2".
# In all other cases, the version is suffixed with "-1".
# 

CHANGELOG=`dirname $0`/debian/changelog

# Test whether we're running Chinook or a "legacy" Maemo.
MAEMO_VERSION=`pkg-config libosso --modversion | cut -d . -f 1`
if [ "x$MAEMO_VERSION" = "x1" ]
then
    VERSION_SUFFIX=-os2006-os2007
else
    VERSION_SUFFIX=-os2008
fi

sed -i "1,1s/(\([0-9.][0-9.]*\)[-a-zA-Z0-9]*)/(\1$VERSION_SUFFIX)/" $CHANGELOG

dpkg-buildpackage -rfakeroot -i -sa

sed -i "1,1s/(\([0-9.][0-9.]*\)[-a-zA-Z0-9]*)/(\1)/" $CHANGELOG
