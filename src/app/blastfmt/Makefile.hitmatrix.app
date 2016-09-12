WATCHERS = camacho boratyng

APP = hit_matrix.cgi
SRC = blast_hitmatrix cgi_hit_matrix

LIB_ = w_hit_matrix gui_glmesa w_gl w_data w_wx \
           gui_graph gui_print gui_opengl gui_objutils gui_utils \
	   eutils_client gencoll_client gbproj \
	   xalgoalignutil ximage xcgi xhtml \
	   entrez2cli entrez2 valerr gbseq entrezgene biotree \
	   xconnserv xqueryparse snputil $(EUTILS_LIBS) xmlwrapp \
	   $(BLAST_LIBS) $(OBJMGR_LIBS)

LIB = $(LIB_:%=%$(STATIC))

LIBS = $(OSMESA_LIBS) $(WXWIDGETS_GL_LIBS) $(WXWIDGETS_LIBS) $(GLEW_LIBS) \
       $(FTGL_LIBS) $(OPENGL_LIBS) \
       $(IMAGE_LIBS) $(CMPRS_LIBS) $(DL_LIBS) $(LIBXSLT_LIBS) $(LIBXML_LIBS) \
       $(NETWORK_LIBS) $(ORIG_LIBS)


CFLAGS   = $(FAST_CFLAGS)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

CPPFLAGS = $(ORIG_CPPFLAGS) $(OSMESA_INCLUDE) $(WXWIDGETS_INCLUDE) $(FTGL_INCLUDE) $(GLEW_INCLUDE)

REQUIRES = MESA

