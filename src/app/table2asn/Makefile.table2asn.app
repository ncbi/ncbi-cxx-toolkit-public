#################################
# $Id$
# Author:  Colleen Bollin, based on file by Mati Shomrat
#################################

# Build application "table2asn"
#################################

APP = table2asn
SRC = table2asn OpticalXML2ASN multireader struc_cmt_reader table2asn_context feature_table_reader \
      fcs_reader table2asn_validator src_quals

LIB  = xdiscrepancy xalgophytree fastme prosplign xalgoalignutil xalgoseq xmlwrapp \
       xvalidate xobjwrite xobjreadex valerr biotree \
       $(ncbi_xloader_wgs) \
       $(OBJEDIT_LIBS) $(XFORMAT_LIBS) $(BLAST_LIBS) id2cli \
       xregexp $(PCRE_LIB) $(SRAREAD_LIBS) $(OBJMGR_LIBS)

LIBS = $(LIBXSLT_LIBS) $(LIBXML_LIBS) $(CMPRS_LIBS) $(PCRE_LIBS) \
       $(SRA_SDK_SYSLIBS) \
       $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

POST_LINK = $(VDB_POST_LINK)

REQUIRES = objects LIBXML LIBXSLT


WATCHERS = bollin gotvyans
