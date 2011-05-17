WATCHERS = camacho boratyng

APP = hit_matrix.cgi
SRC = blast_hitmatrix cgi_hit_matrix

LIB_ = w_hit_matrix gui_glmesa w_gl w_wx w_data \
	   gui_graph gui_opengl gui_print gui_objutils gui_utils \
	   xalgoalignutil xalnmgr ximage xcgi xhtml \
	   entrez2cli entrez2 valerr gbseq entrezgene biotree \
	   xconnserv xqueryparse snputil $(EUTILS_LIBS) xser xconnect xutil xmlwrapp \
           $(BLAST_LIBS) $(OBJMGR_LIBS)
LIB = $(LIB_:%=%$(STATIC))

LIBS = $(OSMESA_LIBS) $(WXWIDGETS_GL_LIBS) $(WXWIDGETS_LIBS) $(GLEW_LIBS) \
       $(IMAGE_LIBS) $(CMPRS_LIBS) $(DL_LIBS) $(LIBXSLT_LIBS) $(LIBXML_LIBS) \
       $(ORIG_LIBS)


CFLAGS   = $(FAST_CFLAGS)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

CPPFLAGS = $(ORIG_CPPFLAGS) $(OSMESA_INCLUDE) $(WXWIDGETS_INCLUDE) $(GLEW_INCLUDE)

REQUIRES = MESA

