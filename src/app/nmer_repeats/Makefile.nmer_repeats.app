# $Id$
# Author:  Josh Cherry

# Build n-mer nucleotide repeat finder app

APP = nmer_repeats
SRC = nmer_repeats
LIB = xalgoseq xalnmgr tables xregexp $(PCRE_LIB) xobjread xobjutil \
      $(OBJMGR_LIBS)
LIBS = $(PCRE_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

REQUIRES = objects dbapi
