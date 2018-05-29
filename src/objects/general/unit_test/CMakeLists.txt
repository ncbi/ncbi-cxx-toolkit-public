#############################################################################
# $Id$
#############################################################################

NCBI_project_tags(test)
NCBI_requires(Boost.Test.Included)
NCBI_add_app(unit_test_field unit_test_field_collection)


# Include projects from this directory
#include(CMakeLists.unit_test_field.app.txt)
#include(CMakeLists.unit_test_field_collection.app.txt)

