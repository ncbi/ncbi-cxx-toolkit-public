#!/bin/bash

LOCAL=$1
VERSION=$2

BASE_URL="https://svn.ncbi.nlm.nih.gov/repos/toolkit/release/public/$VERSION/c++"
BASE_DIR="c++"

if [ "$LOCAL" == "True" ]; then

    mv -v $BASE_DIR/include/internal/sra $BASE_DIR/include || exit -1
    mv -v $BASE_DIR/src/internal/sra $BASE_DIR/src || exit -1
    
    rm -rf $BASE_DIR/{include,src}/internal || exit -1
    
else

    echo 'moving sra component from internal moved to base directory'

    echo "svn mv $BASE_URL/include/internal/sra $BASE_URL/include"
    svn mv $BASE_URL/include/internal/sra $BASE_URL/include -m'public release internal/include/sra moved to include/sra;NOJIRA' || exit -1
    
    echo "svn mv $BASE_URL/src/internal/sra $BASE_URL/src"
    svn mv $BASE_URL/src/internal/sra $BASE_URL/src -m'public release internal/src/sra moved to src/sra;NOJIRA' || exit -1
    
    echo "svn rm $BASE_URL/{include,src}/internal"
    svn rm $BASE_URL/{include,src}/internal -m'internal/*/sra removed from public release;NOJIRA' || exit -1
    
fi

exit 0