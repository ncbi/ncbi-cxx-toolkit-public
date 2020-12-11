# $Id$

NCBI_begin_lib(gui_widgets_seq SHARED)
  NCBI_hosts_projects(
    gui/widgets/seq_graphic/w_seq_graphic
    gui/widgets/tax_tree/w_taxtree
    gui/widgets/seq/w_seq
    gui/widgets/feat_table/w_feat_table
    gui/widgets/seq_desktop/w_seq_desktop
    gui/widgets/seq_text/w_seq_text
    gui/widgets/edit/w_edit
  )
  NCBI_add_definitions(NCBI_GUIWIDGETS_SEQ_EXPORTS)
NCBI_end_lib()
