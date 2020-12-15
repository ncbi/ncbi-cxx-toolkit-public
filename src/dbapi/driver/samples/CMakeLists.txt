# $Id$

NCBI_project_tags(sample)
NCBI_add_library(dbapi_sample_base)
NCBI_add_app(lang_query dbapi_driver_check)
NCBI_add_subdirectory(
  dbapi_bcp dbapi_cursor dbapi_query dbapi_send_data dbapi_testspeed dbapi_conn_policy
)
NCBI_project_watchers(ucko satskyse)

