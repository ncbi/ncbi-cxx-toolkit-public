#################################
# $Id$
# Author:  Eugene Vasilchenko
#################################

# Build application for splitting blobs withing ID1 cache
#################################

REQUIRES = bdb

APP = split_cache
SRC = split_cache
LIB = bdb id2_split $(GENBANK_LIBS) $(GENBANK_READER_ID1C_LIBS)

LIBS += $(BERKELEYDB_LIBS)
