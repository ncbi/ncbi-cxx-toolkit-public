#################################
# $Id$
# Author:  Yuri Kapustin (kapustin@ncbi.nlm.nih.gov)
#################################

# Build demo "demo_nwa"
#################################

APP = demo_nwa
SRC = nwa starter
LIB = xalgo xncbi

CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)
