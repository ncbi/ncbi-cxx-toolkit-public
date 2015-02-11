#########################################################################
# This file was originally generated from by shell script "new_project.sh"
#########################################################################
APP = analyze-shift
SRC = analyze-shift

LIB = variation_utils  gencoll_client hgvs xregexp entrez2cli entrez2 ncbi_xloader_genbank ncbi_xreader_id1 ncbi_xreader_id2 ncbi_xreader_cache ncbi_xreader seqsplit id1 id2 variation xobjutil $(SOBJMGR_LIBS) xconnect xcompress $(OBJREAD_LIBS) seq xutil xncbi
LIBS =  $(DL_LIBS) $(CMPRS_LIBS) $(PCRE_LIBS) $(ORIG_LIBS)
LDFLAGS = -pthread -L../../../../lib/ 
