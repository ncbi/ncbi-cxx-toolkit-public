# $Id$

NCBI_project_tags(test)
NCBI_add_definitions(HAVE_CONFIG_H=1)
NCBI_uses_toolkit_libraries(tds_ut_common_ftds14)
NCBI_set_test_requires(in-house-resources)

NCBI_add_library(tds_ut_common_ftds14)
NCBI_add_app(
  test-tds14 t0001 t0002 t0003 t0004 t0005 t0006 t0007 t0008
  dynamic1 convert dataread utf8_1 utf8_2 utf8_3 numeric iconv_fread
  toodynamic readconf charconv nulls corrupt declarations portconf
  parsing freeze strftime log_elision convert_bounds collations
)

NCBI_project_watchers(ucko satskyse)

