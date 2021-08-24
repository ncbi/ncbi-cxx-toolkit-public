#!/bin/bash

if [ $# -lt 8 ] || [ $# -gt 9 ]; then
    echo "Usage: $0 COMMAND \"SERVICE...\" REQUESTS ITERATIONS INPUT_FILE \"IO_THREADS...\" \"REQUESTS_PER_IO...\" \"BINARY...\" [RATE]";
    exit 1;
fi

COMMAND="$1"
SERVICES=($2)
REQUESTS="$3"
ITERATIONS="$4"
INPUT_FILE="$5"
IO_THREADS=($6)
REQUESTS_PER_IO=($7)
BINARIES=($8)
RATE="$9"

case "$COMMAND" in
"resolve")
    CMD=1
    ;;
"interactive")
    CMD=2
    ;;
"performance")
    CMD=3
    ;;
*)
    echo "Unknown command: $COMMAND";
    exit 2
    ;;
esac

if [ -e psg_client.ini ]; then
    echo "psg_client.ini exists"
    exit 3;
fi

if [ ! -f $INPUT_FILE ]; then
    echo "$INPUT_FILE does not exist or is not a file"
    exit 4;
elif [ "$RATE" != "" ]; then
    read -r FILE_LINES FILE_SIZE REQUEST_LEN UNUSED <<<$(wc -clL $INPUT_FILE);
    if [ $(($FILE_SIZE / $FILE_LINES - $REQUEST_LEN)) -lt 1 ]; then
        echo "Cannot use RATE, $INPUT_FILE has variable length requests"
        exit 6;
    fi
fi

for binary in ${BINARIES[@]}; do
    if [ ! -x $binary/psg_client ]; then
        echo "$binary/psg_client does not exist or is not executable"
        exit 5;
    fi;
done

function create_path
{
    local IFS="-"
    local FILE=$(basename $INPUT_FILE)
    OUTPUT_PATH="$*/$COMMAND/$FILE/$REQUESTS-$ITERATIONS"
    if [ "$RATE" != "" ]; then
        OUTPUT_PATH="$OUTPUT_PATH-$RATE"
    fi
    mkdir -p "$OUTPUT_PATH"
}

function requests
{
    if [ "$RATE" == "" ]; then
        head -$REQUESTS $INPUT_FILE
    else
        head -$REQUESTS $INPUT_FILE |pv -L $(($RATE * (1 + $REQUEST_LEN))) 2>/dev/null
    fi
}

function run
{
    OUTPUT_FILE="$OUTPUT_DIR/raw.$1.$2.$3.$5.txt"
    echo -e "[PSG]\nuse_cache=yes\nnum_io=$1\nrequests_per_io=$2" > psg_client.ini

    if [ $CMD -eq 1 ]; then
        requests |/usr/bin/time -ao $OUTPUT_FILE -f "%e" $3/psg_client resolve -service $5 -worker-threads "$1" -id-file - >/dev/null;
        echo -n "."
    elif [ $CMD -eq 2 ]; then
        requests |/usr/bin/time -ao $OUTPUT_FILE -f "%e" $3/psg_client interactive -service $5 -worker-threads "$1" >/dev/null;
        echo -n "."
    else
        requests |$3/psg_client performance -service $5 -user-threads $1 -local-queue -raw-metrics >/dev/null;

        # Old binaries print named events, so need to replace names
        sed -i '{ s/Start/0/; s/Submit/1/; s/Reply/2/; s/Send/1000/; s/Receive/1001/; s/Close/1002/; s/Retry/1003/; s/Fail/1004/; s/Done/3/; }' psg_client.raw.txt

        # Need to rename request numbers to avoid overlap
        awk "{ \$1 = \$1 \".\" $4; print }" psg_client.raw.txt >> $OUTPUT_FILE;

        # Old binaries create complex metrics on performance command, so need to remove both files
        rm psg_client.raw.txt psg_client.table.txt 2>/dev/null;
    fi
}

function report
{
    local INPUT_FILE="$OUTPUT_DIR/raw.$2.$3.$4.$5.txt"
    local OUTPUT_FILE="$OUTPUT_DIR/pro.$2.$3.$4.$5.txt"

    if [ $CMD -eq 3 ]; then
        $1/psg_client report -input-file "$INPUT_FILE" -output-file "$OUTPUT_FILE";
    else
        sort -n "$INPUT_FILE" >> "$OUTPUT_FILE";
    fi
}

function aggregate
{
    local INPUT_FILE="$OUTPUT_DIR/pro.$2.$3.$4.$5.txt"

    (
        echo -n "$4 $2 $3 $5 ";

        if [ $CMD -eq 3 ]; then
            local MEDIAN=$(($REQUESTS * $ITERATIONS + 5))
            local AVERAGE=$(($REQUESTS * $ITERATIONS + 10))
            sed -n "{${MEDIAN}p;${AVERAGE}p}" "$INPUT_FILE" |awk '{print $2}' |xargs
        else
            local MEDIAN=$(($ITERATIONS / 2))
            awk "NR==$MEDIAN { m = \$1 } { t += \$1 } END { print m, t / NR }" "$INPUT_FILE"
        fi
    ) |sed 's/ /;/g'>> "$1"

    echo -n "."
}

create_path ${BINARIES[@]}

for ((i = 1; 1000; ++i)); do
    OUTPUT_DIR="$OUTPUT_PATH/$i"

    if [ ! -e "$OUTPUT_DIR" ]; then
        mkdir "$OUTPUT_DIR"
        break;
    fi;
done

fb=${BINARIES[0]}
fs=${SERVICES[0]}

# A warm-up
run 6 1 $fb 1 $fs ">/dev/null"
rm "$OUTPUT_FILE"

for ((i = 1; $i <= $ITERATIONS; ++i)); do
    for t in ${IO_THREADS[@]}; do
        for r in ${REQUESTS_PER_IO[@]}; do
            for b in ${BINARIES[@]}; do
                for s in ${SERVICES[@]}; do
                    run $t $r $b $i $s
                done
            done
        done
    done
done;

echo
rm psg_client.ini

for t in ${IO_THREADS[@]}; do
    for r in ${REQUESTS_PER_IO[@]}; do
        for b in ${BINARIES[@]}; do
            for s in ${SERVICES[@]}; do
                report $fb $t $r $b $s
            done
        done
    done
done

OUTPUT_FILE="$OUTPUT_DIR/agg.csv"

cat << EOF > $OUTPUT_FILE
Legend:
app;Version of psg_client application
io;Number of I/O threads
req;Number of requests consecutively given to the same I/O thread
svc;Service name
med;Median ($REQUESTS requests/run, $ITERATIONS runs)
avg;Average ($REQUESTS requests/run, $ITERATIONS runs)
*Measurements performed on $(uname -n)

app;io;req;svc;med;avg
EOF

for t in ${IO_THREADS[@]}; do
    for r in ${REQUESTS_PER_IO[@]}; do
        for b in ${BINARIES[@]}; do
            for s in ${SERVICES[@]}; do
                aggregate $OUTPUT_FILE $t $r $b $s
            done
        done
    done
done
echo
