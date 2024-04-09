# $Id$

NCBI_project_tags(test)
NCBI_add_definitions(HAVE_CONFIG_H=1)
NCBI_uses_toolkit_libraries(odbc_ftds14 tds_ftds14 odbc_ftds14)

NCBI_set_test_assets(odbc.ini)
NCBI_set_test_requires(in-house-resources)

NCBI_add_app(
  test-odbc14 t0001 t0002 t0003 moreresults connect print date
  norowset funccall lang_error tables binary_test moreandcount
  earlybind putdata params raiserror getdata transaction type
  genparams preperror prepare_results testodbc data error rebindpar
  rpc convert_error typeinfo const_params insert_speed compute
  timeout array array_out cursor1 scroll cursor2 describecol
  copydesc prepclose warning paramcore timeout2 timeout3 connect2
  timeout4 freeclose cursor3 cursor4 cursor5 attributes hidden
  blob1 cancel wchar rowset transaction2 cursor6 cursor7 utf8
  utf8_2 utf8_3 stats descrec peter test64 prepare_warn long_error
  mars1 array_error describecol2 closestmt bcp all_types empty_query
  transaction3 transaction4 utf8_4 qn connection_string_parse tvp
  oldpwd
)

NCBI_project_watchers(ucko satskyse)
