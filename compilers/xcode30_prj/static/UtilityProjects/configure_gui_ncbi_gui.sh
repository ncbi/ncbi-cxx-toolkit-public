#!/bin/sh
export PTB_FLAGS="-cfg"
export PTB_PROJECT_REQ=scripts/projects/ncbi_gui.lst
$BUILD_TREE_ROOT/ptb.sh
