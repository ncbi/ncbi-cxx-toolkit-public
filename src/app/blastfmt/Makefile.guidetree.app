WATCHERS = camacho boratyng

APP = guidetree
SRC = guide_tree_app

LIB_ = phytree_format xalgoalignnw xalgophytree fastme \
	   xalgoalignutil xalnmgr biotree taxon1 \
           $(BLAST_LIBS) xconnect $(OBJMGR_LIBS)
LIB = $(LIB_:%=%$(STATIC))

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

CPPFLAGS = $(ORIG_CPPFLAGS)

REQUIRES = MESA
