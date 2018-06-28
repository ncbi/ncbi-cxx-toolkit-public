#############################################################################
# $Id$
#############################################################################

NCBI_project_tags(demo)
NCBI_add_app(alnvwr alnmrg aln_build aln_test)


# Include projects from this directory
#include(CMakeLists.alnvwr.app.txt)
#include(CMakeLists.alnmrg.app.txt)
#include(CMakeLists.aln_build.app.txt)
#include(CMakeLists.aln_test.app.txt)

# Recurse subdirectories
