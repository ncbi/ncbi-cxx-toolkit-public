APP = guidetree
SRC = guide_tree guide_tree_calc guide_tree_simplify guide_tree_app

LIB_ = w_phylo_tree xalgoalignnw xalgophytree fastme \
	   gui_glmesa w_gl w_wx w_data \
	   gui_graph gui_opengl gui_print gui_config gui_objutils gui_utils \
	   xalgoalignutil xalnmgr ximage xcgi xhtml \
	   entrez2cli entrez2 valerr gbseq entrezgene biotree taxon1 \
	   xconnserv xqueryparse $(BLAST_LIBS) $(OBJMGR_LIBS)
LIB = $(LIB_:%=%$(STATIC))

LIBS = $(OSMESA_LIBS) $(WXWIDGETS_LIBS) $(IMAGE_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

CPPFLAGS = $(ORIG_CPPFLAGS) $(OSMESA_INCLUDE) $(WXWIDGETS_INCLUDE)

REQUIRES = MESA
