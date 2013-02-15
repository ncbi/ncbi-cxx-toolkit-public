#!/bin/bash

LOCAL=$1
VERSION=$2

SRC_URL="https://svn.ncbi.nlm.nih.gov/repos/toolkit/trunk/"
DST_URL="https://svn.ncbi.nlm.nih.gov/repos/toolkit/release/grid-python/$VERSION/"
DST_DIR="internal/scripts/common/lib/python/"
PROJ="ncbi"

if [ "$LOCAL" == "True" ]; then

    svn export $SRC_URL$DST_DIR$PROJ $DST_DIR$PROJ

else

    svn mkdir --parents $DST_URL$DST_DIR -m"grid-python prepare_release POSTTAG adjustments;NOJIRA"
    svn cp $SRC_URL$DST_DIR$PROJ $DST_URL$DST_DIR$PROJ -m"grid-python prepare_release POSTTAG adjustments;NOJIRA"
    
fi

exit 0
