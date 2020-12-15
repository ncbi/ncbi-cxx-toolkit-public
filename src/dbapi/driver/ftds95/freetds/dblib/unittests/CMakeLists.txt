# $Id$

NCBI_project_tags(test)
NCBI_add_definitions(HAVE_CONFIG_H=1 NEED_FREETDS_SRCDIR)
NCBI_uses_toolkit_libraries(sybdb_ftds95)
NCBI_set_test_requires(in-house-resources)

NCBI_add_app(
  test-db95 t0001 t0002 t0003 t0004 t0005 t0006 t0007 t0008 t0009
  t0011 t0012 t0013 t0014 t0015 t0016 t0017 t0018 t0019 t0020 t0021
  t0022 t0023 rpc dbmorecmds bcp thread text_buffer done_handling
  timeout hang null null2 setnull numeric pending cancel
)

NCBI_project_watchers(ucko satskyse)
