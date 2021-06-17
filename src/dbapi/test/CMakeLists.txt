#############################################################################
# $Id$
#############################################################################

NCBI_project_tags(test)
NCBI_set_test_requires(in-house-resources)
NCBI_add_app(dbapi_unit_test dbapi_context_test dbapi_svc_test)
NCBI_add_subdirectory(driver)

