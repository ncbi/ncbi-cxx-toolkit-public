# $Id$

APP = agp_validate
SRC = agp_validate AltValidator

LIB = entrez2cli entrez2 taxon1 xobjutil xobjread \
      $(OBJMGR_LIBS:%=%$(STATIC))

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects -Cygwin
CXXFLAGS = $(ORIG_CXXFLAGS)
# -Wno-parentheses

WATCHERS = sapojnik
