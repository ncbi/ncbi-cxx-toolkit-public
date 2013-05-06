# $Id$

APP = unit_test_splign

SRC = unit_test_splign

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB = xalgoalignsplign xalgoalignutil xalgoalignnw \
    xqueryparse xalgoseq\
    $(BLAST_LIBS) \
    xobjutil test_boost $(OBJMGR_LIBS)

LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(CMPRS_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test.Included

# Uncomment if you do not want it to run automatically as part of
# "make check".
CHECK_CMD = unit_test_splign -data-in polya.splign_in.asn -data-expected splign_expected.asn
CHECK_COPY = slign_in.asn splign_expected.asn

WATCHERS = kiryutin

