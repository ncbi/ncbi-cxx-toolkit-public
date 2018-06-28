#############################################################################
# $Id$
#############################################################################

NCBI_add_definitions(HAVE_CONFIG_H=1)
NCBI_uses_toolkit_libraries(odbc_ftds95 tds_ftds95 odbc_ftds95)
NCBI_project_watchers(ucko satskyse)

NCBI_add_app(
  test-odbc95 t0001 t0002 t0003 t0004 connect print date norowset
  funccall lang_error tables binary_test moreandcount earlybind
  putdata params raiserror getdata transaction type genparams
  preperror prepare_results testodbc data error rebindpar rpc
  convert_error typeinfo const_params insert_speed compute timeout
  array array_out cursor1 scroll cursor2 describecol copydesc
  prepclose warning paramcore timeout2 timeout3 connect2 timeout4
  freeclose cursor3 cursor4 cursor5 attributes hidden blob1 cancel
  wchar rowset transaction2 cursor6 cursor7 utf8 utf8_2 stats
  descrec peter test64 prepare_warn long_error mars1 array_error
  describecol2 closestmt
)


if(OFF)

add_definitions(-DHAVE_CONFIG_H=1 -DNEED_FREETDS_SRCDIR)

# Include projects from this directory
include(CMakeLists.test-odbc95.app.txt)
include(CMakeLists.t0001.app.txt)
include(CMakeLists.t0002.app.txt)
include(CMakeLists.t0003.app.txt)
include(CMakeLists.t0004.app.txt)
include(CMakeLists.connect.app.txt)
include(CMakeLists.print.app.txt)
include(CMakeLists.date.app.txt)
include(CMakeLists.norowset.app.txt)
include(CMakeLists.funccall.app.txt)
include(CMakeLists.lang_error.app.txt)
include(CMakeLists.tables.app.txt)
include(CMakeLists.binary_test.app.txt)
include(CMakeLists.moreandcount.app.txt)
include(CMakeLists.earlybind.app.txt)
include(CMakeLists.putdata.app.txt)
include(CMakeLists.params.app.txt)
include(CMakeLists.raiserror.app.txt)
include(CMakeLists.getdata.app.txt)
include(CMakeLists.transaction.app.txt)
include(CMakeLists.type.app.txt)
include(CMakeLists.genparams.app.txt)
include(CMakeLists.preperror.app.txt)
include(CMakeLists.prepare_results.app.txt)
include(CMakeLists.testodbc.app.txt)
include(CMakeLists.data.app.txt)
include(CMakeLists.error.app.txt)
include(CMakeLists.rebindpar.app.txt)
include(CMakeLists.rpc.app.txt)
include(CMakeLists.convert_error.app.txt)
include(CMakeLists.typeinfo.app.txt)
include(CMakeLists.const_params.app.txt)
include(CMakeLists.insert_speed.app.txt)
include(CMakeLists.compute.app.txt)
include(CMakeLists.timeout.app.txt)
include(CMakeLists.array.app.txt)
include(CMakeLists.array_out.app.txt)
include(CMakeLists.cursor1.app.txt)
include(CMakeLists.scroll.app.txt)
include(CMakeLists.cursor2.app.txt)
include(CMakeLists.describecol.app.txt)
include(CMakeLists.copydesc.app.txt)
include(CMakeLists.prepclose.app.txt)
include(CMakeLists.warning.app.txt)
include(CMakeLists.paramcore.app.txt)
include(CMakeLists.timeout2.app.txt)
include(CMakeLists.timeout3.app.txt)
include(CMakeLists.connect2.app.txt)
include(CMakeLists.timeout4.app.txt)
include(CMakeLists.freeclose.app.txt)
include(CMakeLists.cursor3.app.txt)
include(CMakeLists.cursor4.app.txt)
include(CMakeLists.cursor5.app.txt)
include(CMakeLists.attributes.app.txt)
include(CMakeLists.hidden.app.txt)
include(CMakeLists.blob1.app.txt)
include(CMakeLists.cancel.app.txt)
include(CMakeLists.wchar.app.txt)
include(CMakeLists.rowset.app.txt)
include(CMakeLists.transaction2.app.txt)
include(CMakeLists.cursor6.app.txt)
include(CMakeLists.cursor7.app.txt)
include(CMakeLists.utf8.app.txt)
include(CMakeLists.utf8_2.app.txt)
include(CMakeLists.stats.app.txt)
include(CMakeLists.descrec.app.txt)
include(CMakeLists.peter.app.txt)
include(CMakeLists.test64.app.txt)
include(CMakeLists.prepare_warn.app.txt)
include(CMakeLists.long_error.app.txt)
include(CMakeLists.mars1.app.txt)
include(CMakeLists.array_error.app.txt)
include(CMakeLists.describecol2.app.txt)
include(CMakeLists.closestmt.app.txt)
endif()
