#################################
# $Id$
#################################

# Build demo "splign"
#################################

APP = splign
SRC = subjmixer splign seq_loader splign_app util \
      hf_hit hf_hitparser

LIB = xalgoalign tables $(OBJMGR_LIBS:%=%$(STATIC))

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)
