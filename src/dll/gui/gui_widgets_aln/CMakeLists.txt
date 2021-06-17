# $Id$

NCBI_begin_lib(gui_widgets_aln SHARED)
  NCBI_hosts_projects(
    gui/widgets/aln_crossaln/w_aln_crossaln
    gui/widgets/aln_multiple/w_aln_multi
    gui/widgets/hit_matrix/w_hit_matrix
    gui/widgets/aln_table/w_aln_table 
  )
  NCBI_add_definitions(NCBI_GUIWIDGETS_ALN_EXPORTS)
NCBI_end_lib()
