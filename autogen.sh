#!/bin/sh

set -x
aclocal-1.7
autoconf
libtoolize
automake-1.7 --add-missing --foreign
