# $Id$
# Author:  Yuri Kapustin

# Generic pairwise global alignment utility

APP = nw_aligner
SRC = nwa

LIB = xalgoalignnw tables $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)
