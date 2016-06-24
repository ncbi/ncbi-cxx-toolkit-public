# $Id$

APP = sub_cache_create
SRC = sub_cache_create

LIB = asn_cache  \
      $(ncbi_xloader_wgs) $(SRAREAD_LIBS) \
      bdb $(OBJMGR_LIBS)
LIBS = $(SRA_SDK_SYSLIBS) $(BERKELEYDB_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

WATCHERS = marksc2
