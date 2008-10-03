#!/bin/sh
xcodebuild -project UtilityProjects/PTB.xcodeproj -target project_tree_builder -configuration ReleaseDLL
if test $? -eq 0; then
./bin/ReleaseDLL/project_tree_builder -logfile UtilityProjects/ncbi_cpp_configuration_log.txt -conffile ../../../src/build-system/project_tree_builder.ini ../../../ scripts/projects/xcode_cpp.lst ncbi_cpp
fi
open UtilityProjects/ncbi_cpp_configuration_log.txt
