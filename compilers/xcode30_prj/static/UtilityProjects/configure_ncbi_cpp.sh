#!/bin/sh
xcodebuild -project UtilityProjects/PTB.xcodeproj -target project_tree_builder -configuration ReleaseDLL
if test $? -eq 0; then
./bin/ReleaseDLL/project_tree_builder -logfile ncbi_cpp_configuration_log.txt -conffile ../../../src/build-system/project_tree_builder.ini ../../../ scripts/projects/ncbi_cpp.lst ncbi_cpp
fi
echo done
if test "$TERM" = "dumb"; then
open ncbi_cpp_configuration_log.txt
fi