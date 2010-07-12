# $Id$

APP = unit_test_gene_model

SRC = unit_test_gene_model

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB = xalgoseq xalnmgr xobjutil taxon1 xregexp tables test_boost $(OBJMGR_LIBS)
LIBS = $(PCRE_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(CMPRS_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test.Included

# Uncomment if you do not want it to run automatically as part of
# "make check".
CHECK_CMD = unit_test_gene_model -data-in alignments.asn -data-expected annotations.asn
CHECK_COPY = alignments.asn annotations.asn

WATCHERS = dicuccio chetvern
