# $Id$
# Author:  Josh Cherry

# Build n-mer nucleotide repeat finder app

APP = nmer_repeats
SRC = nmer_repeats
LIB = xalgoseq xregexp regexp xobjread xobjutil $(OBJMGR_LIBS)
LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

REQUIRES = objects dbapi
