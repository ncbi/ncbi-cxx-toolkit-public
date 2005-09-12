#################################
# $Id$
#################################

# Build demo "splign"
#################################

APP = splign
SRC = seq_loader splign_app

LIB = xalgoalignsplign xalgoalignutil xalgoalignnw \
      xblast xalgodustmask seqdb xnetblastcli blastdb \
      xnetblast scoremat xobjutil xobjread tables \
      $(OBJMGR_LIBS)
#      $(OBJMGR_LIBS:%=%$(STATIC))

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CXXFLAGS = $(FAST_CXXFLAGS)
#CXXFLAGS = $(FAST_CXXFLAGS) -D GENOME_PIPELINE
LDFLAGS  = $(FAST_LDFLAGS)
