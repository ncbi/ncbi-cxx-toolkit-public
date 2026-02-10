#!/bin/bash -e

DIR=$(dirname $(readlink -f $0))
TEST_BINARY=${1}

export JAVA_HOME="/usr/lib/jvm/jre-11"

CLUSTER_NAME="psg_ipg_test"
VERSION="4.1.10"
CQLSH="/netmnt/vast01/seqdb/id_dumps/id_software/cassandra/test/bin/cqlsh"
CCMENV="/netmnt/vast01/seqdb/id_dumps/id_software/venvs/ccm"

source ${CCMENV}/bin/activate
TEST_BASE_NAME=$(basename ${TEST_BINARY})
exec ${DIR}/ccm.py --binary "${TEST_BINARY}" --gtest_output "xml:xunit_${TEST_BASE_NAME}.xml"
