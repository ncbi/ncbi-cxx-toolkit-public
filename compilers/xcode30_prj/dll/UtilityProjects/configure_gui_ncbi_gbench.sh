#!/bin/sh
export PTB_FLAGS="-dll -cfg"
export PTB_PROJECT_REQ=scripts/projects/ncbi_gbench.lst
$BUILD_TREE_ROOT/ptb.sh
