# $Id$

NCBI_begin_lib(gui_widgets SHARED)
  NCBI_hosts_projects(
    gui/widgets/wx/w_wx
    gui/widgets/gl/w_gl
    gui/widgets/data/w_data
    gui/widgets/loaders/w_loaders
    gui/widgets/aln_score/w_aln_score
    gui/widgets/feedback/w_feedback
  )
  NCBI_add_definitions(NCBI_GUIWIDGETS_EXPORTS)
NCBI_end_lib()
