#!/bin/sh
# $Id$

script_name="`basename "$0"`"

if test $# -ne 2; then
    echo "Usage: $script_name <PATH_TO_TOOLKIT_DIR> <TARGET_DIR>" >&2
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

if test -f "$bin_dir/grid_cli.gz"; then
    mkdir "$target_dir" || exit 3
    cp -p "$bin_dir/grid_cli.gz" "$target_dir"
    gunzip "$target_dir/grid_cli.gz"
elif test -f "$bin_dir/grid_cli"; then
    mkdir "$target_dir" || exit 3
    cp -p "$bin_dir/grid_cli" "$target_dir"
else
    error "cannot find grid_cli binary in '$bin_dir'"
fi

repo='https://svn.ncbi.nlm.nih.gov/repos/toolkit'

(cd "$target_dir" && \
    svn export \
        --quiet \
        -r"{$version}" \
        "$repo/trunk/internal/scripts/common/lib/python/ncbi")

cat <<EOF > "$target_dir/__init__.py"
import sys
import os.path

module_path = __path__[0]

def version():
    import re
    match = re.search('\d+(_\d+)+$', module_path)
    if match:
        return match.group(0).replace('_', '.')
    return 'unknown'

def get_grid_cli_pathname():
    return __builtins__['ncbi_grid_cli_pathname']

__builtins__['ncbi_grid_version_module'] = sys.modules[__name__]
__builtins__['ncbi_grid_cli_pathname'] = os.path.abspath(
        os.path.join(module_path, 'grid_cli'))
EOF
