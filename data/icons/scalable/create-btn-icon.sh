#!/bin/bash

# This script merges a PNG icon representing an action with the btn-background
# image; this create the final icon that will be used as a button on the map.
#
# Example usage:
# ./create-btn-icon.sh orig/*.png
#
# This script makes use of the "composite" tool, which in ubuntu can be found
# as part of the imagemagick package.

for i in "$@"
do
    composite -gravity center "$i" btn-background.png "$(basename "$i")"
done

