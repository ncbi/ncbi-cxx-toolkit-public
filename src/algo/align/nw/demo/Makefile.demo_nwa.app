#################################
# $Id$
# Author:  Yuri Kapustin (kapustin@ncbi.nlm.nih.gov)
#################################

# Build demo "demo_nwa"
#################################

APP = demo_nwa
SRC = nwa starter
LIB = xncbi xalgo

CXXFLAGS = $(ORIG_CXXFLAGS) $(FAST_CXXFLAGS)
LDFLAGS  = $(ORIG_LDFLAGS) $(FAST_LDFLAGS)
