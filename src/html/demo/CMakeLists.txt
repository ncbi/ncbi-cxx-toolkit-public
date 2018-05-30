#############################################################################
# $Id$
#############################################################################

NCBI_project_tags(demo)
NCBI_add_app(demo_html demo_html_template)

if(OFF)
# 
#

# Include projects from this directory
include(CMakeLists.demo_html.app.txt)
include(CMakeLists.demo_html_template.app.txt)
endif()
