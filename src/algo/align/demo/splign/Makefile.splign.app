#################################
# $Id$
#################################

# Build demo "splign"
#################################

APP = splign
SRC = seq_loader splign_app

#LIB = xalgoalign tables $(OBJMGR_LIBS)
LIB = xalgoalign tables $(OBJMGR_LIBS:%=%$(STATIC))

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)
