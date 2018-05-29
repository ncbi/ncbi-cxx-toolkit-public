#############################################################################
# $Id$
#############################################################################

NCBI_project_tags(test)
NCBI_requires(Boost.Test.Included)
NCBI_add_app(unit_test_string_constraint unit_test_location_constraint)


# Include projects from this directory
#include(CMakeLists.unit_test_string_constraint.app.txt)
#include(CMakeLists.unit_test_location_constraint.app.txt)

