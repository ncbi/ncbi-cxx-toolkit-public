#############################################################################
# $Id$
#############################################################################

NCBI_add_definitions(HAVE_CONFIG_H=1)
NCBI_uses_toolkit_libraries(tds_ftds95)
NCBI_project_watchers(ucko satskyse)

NCBI_add_app(condition mutex1 strings passarg)


# Include projects from this directory
#include(CMakeLists.condition.app.txt)
#include(CMakeLists.mutex1.app.txt)
#include(CMakeLists.strings.app.txt)
#include(CMakeLists.passarg.app.txt)

