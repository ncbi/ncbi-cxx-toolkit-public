APP = guidetree
SRC = guide_tree guide_tree_calc guide_tree_simplify guide_tree_app

LIB_ = cobalt w_phylo_tree xalgoalignnw xalgophytree fastme biotree w_gl \
	   w_data gui_graph gui_opengl gui_glmesa gui_print gui_config \
	   gui_objutils xalgoalignutil gui_utils valerr entrez2 ximage \
	   xalnmgr xconnserv taxon1 xcgi xhtml xqueryparse \
	   $(BLAST_LIBS) $(OBJMGR_LIBS)
LIB = $(LIB_:%=%$(STATIC))

LIBS = $(OSMESA_LIBS) $(IMAGE_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

CPPFLAGS = $(WXWIDGETS_INCLUDE) $(OSMESA_INCLUDE) $(ORIG_CPPFLAGS)

REQUIRES = MESA
