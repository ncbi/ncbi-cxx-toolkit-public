#################################
# $Id$
#################################

APP = splign
SRC = splign_app

LIB = xalgoalignsplign xalgoalignutil xalgoalignnw \
      xblast xalgodustmask seqdb xnetblastcli blastdb \
      ncbi_xloader_lds lds bdb \
      xnetblast scoremat xobjsimple xobjutil xobjread tables \
      $(OBJMGR_LIBS:%=%$(STATIC))
#      $(OBJMGR_LIBS)

LIBS = $(BERKELEYDB_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CXXFLAGS = $(FAST_CXXFLAGS)
#CXXFLAGS = $(FAST_CXXFLAGS) -D GENOME_PIPELINE
LDFLAGS  = $(FAST_LDFLAGS)
