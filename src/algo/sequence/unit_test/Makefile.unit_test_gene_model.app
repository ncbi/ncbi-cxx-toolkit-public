# $Id$

APP = unit_test_gene_model

SRC = unit_test_gene_model

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB = xalgoseq xalnmgr xobjread xvalidate valerr submit valid taxon3 xalnmgr $(XFORMAT_LIBS) xobjutil taxon1 xregexp tables test_boost $(OBJMGR_LIBS) $(PCRE_LIB) 
LIBS = $(PCRE_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(CMPRS_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test.Included

# Uncomment if you do not want it to run automatically as part of
# "make check".
CHECK_CMD = unit_test_gene_model -data-in alignments.asn -data-expected annotations.asn -seqdata-expected seqdata.asn -combined-data-expected combined_annot.asn -combined-with-omission-expected combined_with_omission.asn
CHECK_COPY = alignments.asn annotations.asn seqdata.asn combined_annot.asn combined_with_omission.asn

WATCHERS = dicuccio chetvern mozese2
