# $Id$

NCBI_begin_lib(gui_utils SHARED)
  NCBI_hosts_projects(
    gui/utils/gui_utils
    gui/objutils/gui_objutils
    gui/opengl/gui_opengl
    gui/print/gui_print
  )
  NCBI_add_definitions(NCBI_GUIUTILS_EXPORTS)
NCBI_end_lib()
