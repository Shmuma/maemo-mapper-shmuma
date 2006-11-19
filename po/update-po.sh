#!/bin/sh

xgettext --keyword=_ -C -s -o template.pot `dirname $0`/../src/maemo-mapper.c

for FILE in `dirname $0`/*.po
do
	msgmerge -q -s -U $FILE template.pot
done
