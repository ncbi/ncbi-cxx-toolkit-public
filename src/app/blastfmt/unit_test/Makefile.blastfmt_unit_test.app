# $Id$

APP = blastfmt_unit_test
SRC = blast_hitmatrix_wrapper hitmatrix_unit_test

CPPFLAGS = $(OSMESA_INCLUDE) $(WXWIDGETS_INCLUDE) $(GLEW_INCLUDE) $(ORIG_CPPFLAGS) \
		   $(BOOST_INCLUDE)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS = $(FAST_LDFLAGS) $(OPENMP_FLAGS)

LIB_ = $(BLAST_FORMATTER_LIBS) w_hit_matrix gui_glmesa w_gl w_wx w_data \
      gui_graph gui_opengl gui_print gui_objutils gencoll_client gbproj \
      gui_utils snputil \
      xalgoalignutil xalnmgr ximage xcgi xhtml \
	  entrez2cli entrez2 valerr biotree gbseq entrezgene \
	  xconnserv xqueryparse $(EUTILS_LIBS) xser xconnect xutil xmlwrapp \
          test_boost $(BLAST_LIBS) $(OBJMGR_LIBS) variation

LIB = $(LIB_:%=%$(STATIC))
LIBS = $(OSMESA_LIBS) $(WXWIDGETS_GL_LIBS) $(WXWIDGETS_LIBS) $(GLEW_LIBS) \
           $(FTGL_LIBS) $(OPENGL_LIBS) \
           $(IMAGE_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) \
           $(LIBXSLT_LIBS) $(LIBXML_LIBS) $(ORIG_LIBS)

CHECK_CMD = blastfmt_unit_test
CHECK_COPY = data

WATCHERS = boratyng madden camacho
