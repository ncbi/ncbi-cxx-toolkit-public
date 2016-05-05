APP = cobalt
SRC = cobalt_app_util cobalt_app
LIB = cobalt xalgoalignnw xalgophytree fastme biotree \
      align_format taxon1 gene_info xcgi xhtml $(BLAST_LIBS) $(OBJMGR_LIBS)

CFLAGS   = $(FAST_CFLAGS)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

# CPPFLAGS = $(ORIG_CPPFLAGS) $(NCBI_C_INCLUDE)
LIBS = $(NETWORK_LIBS) $(CMPRS_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = -Cygwin
WATCHERS = boratyng
