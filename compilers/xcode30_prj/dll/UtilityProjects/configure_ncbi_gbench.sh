#!/bin/sh
xcodebuild -project ../static/UtilityProjects/PTB.xcodeproj -target project_tree_builder -configuration ReleaseDLL
if test $? -eq 0; then
../static/bin/ReleaseDLL/project_tree_builder -dll -logfile ncbi_gbench_configuration_log.txt -conffile ../../../src/build-system/project_tree_builder.ini ../../../ scripts/projects/ncbi_gbench.lst ncbi_gbench
fi
echo done
if test "$TERM" = "dumb"; then
open ncbi_gbench_configuration_log.txt
fi
