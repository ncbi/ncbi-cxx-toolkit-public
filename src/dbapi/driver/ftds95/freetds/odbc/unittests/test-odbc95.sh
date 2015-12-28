#!/bin/sh
servers='MSDEV1 DBAPI_DEV3'
failures=
succeeded_anywhere=no
status=0

if [ -r test-odbc95.cfg ]; then
    . test-odbc95.cfg
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
        --need-ftdsconf )
            shift
            FREETDSCONF=freetds.conf
            export FREETDSCONF
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

ODBCINI=odbc.ini
SYSODBCINI=odbc.ini
export ODBCINI SYSODBCINI

base=`echo $1 | sed -e 's/^odbc95_//'`
if [ -r "$base.in" ]; then
    for x in "$base".??*; do
        cp -f "$x" "odbc95_$x"
    done
fi

for server in $servers; do
    TDSPWDFILE=login-info-$server.txt
    case "$server" in
        MS* )
            TDSVER=7.3
            ;;
        * )
            # freeclose's internal proxy can't handle autodetection
            TDSVER=5.0
            ;;
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
    $CHECK_EXEC "$@"
    case $? in
        0 )
            echo ... SUCCEEDED on $server
            succeeded_anywhere=yes
            ;;
        77 )
            echo ... SKIPPED on $server
            ;;
        * )
            status=$?
            echo ... FAILED on $server, with status $status
            failures="$failures $server ($status)"
            ;;
    esac
    echo
done

if [ -n "$failures" ]; then
    echo "FAILED:$failures"
elif [ "$succeeded_anywhere" = no ]; then
    echo NCBI_UNITTEST_SKIPPED
fi

exit $status
