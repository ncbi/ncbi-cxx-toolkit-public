#################################
# $Id$
# Author:  Colleen Bollin, based on file by Mati Shomrat
#################################

# Build application "table2asn"
#################################

APP = table2asn

SRC = table2asn multireader struc_cmt_reader table2asn_context feature_table_reader \
      fcs_reader table2asn_validator src_quals fasta_ex suspect_feat descr_apply

LIB  = xdiscrepancy xalgophytree fastme prosplign xalgoalignutil xalgoseq \
	   xflatfile fix_pub eutils_client xmlwrapp \
       xvalidate xobjwrite xobjreadex valerr biotree macro ctransition \
       $(OBJEDIT_LIBS) $(XFORMAT_LIBS) $(BLAST_LIBS) taxon1 xobjutil id2cli \
	   $(SEQ_LIBS) pub medline general xser xconnect xutil xncbi \
	   xregexp $(PCRE_LIB) xqueryparse $(DATA_LOADERS_UTIL_LIB) $(OBJMGR_LIBS) 

LIBS = $(GENBANK_THIRD_PARTY_LIBS) \
       $(LIBXSLT_STATIC_LIBS) $(LIBXML_STATIC_LIBS) $(BERKELEYDB_STATIC_LIBS) \
       $(SQLITE3_STATIC_LIBS) $(VDB_STATIC_LIBS) $(FTDS_LIBS) \
       $(CMPRS_LIBS) $(PCRE_LIBS) $(NETWORK_LIBS) $(BLAST_THIRD_PARTY_LIBS) $(ORIG_LIBS)

POST_LINK = $(VDB_POST_LINK)

REQUIRES = objects LIBXML LIBXSLT BerkeleyDB SQLITE3

WATCHERS = bollin gotvyans foleyjp
