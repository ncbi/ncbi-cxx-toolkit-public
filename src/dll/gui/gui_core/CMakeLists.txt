# $Id$

NCBI_begin_lib(gui_core SHARED)
  NCBI_hosts_projects(
    gui/core/gui_core
    gui/framework/gui_framework
  )
  NCBI_add_definitions(NCBI_GUICORE_EXPORTS)
NCBI_end_lib()
