#!/bin/sh

while read exclude
do
    excludes="$excludes $exclude"
done < FULL_EXCLUDE_LIST

~/local-svn/bin/svndumpfilter exclude --drop-empty-revs --renumber-revs $excludes \
        < TOOLKIT_DUMP \
        > TOOLKIT_DUMP-FILT \
        2> DUMP_ERRORS
