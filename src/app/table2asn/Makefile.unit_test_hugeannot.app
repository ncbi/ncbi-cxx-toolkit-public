#################################
# $Id$
# Author:  Colleen Bollin, based on file by Mati Shomrat
#################################

# Build application "table2asn"
#################################

APP = unit_test_hugeannot

SRC = unit_test_hugeannot annot_match_5col annot_match

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB  = test_boost xhugeasn xdiscrepancy prosplign xmlwrapp xvalidate xobjwrite xflatfile $(DATA_LOADERS_UTIL_LIB)

LIBS = $(ORIG_LIBS)
#       $(BLAST_THIRD_PARTY_STATIC_LIBS) $(GENBANK_THIRD_PARTY_LIBS) \
#       $(LIBXSLT_STATIC_LIBS) $(LIBXML_STATIC_LIBS) $(BERKELEYDB_STATIC_LIBS) \
#       $(SQLITE3_STATIC_LIBS) $(VDB_STATIC_LIBS) $(FTDS_LIBS) \
#       $(CMPRS_LIBS) $(PCRE_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

#POST_LINK = $(VDB_POST_LINK)

REQUIRES = objects LIBXML Boost.Test.Included

WATCHERS = stakhovv gotvyans foleyjp
