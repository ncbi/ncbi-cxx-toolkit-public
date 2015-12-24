#!/bin/sh
servers='MSDEV1 DBAPI_DEV3'
failures=
status=0

if [ -r test-tds95.cfg ]; then
    . test-tds95.cfg
    if test -d "$syb" -o -z "$SYBASE"; then
        SYBASE=$syb
        export SYBASE
    fi
fi

for server in $servers; do
    TDSPWDFILE=login-info-$server.txt
    case "$server" in
        MS* )
            TDSVER=7.3
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
