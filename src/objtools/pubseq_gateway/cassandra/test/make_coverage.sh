#!/bin/bash

FILE=$(readlink -e $0)
DIR=$(dirname $FILE)

pushd $DIR
source fac-switch.sh lcov default
lcov -d ../../../../../src/ -c -o code.coverage
lcov -e code.coverage '*objtools/pubseq_gateway/*cassandra*' -o extracted_coverage.info
genhtml extracted_coverage.info --ignore-errors source -t "objtools/pubseq_gateway/cassandra" --num-spaces 4 --legend --demangle-cpp -p $(readlink -e ../../../../..) -o code_coverage/
cp extracted_coverage.info code_coverage/
popd
