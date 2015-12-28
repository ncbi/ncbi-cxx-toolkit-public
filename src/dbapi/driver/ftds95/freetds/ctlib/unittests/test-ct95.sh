#!/bin/sh
servers='MSDEV1 DBAPI_DEV3'
failures=
status=0

MSTDSVER=7.3

if [ -r test-ct95.cfg ]; then
    . test-ct95.cfg
    if test -d "$syb" -o -z "$SYBASE"; then
        SYBASE=$syb
        export SYBASE
    fi
elif [ -z "${NCBI_TRIED_RUN_SYBASE_APP+set}" ] \
     &&  run_sybase_app.sh -run-script true; then
    NCBI_TRIED_RUN_SYBASE_APP=1
    export NCBI_TRIED_RUN_SYBASE_APP
    exec run_sybase_app.sh -run-script "$0" "$@"
fi

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

for server in $servers; do
    TDSPWDFILE=login-info-$server.txt
    case "$server" in
        MS* )
            TDSVER=$MSTDSVER
            export TDSVER
            ;;
        * )
            unset TDSVER
            ;;
    esac
    cat >$TDSPWDFILE <<EOF
SRV=$server
UID=DBAPI_test
PWD=allowed
DB=DBAPI_Sample
EOF
    export TDSPWDFILE
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
