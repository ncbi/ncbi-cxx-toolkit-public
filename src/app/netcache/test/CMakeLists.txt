# $Id$

NCBI_begin_app(logs_replay)
  NCBI_sources(logs_replay)
  NCBI_uses_toolkit_libraries(ncbi_xcache_netcache)
NCBI_end_app()

NCBI_begin_app(logs_splitter)
  NCBI_sources(logs_splitter)
  NCBI_uses_toolkit_libraries(xncbi)
NCBI_end_app()

NCBI_begin_app(test_nc_stress)
  NCBI_sources(test_nc_stress)
  NCBI_uses_toolkit_libraries(xconnserv)
NCBI_end_app()

NCBI_begin_app(test_nc_stress_pubmed)
  NCBI_sources(test_nc_stress_pubmed)
  NCBI_uses_toolkit_libraries(xconnserv)
NCBI_end_app()
