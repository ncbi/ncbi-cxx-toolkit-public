#################################
# $Id$
# Author:  Colleen Bollin, based on file by Mati Shomrat
#################################

# Build application "unit_test_hugeannot"
#################################

APP = unit_test_hugeannot

SRC = unit_test_hugeannot annot_match_5col annot_match

CPPFLAGS = $(ORIG_CPPFLAGS) $(BOOST_INCLUDE)

LIB  = test_boost xhugeasn $(DATA_LOADERS_UTIL_LIB) $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(DATA_LOADERS_UTIL_LIBS) $(GENBANK_THIRD_PARTY_LIBS) \
       $(LMDB_LIBS) $(ZSTD_LIBS) -llzo2 $(ORIG_LIBS)

POST_LINK = $(VDB_POST_LINK)

REQUIRES = objects LIBXML Boost.Test.Included

CHECK_CMD = unit_test_hugeannot

WATCHERS = stakhovv gotvyans foleyjp
