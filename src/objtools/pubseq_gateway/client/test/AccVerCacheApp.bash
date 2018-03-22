#!/bin/bash

if [ $# -lt 1 ]; then
    echo "Usage: $0 HOST:PORT";
    exit 1;
fi

DIR=$(dirname $0)
BINARY=$(basename $0 .bash)
EXPECTED=$BINARY.txt
SERVER=$1
shift

if [ $# -lt 1 ]; then
    ARGS="-t 8";
else
    ARGS=$@
fi

if [ ! -x $DIR/$BINARY ]; then
    echo "Binary '$BINARY' is missing"
    exit 2;
fi

if [ ! -f $DIR/$EXPECTED ]; then
    echo "File with expected results '$EXPECTED' is missing"
    exit 2;
fi

diff -su $DIR/$EXPECTED <($DIR/$BINARY -fa $DIR/$EXPECTED -H $SERVER $ARGS |sort)
