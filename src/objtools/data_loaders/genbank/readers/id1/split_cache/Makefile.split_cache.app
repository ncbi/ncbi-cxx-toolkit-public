#################################
# $Id$
# Author:  Eugene Vasilchenko
#################################

# Build application for splitting blobs withing ID1 cache
#################################

REQUIRES = bdb BerkeleyDB

APP = split_cache
SRC = split_cache
LIB = id2_split bdb $(OBJMGR_LIBS) $(GENBANK_READER_ID1C_LIBS)

LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(BERKELEYDB_LIBS) $(ORIG_LIBS)
