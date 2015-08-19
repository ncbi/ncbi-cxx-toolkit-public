#################################
# $Id$
# Author:  Colleen Bollin, based on file by Mati Shomrat
#################################

# Build application "table2asn"
#################################

APP = table2asn
SRC = table2asn OpticalXML2ASN multireader struc_cmt_reader table2asn_context feature_table_reader \
      fcs_reader table2asn_validator

LIB  = xalgophytree fastme prosplign xalgoalignutil xalgoseq xmlwrapp \
       xcleanup xvalidate xobjreadex valerr biotree \
       xobjwrite ncbi_xloader_wgs \
       $(OBJEDIT_LIBS) $(XFORMAT_LIBS) $(BLAST_LIBS) \
       xregexp $(PCRE_LIB) $(OBJMGR_LIBS) $(SRAREAD_LIBS)

LIBS = $(LIBXSLT_LIBS) $(LIBXML_LIBS) $(CMPRS_LIBS) $(PCRE_LIBS) \
       $(SRA_SDK_SYSLIBS) \
       $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects LIBXML LIBXSLT


WATCHERS = bollin gotvyans

