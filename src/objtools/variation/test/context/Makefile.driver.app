# $Id$
	
SRC = driver
APP = driver

LIB = variation_utils ncbi_xloader_genbank ncbi_xreader_id1 ncbi_xreader_id2 ncbi_xreader_cache ncbi_xreader seqsplit id1 id2 variation xobjutil $(SOBJMGR_LIBS) xconnect xcompress xmlwrapp
LIBS =  $(CMPRS_LIBS) $(LIBXSLT_LIBS)
LDFLAGS = -pthread -L../../../../../lib/ 

WATCHERS = filippov