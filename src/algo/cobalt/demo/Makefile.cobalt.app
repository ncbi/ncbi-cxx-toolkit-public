APP = cobalt
SRC = cobalt_app
LIB = cobalt xalgoalignnw xalgophytree fastme biotree xblast xalgoblastdbindex \
      xalgodustmask composition_adjustment xnetblastcli xnetblast scoremat \
      ncbi_xloader_blastdb seqdb blastdb xalnmgr xobjsimple xobjutil \
      xobjread creaders tables $(OBJMGR_LIBS)

CFLAGS   = $(FAST_CFLAGS)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

# CPPFLAGS = $(ORIG_CPPFLAGS) $(NCBI_C_INCLUDE)
LIBS = $(NETWORK_LIBS) $(CMPRS_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = -Cygwin
