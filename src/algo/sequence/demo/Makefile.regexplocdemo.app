# $Id$

SRC = regexplocdemo
APP = regexplocdemo

CPPFLAGS = $(ORIG_CPPFLAGS)
CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)

LIB = xalgoseq xregexp regexp xobjutil $(OBJMGR_LIBS)

LIBS = $(ORIG_LIBS) $(DL_LIBS) $(NETWORK_LIBS)
