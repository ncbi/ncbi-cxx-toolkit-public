# $Id$
#################################
# Build demo "hfilter"

APP = hfilter
SRC = hitfilter_app

LIB = xalgoalignutil $(SOBJMGR_LDEP)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)
