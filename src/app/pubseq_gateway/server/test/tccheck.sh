#! /usr/bin/env bash

if [ "aa$TEAMCITY_VERSION" == "aa" ] ; then
    echo "Skipping tests which are supposed to be run under teamcity"
    set
    exit 0
fi

set -x

# Find free port
PORTCFG=$(cat <<EOF | python3
import socket

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.bind(('', 0))
addr = s.getsockname()
print (addr[1])
s.close()

EOF

)

# Prepare .ini
[ -e ./pubseq_gateway.ini ] || exit 2
sed -i.bak "s/@PORT@/$PORTCFG/g" ./pubseq_gateway.ini

# Update psg.bash
sed -i.bak "s/unset PORTCFG/PORTCFG=$PORTCFG/g" test/psg.bash

# Start server
./pubseq_gateway > server.stdout 2>&1 &
PID=$!

python3 test/run_tests.py > test.log 2>&1

kill -INT $PID
sleep 5
kill -9 $PID

echo "##teamcity[publishArtifacts '$(pwd)/report.html']"
echo "##teamcity[publishArtifacts '$(pwd)/log.html']"
echo "##teamcity[publishArtifacts '$(pwd)/output.xml']"

echo "##teamcity[blockOpened name='Server STDOUT']"
cat server.stdout
echo "##teamcity[blockClosed name='Server STDOUT']"

echo "##teamcity[blockOpened name='Test log']"
cat test.log
echo "##teamcity[blockClosed name='Test log']"
