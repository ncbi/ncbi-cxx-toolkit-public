#############################################################################
# $Id$
#############################################################################

NCBI_add_definitions(HAVE_CONFIG_H=1)
NCBI_uses_toolkit_libraries(ct_ftds95)
NCBI_project_watchers(ucko satskyse)

NCBI_add_app(
  test-ct95 t0001 t0002 t0003 t0004 t0005 t0006 t0007 t0008 t0009
  connect_fail ct_options lang_ct_param array_bind cs_diag
  get_send_data rpc_ct_param rpc_ct_setparam ct_diagclient
  ct_diagserver ct_diagall cs_config cancel blk_in blk_out ct_cursor
  ct_cursors ct_dynamic blk_in2 datafmt rpc_fail
)


if(OFF)


# Include projects from this directory
include(CMakeLists.test-ct95.app.txt)
include(CMakeLists.t0001.app.txt)
include(CMakeLists.t0002.app.txt)
include(CMakeLists.t0003.app.txt)
include(CMakeLists.t0004.app.txt)
include(CMakeLists.t0005.app.txt)
include(CMakeLists.t0006.app.txt)
include(CMakeLists.t0007.app.txt)
include(CMakeLists.t0008.app.txt)
include(CMakeLists.t0009.app.txt)
include(CMakeLists.connect_fail.app.txt)
include(CMakeLists.ct_options.app.txt)
include(CMakeLists.lang_ct_param.app.txt)
include(CMakeLists.array_bind.app.txt)
include(CMakeLists.cs_diag.app.txt)
include(CMakeLists.get_send_data.app.txt)
include(CMakeLists.rpc_ct_param.app.txt)
include(CMakeLists.rpc_ct_setparam.app.txt)
include(CMakeLists.ct_diagclient.app.txt)
include(CMakeLists.ct_diagserver.app.txt)
include(CMakeLists.ct_diagall.app.txt)
include(CMakeLists.cs_config.app.txt)
include(CMakeLists.cancel.app.txt)
include(CMakeLists.blk_in.app.txt)
include(CMakeLists.blk_out.app.txt)
include(CMakeLists.ct_cursor.app.txt)
include(CMakeLists.ct_cursors.app.txt)
include(CMakeLists.ct_dynamic.app.txt)
include(CMakeLists.blk_in2.app.txt)
include(CMakeLists.datafmt.app.txt)
include(CMakeLists.rpc_fail.app.txt)
endif()
