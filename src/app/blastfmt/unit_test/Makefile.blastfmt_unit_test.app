# $Id$

APP = blastfmt_unit_test
SRC = blast_hitmatrix_wrapper hitmatrix_unit_test

CPPFLAGS = $(OSMESA_INCLUDE) $(WXWIDGETS_INCLUDE) $(GLEW_INCLUDE) $(ORIG_CPPFLAGS) \
		   $(BOOST_INCLUDE)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS = $(FAST_LDFLAGS) 

LIB_ = w_hit_matrix gui_glmesa w_gl w_data w_wx gui_objutils gbproj \
       xalgoalignutil $(BLAST_FORMATTER_LIBS) gui_graph gui_print gui_opengl \
       gencoll_client gui_utils snputil ximage entrez2cli entrez2 eutils_client \
       valerr biotree entrezgene xconnserv xqueryparse $(EUTILS_LIBS) xmlwrapp \
       test_boost $(BLAST_LIBS) $(OBJMGR_LIBS)

LIB = $(LIB_:%=%$(STATIC))
LIBS = $(OSMESA_LIBS) $(WXWIDGETS_GL_LIBS) $(WXWIDGETS_LIBS) $(GLEW_LIBS) \
           $(FTGL_LIBS) $(OPENGL_LIBS) \
           $(IMAGE_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) \
           $(LIBXSLT_LIBS) $(LIBXML_LIBS) $(ORIG_LIBS)

CHECK_CMD = blastfmt_unit_test
CHECK_COPY = data

WATCHERS = boratyng madden camacho
