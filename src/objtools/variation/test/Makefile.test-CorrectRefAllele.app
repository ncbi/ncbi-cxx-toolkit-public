#########################################################################
# This file was originally generated from by shell script "new_project.sh"
#########################################################################
APP = test-CorrectRefAllele
SRC = test-CorrectRefAllele

LIB = CorrectRefAllele ncbi_xcache_netcache xconnserv  ncbi_xloader_genbank ncbi_xreader_id1 ncbi_xreader_id2 ncbi_xloader_blastdb seqdb blastdb ncbi_xreader_cache ncbi_xreader seqsplit id1 id2 refseq_aln_source hgvs variation xobjutil $(SOBJMGR_LIBS) $(SEQ_LIBS) xconnect xcompress $(CMPRS_LIB)
LIBS =  $(DL_LIBS) $(CMPRS_LIBS)
LDFLAGS = -pthread -L../../../../lib/ 