#!/bin/sh

# $Id$
# Author:  Andrei Gourianov, NCBI (gouriano@ncbi.nlm.nih.gov)
#
# Run project_tree_builder to generate XCode project file
#
# DO NOT ATTEMPT to run this script manually
# It should be run by CONFIGURE project only
# (open a project and build or rebuild CONFIGURE target)
#

PTB_EXE=${PTB_PATH}/project_tree_builder
test "$PTB_PROJECT" = "" && PTB_PROJECT=${PTB_PROJECT_REQ}

echo "======================================================================"
echo Building project_tree_builder.
echo xcodebuild -project UtilityProjects/PTB.xcodeproj -target project_tree_builder -configuration ReleaseDLL
echo "======================================================================"
xcodebuild -project UtilityProjects/PTB.xcodeproj -target project_tree_builder -configuration ReleaseDLL

if ! test -x $PTB_EXE; then
  echo ERROR: application not found: $PTB_EXE
  exit 1
fi
$PTB_EXE -version
if test $? -ne 0; then
  echo ERROR: cannot find working $PTB_EXE
  exit 1
fi

echo "======================================================================"
echo Running CONFIGURE.
echo $PTB_EXE $PTB_FLAGS -logfile ${SLN_PATH}_configuration_log.txt -conffile ${TREE_ROOT}src/build-system/project_tree_builder.ini $TREE_ROOT $PTB_PROJECT $SLN_PATH
echo "======================================================================"
$PTB_EXE $PTB_FLAGS -logfile ${SLN_PATH}_configuration_log.txt -conffile ${TREE_ROOT}src/build-system/project_tree_builder.ini $TREE_ROOT $PTB_PROJECT $SLN_PATH

if test "$TERM" = "dumb"; then
  open ${SLN_PATH}_configuration_log.txt
fi
