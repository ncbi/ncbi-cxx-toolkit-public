#!/bin/sh

exclusions='branches tags production public release trunk/internal'

while read exclusion; do
    exclusions="$exclusions trunk/c++/$exclusion"
done <<EOF
include/connect/daemons
include/connect/ext
include/cgi/caf_plain.hpp
include/cgi/caf_encoded.hpp
include/internal
src/connect/daemons
src/connect/ext
src/connect/ncbi_lbsmd.c
src/connect/ncbi_lbsm.c
src/connect/ncbi_lbsm.h
src/connect/ncbi_lbsm_ipc.c
src/connect/ncbi_lbsm_ipc.h
src/internal
src/objects/add_asn.sh
scripts/cvs_core.sh
scripts/cvs_core.bat
scripts/update_core.sh
scripts/update_projects.sh
scripts/import_project.sh
scripts/internal
doc/internal
EOF

./read_dump.pl $exclusions \
    < TOOLKIT_DUMP-REVPATHS \
    > FULL_EXCLUDE_LIST
