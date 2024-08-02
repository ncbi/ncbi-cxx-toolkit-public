APP = hyperclust

SRC = hyperclust cobalt_app_util
LIB = cobalt xalgoalignnw xalgophytree fastme biotree \
      align_format taxon1  xcgi xhtml $(BLAST_LIBS) $(OBJMGR_LIBS)

CFLAGS   = $(FAST_CFLAGS)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

# CPPFLAGS = $(ORIG_CPPFLAGS)
LIBS = $(BLAST_THIRD_PARTY_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) \
       $(ORIG_LIBS)

WATCHERS = boratyng
