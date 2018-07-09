#############################################################################
# $Id$
#############################################################################

NCBI_project_tags(demo)
NCBI_add_app(demo_score_builder demo_genomic_compart)

# Include projects from this directory
#include(CMakeLists.demo_score_builder.app.txt)
#include(CMakeLists.demo_genomic_compart.app.txt)

