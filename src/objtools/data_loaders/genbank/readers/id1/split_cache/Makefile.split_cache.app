#################################
# $Id$
# Author:  Eugene Vasilchenko
#################################

# Build application for splitting blobs withing ID1 cache
#################################

REQUIRES = dbapi

APP = split_cache
SRC = split_cache \
	blob_splitter blob_splitter_params splitted_blob \
	blob_splitter_impl blob_splitter_parser blob_splitter_maker \
	id_range object_splitinfo asn_sizer annot_piece chunk_info size
LIB = bdb $(OBJMGR_LIBS)

LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(BERKELEYDB_LIBS) $(ORIG_LIBS)
