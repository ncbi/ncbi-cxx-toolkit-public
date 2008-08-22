APP = hit_matrix.cgi
SRC = blast_hitmatrix cgi_hit_matrix

LIB = w_hit_matrix w_aln_data gui_glmesa w_gl gui_graph gui_opengl gui_print\
      gui_objutils gui_utils_fl gui_utils xalnmgr ximage xcgi xhtml \
      xalnmgr xobjutil entrez2cli entrez2 valerr gbseq tables xconnserv $(OBJMGR_LIBS)

LIBS = $(OSMESA_LIBS) $(IMAGE_LIBS) $(FLTK_LIBS_ALL) $(CMPRS_LIBS) \
       $(DL_LIBS) $(ORIG_LIBS)


CFLAGS   = $(FAST_CFLAGS)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

CPPFLAGS = $(FLTK_INCLUDE) $(OSMESA_INCLUDE) $(ORIG_CPPFLAGS)

REQUIRES = MESA

