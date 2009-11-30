APP = hit_matrix.cgi
SRC = blast_hitmatrix cgi_hit_matrix

LIB_ = w_hit_matrix gui_glmesa w_gl gui_graph gui_opengl gui_print \
	   gui_objutils gui_utils xalgoalignutil xalnmgr ximage \
	   xcgi xhtml xalnmgr entrez2cli entrez2 valerr gbseq xconnserv \
	   xqueryparse $(BLAST_LIBS) $(OBJMGR_LIBS)
LIB = $(LIB_:%=%$(STATIC))

LIBS = $(OSMESA_LIBS) $(IMAGE_LIBS) $(CMPRS_LIBS) \
       $(DL_LIBS) $(ORIG_LIBS)


CFLAGS   = $(FAST_CFLAGS)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

CPPFLAGS = $(WXWIDGETS_INCLUDE) $(OSMESA_INCLUDE) $(ORIG_CPPFLAGS)

REQUIRES = MESA

