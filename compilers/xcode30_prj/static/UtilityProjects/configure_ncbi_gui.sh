#!/bin/sh
xcodebuild -project UtilityProjects/PTB.xcodeproj -target project_tree_builder -configuration ReleaseDLL
if test $? -eq 0; then
./bin/ReleaseDLL/project_tree_builder -logfile ncbi_gui_configuration_log.txt -conffile ../../../src/build-system/project_tree_builder.ini ../../../ scripts/projects/ncbi_gui.lst ncbi_gui
fi
echo done
open ncbi_gui_configuration_log.txt
