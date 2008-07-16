#!/bin/sh

xgettext --keyword=_ -C -s -o template.pot `sed 's/\\\\$//' POTFILES`

for FILE in `dirname $0`/*.po
do
    if test ! -L "$FILE"
    then
        msgmerge -q -s -U $FILE template.pot
    fi
done
