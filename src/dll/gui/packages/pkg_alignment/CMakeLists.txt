# $Id$

NCBI_begin_lib(gui_view_align SHARED)
  NCBI_hosts_projects(
    gui/packages/pkg_alignment/gui_view_align
  )
  NCBI_add_definitions(NCBI_GUIVIEW_ALIGN_EXPORTS)
NCBI_end_lib()

NCBI_begin_lib(pkg_alignment SHARED)
  NCBI_hosts_projects(
    gui/packages/pkg_alignment/pkg_alignment
  )
NCBI_end_lib()
