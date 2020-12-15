# $Id$

NCBI_add_definitions(HAVE_CONFIG_H=1)
NCBI_uses_toolkit_libraries(tds_ftds95)
NCBI_add_app(condition mutex1 strings passarg)
NCBI_project_watchers(ucko satskyse)

