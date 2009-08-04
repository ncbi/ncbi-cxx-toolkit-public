APP = guidetree
SRC = guide_tree guide_tree_calc guide_tree_simplify guide_tree_app

LIB = cobalt w_phylo_tree w_aln_data xalgoalignnw xalgophytree fastme biotree \
      w_gl gui_graph gui_opengl gui_glmesa gui_print gui_config gui_objutils \
      xalgoalignutil gui_utils_fl gui_utils valerr entrez2 w_fltk ximage \
      xalnmgr xconnserv taxon1 xcgi xhtml xqueryparse \
      $(BLAST_LIBS) $(OBJMGR_LIBS)

LIBS = $(OSMESA_LIBS) $(IMAGE_LIBS) $(FLTK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

CPPFLAGS = $(FLTK_INCLUDE) $(OSMESA_INCLUDE) $(ORIG_CPPFLAGS)

REQUIRES = MESA
