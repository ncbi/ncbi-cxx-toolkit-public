# $Id$
	
SRC = driver
APP = driver

LIB = variation_utils  submit ncbi_xcache_netcache xconnserv  ncbi_xloader_genbank ncbi_xreader_id1 ncbi_xreader_id2 ncbi_xloader_blastdb seqdb blastdb ncbi_xreader_cache ncbi_xreader seqsplit id1 id2 refseq_aln_source hgvs variation xobjutil $(SOBJMGR_LIBS) $(SEQ_LIBS) xconnect xcompress $(CMPRS_LIB) xmlwrapp xncbi
LIBS =  $(DL_LIBS) $(CMPRS_LIBS) $(ORIG_LIBS) $(LIBXML_LIBS) $(LIBXSLT_LIBS)
CPPFLAGS= $(ORIG_CPPFLAGS) $(LIBXML_INCLUDE) $(LIBXSLT_INCLUDE)
LDFLAGS = -pthread -L../../../../../lib/ 

WATCHERS = filippov