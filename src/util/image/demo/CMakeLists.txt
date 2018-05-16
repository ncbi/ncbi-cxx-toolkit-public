#############################################################################
# $Id$
#############################################################################
# 
#
NCBI_disable_pch()
NCBI_project_tags(demo)
NCBI_add_app(sub_image conv_image image_info)

# Include projects from this directory
#include(CMakeLists.sub_image.app.txt)
#include(CMakeLists.conv_image.app.txt)
#include(CMakeLists.image_info.app.txt)

