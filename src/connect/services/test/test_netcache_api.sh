#!/bin/sh
#
# $Id$
#

echo "--------- Without mirroring ---------"

cat >test_netcache_api.ini <<EOF
[netcache_api]
enable_mirroring = false
EOF

$CHECK_EXEC ./test_netcache_api -repeat 1 NC_UnitTest
status=$?
if [ $status -ne 0 ]; then
    exit $status
fi


echo
echo "--------- With mirroring ---------"

cat >test_netcache_api.ini <<EOF
[netcache_api]
enable_mirroring = true
EOF

$CHECK_EXEC ./test_netcache_api -repeat 1 NC_UnitTest
exit $?
