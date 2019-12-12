#!/bin/bash

if [ $# -lt 9 ]; then
    echo "Usage: $0 SERVICE REQUESTS ITERATIONS USER_THREADS IO_THREADS INPUT_FILE SKIP_FIRST BINARY BINARY [BINARY ...]";
    exit 1;
fi

SERVICE="$1"
REQUESTS="$2"
ITERATIONS="$3"
USER_THREADS="$4"
IO_THREADS="$5"
INPUT_FILE="$6"
SKIP_FIRST=$7
shift 7
BINARIES=($@)

function create_path
{
    local IFS="-"
    OUTPUT_PATH="$*/$SERVICE/$TYPE/$USER_THREADS-$IO_THREADS/$REQUESTS-$ITERATIONS-$SKIP_FIRST"
    mkdir -p "$OUTPUT_PATH"
}

function run
{
    if [ $SKIP_FIRST -eq 0 ]; then
        head -$REQUESTS $INPUT_FILE |$1/psg_client performance -service $SERVICE -user-threads $USER_THREADS -io-threads $IO_THREADS -local-queue -use-cache yes -raw-metrics;
    else
        SKIP_FIRST=$(($REQUESTS+$SKIP_FIRST));
        head -$SKIP_FIRST $INPUT_FILE |tail -$REQUESTS |$1/psg_client performance -service $SERVICE -user-threads $USER_THREADS -io-threads $IO_THREADS -local-queue -use-cache yes -raw-metrics -delay 1;
    fi
}

TYPE=$(basename $INPUT_FILE .json)

for bin in "${BINARIES[@]}"; do
    if [ ! -x $bin/psg_client ]; then
        echo "$bin/psg_client does not exist or is not executable"
        exit 2;
    fi;
done

if [ ! -f $INPUT_FILE ]; then
    echo "$INPUT_FILE does not exist or is not a file"
    exit 3;
fi

create_path ${BINARIES[@]}

for ((i = 1; 1000; ++i)); do
    OUTPUT_DIR="$OUTPUT_PATH/$i"

    if [ ! -e "$OUTPUT_DIR" ]; then
        mkdir "$OUTPUT_DIR"
        break;
    fi;
done

if [ $SKIP_FIRST -eq 0 ]; then
    # A warm-up
    run ${BINARIES[0]};
fi

for ((i = 1; $i <= $ITERATIONS; ++i)); do
    for bin in "${BINARIES[@]}"; do
        sleep 0.25;
        run $bin
        sleep 0.25;

        # Old binaries print named events, so need to replace names
        sed -i '{ s/Start/0/; s/Submit/1/; s/Reply/2/; s/Send/1000/; s/Receive/1001/; s/Close/1002/; s/Retry/1003/; s/Fail/1004/; s/Done/3/; }' psg_client.raw.txt

        # Need to rename request numbers to avoid overlap
        awk "{ \$1 += ($i - 1) * $REQUESTS; print }" psg_client.raw.txt >> $OUTPUT_DIR/raw.${bin}.txt;

        # Old binaries create complex metrics on performance command, so need to remove both files
        rm psg_client.raw.txt psg_client.table.txt 2>/dev/null;
    done;
done;

for bin in "${BINARIES[@]}"; do
    # Old binaries do not have report command, so need to use new binary
    ${BINARIES[0]}/psg_client report -input-file $OUTPUT_DIR/raw.${bin}.txt -output-file $OUTPUT_DIR/table.${bin}.txt;
done;

echo "vim -R -O -c ':windo :set nowrap' -c ':windo normal ggG' -c ':windo :set scb' $OUTPUT_DIR/table.*"
