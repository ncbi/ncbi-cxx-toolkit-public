#!/bin/bash

TEST_BINARY=$(dirname $0)/test_netstorage

# Check if able to execute actual binary
if [[ ! -x $TEST_BINARY ]]; then
    echo "Error: $TEST_BINARY is either not found or not executable"
    exit 2;
fi

# Help
if [[ $# -eq 0 || "$1" == "--help" ]]; then
    echo "Usage: $(basename $0) SERVICE_NAME [HOST:PORT]..."
    exit 1;
fi

# Service name
SERVICE=$1
export NCBI_CONFIG__NETSTORAGE_API__SERVICE_NAME=$SERVICE
shift

# If "HOST:PORT" parameters are provided
if [[ $# -gt 0 ]]; then
    export CONN_LOCAL_ENABLE=1

    for ((i = 0; $# > 0; ++i)); do
        export ${SERVICE}_CONN_LOCAL_SERVER_${i}="STANDALONE $1 L=yes"
        shift
    done
fi

# Disable irrelevant tests
export NCBI_CONFIG__UNITTESTS_DISABLE__Serverless="true"
export NCBI_CONFIG__UNITTESTS_DISABLE__NetCache="true"

# Run actual binary
$TEST_BINARY --show_progress
