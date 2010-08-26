APP = guide_tree_unit_test
SRC = guidetreecalc_wrapper guidetreecalc_unit_test guidetree_wrapper \
      guidetree_unit_test guidetreesimplify_wrapper guidetreerender_wrapper \
      guidetreerender_unit_test

LIB_ = w_phylo_tree xalgoalignnw xalgophytree fastme \
	   gui_glmesa w_gl w_wx w_data \
	   gui_graph gui_opengl gui_print gui_config gui_objutils gui_utils \
	   xalgoalignutil xalnmgr ximage xcgi xhtml \
	   entrez2cli entrez2 valerr gbseq entrezgene biotree taxon1 \
	   xconnserv xqueryparse test_boost $(BLAST_LIBS) $(OBJMGR_LIBS)
LIB = $(LIB_:%=%$(STATIC))

LIBS = $(OSMESA_LIBS) $(WXWIDGETS_LIBS) $(IMAGE_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

CPPFLAGS = $(ORIG_CPPFLAGS) $(OSMESA_INCLUDE) $(WXWIDGETS_INCLUDE) $(BOOST_INCLUDE)

REQUIRES = MESA

# CHECK_CMD = guidetree_unit_test
# CHECK_COPY = data

# WATCHERS = blastsoft
