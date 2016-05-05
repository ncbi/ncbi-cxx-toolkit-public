# $Id$

WATCHERS = astashya

SRC = xcompareannotsdemo
APP = xcompareannotsdemo

CPPFLAGS = $(ORIG_CPPFLAGS)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

LIB  = xalgoseq xalnmgr $(OBJREAD_LIBS) xobjutil taxon1 \
       tables xregexp $(PCRE_LIB) $(OBJMGR_LIBS)

LIBS = $(PCRE_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)
