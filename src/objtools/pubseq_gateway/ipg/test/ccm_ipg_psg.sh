#!/bin/bash -e

DIR=$(dirname $(readlink -f $0))

exec $DIR/ccm.sh $DIR/psg_ipg_test

