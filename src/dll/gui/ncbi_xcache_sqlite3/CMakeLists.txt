# $Id$

NCBI_begin_lib(ncbi_xcache_sqlite3 SHARED)
  NCBI_hosts_projects(
    gui/cache/ncbi_xcache_sqlite3
  )
  NCBI_add_definitions(NCBI_SQLITE3_CACHE_EXPORTS)
NCBI_end_lib()
