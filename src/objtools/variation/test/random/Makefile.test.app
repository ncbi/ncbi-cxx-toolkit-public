#########################################################################
# This file was originally generated from by shell script "new_project.sh"
#########################################################################
APP = test
SRC = test

LIB = variation_utils  gencoll_client genome_collection hgvs xregexp entrez2cli entrez2 ncbi_xloader_genbank ncbi_xreader_id1 ncbi_xreader_id2 ncbi_xreader_cache ncbi_xreader submit ncbi_xcache_netcache xconnserv  ncbi_xloader_genbank ncbi_xreader_id1 ncbi_xreader_id2 ncbi_xloader_blastdb seqdb blastdb ncbi_xreader_cache ncbi_xreader seqsplit id1 id2 refseq_aln_source variation xobjutil $(SOBJMGR_LIBS) $(SEQ_LIBS) xconnect xcompress $(CMPRS_LIB)  $(PCRE_LIB)  $(OBJREAD_LIBS) seq xutil xncbi
LIBS =  $(DL_LIBS) $(CMPRS_LIBS) $(PCRE_LIBS) 
LDFLAGS = -pthread -L../../../../../lib/ 