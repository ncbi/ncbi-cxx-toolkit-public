#!/bin/sh
servers='MSDEV1 DBAPI_DEV3'
failures=
status=0

MSTDSVER=7.3

while :; do
    case "$1" in
        --ms-ver )
            shift
            MSTDSVER=$1
            shift
            ;;
        --no-auto )
            shift
            if [ -n "${NCBI_AUTOMATED_BUILD+set}" ]; then
                echo NCBI_UNITTEST_SKIPPED
                exit 0
            fi
            ;;
        -* )
            echo "$0: Unsupported option $1"
            exit 1
            ;;
        * )
            break
            ;;
    esac
done

base=`echo $1 | sed -e 's/^db95_//'`
if [ -r "$base.sql" ]; then
    for x in "$base".??*; do
        cp -f "$x" "db95_$x"
    done
fi
if [ -r "${base}_1.sql" ]; then
    for x in "$base"_*.??*; do
        cp -f "$x" "db95_$x"
    done
fi

for server in $servers; do
    TDSPWDFILE=login-info-$server.txt
    case "$server" in
        MS* ) TDSVER=$MSTDSVER ;;
        * )   TDSVER=5.0 ;;
    esac
    cat >$TDSPWDFILE <<EOF
SRV=$server
UID=DBAPI_test
PWD=allowed
DB=DBAPI_Sample
EOF
    export TDSPWDFILE TDSVER
    echo $server
    echo --------------------
    if $CHECK_EXEC "$@"; then
        echo ... SUCCEEDED on $server
    else
        status=$?
        echo ... FAILED on $server, with status $status
        failures="$failures $server ($status)"
    fi
    echo
done

[ -z "$failures" ]  ||  echo "FAILED:$failures"

exit $status
