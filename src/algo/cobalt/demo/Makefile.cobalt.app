APP = cobalt
SRC = cobalt_app
LIB = cobalt xalgoalignnw xalgophytree fastme biotree xblast xalgodustmask \
      xnetblastcli xnetblast scoremat ncbi_xloader_blastdb seqdb blastdb \
      xalnmgr xobjutil xobjread tables $(OBJMGR_LIBS)

CFLAGS   = $(FAST_CFLAGS)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

CPPFLAGS = $(ORIG_CPPFLAGS) $(NCBI_C_INCLUDE)
NCBI_C_LIBS = -lblastapi -lncbitool -lncbiobj -lncbi
LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

