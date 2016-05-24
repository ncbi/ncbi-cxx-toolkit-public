# $Id$

APP = asn_cache_test
SRC = asn_cache_test

LIB = asn_cache ncbi_xloader_asn_cache \
	   bdb xser xconnect \
	  $(COMPRESS_LIBS) $(SOBJMGR_LIBS)
LIBS = $(BERKELEYDB_LIBS) $(CMPRS_LIBS) $(DL_LIBS) $(ORIG_LIBS)

WATCHERS = marksc2
