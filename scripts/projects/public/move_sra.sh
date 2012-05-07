#!/bin/bash

LOCAL=$1
VERSION=$2

BASE_URL="https://svn.ncbi.nlm.nih.gov/viewvc/toolkit/release/public/$VERSION/c++"
BASE_DIR="c++"

if [ "$LOCAL"=="True" ]; then

    mv $BASE_DIR/include/internal/sra $BASE_DIR/include
    mv $BASE_DIR/src/internal/sra $BASE_DIR/src
    
    rm -rf $BASE_DIR/{include,src}/internal
    
else

    svn mv $BASE_URL/include/internal/sra $BASE_URL/include -m'public release internal/include/sra moved to include/sra;NOJIRA'
    svn mv $BASE_URL/src/internal/sra $BASE_URL/src -m'public release internal/src/sra moved to src/sra;NOJIRA'
    
    svn rm $BASE_URL/{include,src}/internal -m'internal/*/sra removed from public release;NOJIRA'
    
fi

exit 0