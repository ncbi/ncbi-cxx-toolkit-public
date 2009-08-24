# $Id$

APP = unit_test_gene_model

SRC = unit_test_gene_model

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB = xalgoseq test_boost xobjutil $(OBJMGR_LIBS) $(COMPRESS_LIBS)
LIBS = $(DL_LIBS) $(CMPRS_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test.Included

# Uncomment if you do not want it to run automatically as part of
# "make check".
CHECK_CMD = unit_test_gene_model -data-in alignments.asn -data-expected annotations.asn
CHECK_COPY = alignments.asn annotations.asn

