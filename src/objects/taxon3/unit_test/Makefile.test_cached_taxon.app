# $Id$

APP = test_cached_taxon
SRC = test_cached_taxon

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB  = test_boost taxon3 $(OBJMGR_LIBS) \
       $(OBJEDIT_LIBS)

LIBS = $(CMPRS_LIBS) $(PCRE_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test.Included

CHECK_CMD =
CHECK_COPY = test_cached_taxon.ini
CHECK_TIMEOUT = 3000

WATCHERS = bollin kans kornbluh
