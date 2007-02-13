#!/bin/sh
#
# This little script builds an appropriately-versioned .deb file depending
# on your exact architecture (as defined in CFLAGS).
# On an N800 (-mcpu=arm1136jf-s), the version is suffixed with "-3".
# On a 770 (-mcpu=arm926ej-s), the version is suffixed with "-2".
# In all other cases, the version is suffixed with "-1".
# 

CHANGELOG=`dirname $0`/debian/changelog

case $CFLAGS in
    *-mcpu=arm1136jf-s*)
    VERSION_SUFFIX=-3
    DBP_EXTRA=-B
    ;;
    *-mcpu=arm926ej-s*)
    VERSION_SUFFIX=-2
    DBP_EXTRA=-B
    ;;
    *)
    VERSION_SUFFIX=-1
    DBP_EXTRA=
esac

head -n 1 $CHANGELOG | sed "s/)/$VERSION_SUFFIX)/" > $CHANGELOG.new
awk 'NR>1 {print}' $CHANGELOG >> $CHANGELOG.new
mv $CHANGELOG $CHANGELOG.old
mv $CHANGELOG.new $CHANGELOG

dpkg-buildpackage -rfakeroot -I.svn $DBP_EXTRA

mv $CHANGELOG.old $CHANGELOG
