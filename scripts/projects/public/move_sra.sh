#!/bin/bash

LOCAL=$1
VERSION=$2

BASE_URL="https://svn.ncbi.nlm.nih.gov/repos/toolkit/release/public/$VERSION/c++"
BASE_DIR="c++"

echo 'moving sra component from internal to base directory of public release project'

if [ "$LOCAL" == "True" ]; then

    mv -v $BASE_DIR/include/internal/sra $BASE_DIR/include || exit -1
    mv -v $BASE_DIR/src/internal/sra $BASE_DIR/src || exit -1
    
    rm -rf $BASE_DIR/{include,src}/internal || exit -1
    
else

    echo "svn mv $BASE_URL/include/internal/sra $BASE_URL/include"
    svn mv $BASE_URL/include/internal/sra $BASE_URL/include -m'public release internal/include/sra moved to include/sra;NOJIRA' || exit -1
    
    echo "svn mv $BASE_URL/src/internal/sra $BASE_URL/src"
    svn mv $BASE_URL/src/internal/sra $BASE_URL/src -m'public release internal/src/sra moved to src/sra;NOJIRA' || exit -1
    
    echo "svn rm $BASE_URL/{include,src}/internal"
    svn rm $BASE_URL/{include,src}/internal -m'internal/*/sra removed from public release;NOJIRA' || exit -1
    
    TMP_DIR=`mktemp -d`
    
    svn checkout $BASE_URL/src/sra $TMP_DIR/sra --depth 'empty'
    
    echo "freezing svn:externals for $BASE_URL/src/sra"
    
    IFS=$'\n'

    for EXT_DEF in `svn pg svn:externals $TMP_DIR/sra`; do
    
	EXT_NAME=`echo $EXT_DEF | sed -n 's/\(.*[^ ]\) *\(-r\|http[s]*\).*/\1/p'`
	EXT_REV=`echo $EXT_DEF | sed -n 's/.* -r\([0-9]*\).*/\1/p'`
	EXT_URL=`echo $EXT_DEF | sed -n 's/.*\(http[s]*:\/\/.*\)/\1/p'`
	
	[ -n "$EXT_REV" ] || EXT_REV=`svn info $EXT_URL | grep 'Revision: ' | sed 's/Revision: //'`
	
	echo $EXT_NAME -r$EXT_REV $EXT_URL
    
    done | svn ps svn:externals -F - $TMP_DIR/sra || exit -1
    
    svn ci $TMP_DIR/sra -m'sra project externals frozen'
    
    rm -rf $TMP_DIR
    
fi

exit 0
