# $Id$

NCBI_begin_lib(gui_widgets_misc SHARED)
  NCBI_hosts_projects(
    gui/widgets/phylo_tree/w_phylo_tree
    gui/widgets/object_list/w_object_list
    gui/widgets/text_widget/w_text_widget
    gui/widgets/grid_widget/w_grid_widget
    gui/widgets/macro_edit/w_macro_edit
  )
  NCBI_add_definitions(NCBI_GUIWIDGETS_MISC_EXPORTS)
NCBI_end_lib()
