#!/bin/bash

if [ $# -lt 7 ] || [ $# -gt 8 ]; then
    echo "Usage: $0 REQUESTS ITERATIONS OLD_BINARY NEW_BINARY USER_THREADS IO_THREADS INPUT_FILE [OUTPUT_SUBDIR]";
    exit 1;
fi

REQUESTS=$1
ITERATIONS=$2
OLD_BINARY=$3
NEW_BINARY=$4
USER_THREADS=$5
IO_THREADS=$6
INPUT_FILE=$7
OUTPUT_SUBDIR=$8
TYPE=$(basename $INPUT_FILE .json)
OUTPUT_DIR="$NEW_BINARY-$OLD_BINARY/$USER_THREADS-$IO_THREADS/$TYPE/$OUTPUT_SUBDIR"

if [ ! -x $OLD_BINARY/psg_client ]; then
    echo "$OLD_BINARY/psg_client does not exist or is not executable"
    exit 2;
fi

if [ ! -x $NEW_BINARY/psg_client ]; then
    echo "$NEW_BINARY/psg_client does not exist or is not executable"
    exit 3;
fi

if [ ! -f $INPUT_FILE ]; then
    echo "$INPUT_FILE does not exist or is not a file"
    exit 4;
fi

if [ -a $OUTPUT_DIR ]; then
    echo "$OUTPUT_DIR already exists"
    exit 5;
else
    mkdir -p $OUTPUT_DIR
fi

for i in $(seq 1 $ITERATIONS); do
    for bin in $OLD_BINARY $NEW_BINARY; do
        sleep 0.25;

        $bin/psg_client performance -service psg -user-threads $USER_THREADS -io-threads $IO_THREADS -local-queue -use-cache yes -raw-metrics < <(head -$REQUESTS $INPUT_FILE);

        sleep 0.25;

        # Old binaries print named events, so need to replace names
        sed -i '{ s/Start/0/; s/Submit/1/; s/Reply/2/; s/Send/1000/; s/Receive/1001/; s/Close/1002/; s/Retry/1003/; s/Fail/1004/; s/Done/3/; }' psg_client.raw.txt

        # Need to rename request numbers to avoid overlap
        awk "{ \$1 += ($i - 1) * $REQUESTS; print }" psg_client.raw.txt >> $OUTPUT_DIR/raw.${bin}.txt;

        # Old binaries create complex metrics on performance command, so need to remove both files
        rm psg_client.raw.txt psg_client.table.txt 2>/dev/null;
    done;
done;

for bin in $OLD_BINARY $NEW_BINARY; do
    # Old binaries do not have report command, so need to use new binary
    $NEW_BINARY/psg_client report -input-file $OUTPUT_DIR/raw.${bin}.txt -output-file $OUTPUT_DIR/table.${bin}.txt;
done;

echo "vim -R -O -c ':windo :set nowrap' -c ':windo normal ggG' -c ':windo :set scb' $OUTPUT_DIR/table.*"
