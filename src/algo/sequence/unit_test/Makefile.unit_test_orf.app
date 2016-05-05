# $Id$

APP = unit_test_orf

SRC = unit_test_orf

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB  = xalgoseq taxon1 xalnmgr xobjutil tables xregexp test_boost $(PCRE_LIB) $(OBJMGR_LIBS)
LIBS = $(PCRE_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(CMPRS_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test.Included

# Uncomment if you do not want it to run automatically as part of
# "make check".
CHECK_CMD = unit_test_orf -in orf.annot.asn
CHECK_COPY = orf.annot.asn

WATCHERS = astashya
