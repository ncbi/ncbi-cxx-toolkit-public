#!/bin/sh
# $Id$

script_name="`basename "$0"`"

if test $# -ne 2; then
cat <<EOF >&2
This script takes the grid_cli binary from the specified C++ Toolkit
build directory and combines it with a set of the same-aged 'ncbi.grid'
Python modules. The resulting Python module can be used for testing the
NetSchedule server against a particular grid_cli version.

Usage:
    $script_name <TOOLKIT_BUILD_DIR> <TARGET_MODULE_DIR>

Example:
    $ ./$script_name \\
            "\$NCBI/c++.production/DebugMT" 'grid_modules'

    $ python
    >>> from grid_modules.ncbi.grid import ipc
    >>> rpc_server = ipc.RPCServer()

EOF
    exit 1
fi

error()
{
    echo "$script_name: $1" >&2
    exit 2
}

toolkit_dir="$1"
target_dir="$2"

build_info_file="`dirname "$toolkit_dir"`/build_info"

test -f "$build_info_file" || error "file '$build_info_file' does not exist"

version="`perl -wne 'm/VERSION\s*=\s*(.*)/s && print $1' "$build_info_file"`"

bin_dir="$toolkit_dir/bin"

repo='https://svn.ncbi.nlm.nih.gov/repos/toolkit'

(cd `dirname $target_dir` && \
    svn export \
        --quiet \
        -r"{$version}" \
        "$repo/trunk/internal/scripts/common/lib/python/grid-python" \
        "${target_dir}" )

if test -f "$bin_dir/grid_cli.gz"; then
    [ -d "$target_dir" ] || mkdir "$target_dir"
    cp -p "$bin_dir/grid_cli.gz" "$target_dir"
    gunzip "$target_dir/grid_cli.gz"
elif test -f "$bin_dir/grid_cli.bz2"; then
    [ -d "$target_dir" ] || mkdir "$target_dir"
    cp -p "$bin_dir/grid_cli.bz2" "$target_dir"
    bunzip2 "$target_dir/grid_cli.bz2"
elif test -f "$bin_dir/grid_cli"; then
    [ -d "$target_dir" ] || mkdir "$target_dir"
    cp -p "$bin_dir/grid_cli" "$target_dir"
else
    error "cannot find grid_cli binary in '$bin_dir'"
fi

