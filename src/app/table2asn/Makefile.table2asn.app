#################################
# $Id$
# Author:  Colleen Bollin, based on file by Mati Shomrat
#################################

# Build application "table2asn"
#################################

APP = table2asn
SRC = table2asn OpticalXML2ASN multireader struc_cmt_reader table2asn_context feature_table_reader \
      remote_updater fcs_reader table2asn_validator

LIB  = xalgophytree fastme prosplign xalgoalignutil xalgoseq xmlwrapp \
       xcleanup xvalidate xobjreadex valid valerr taxon3 taxon1 biotree \
       xobjedit \
       $(XFORMAT_LIBS) $(BLAST_LIBS) xregexp $(PCRE_LIB) $(OBJMGR_LIBS)


LIBS = $(LIBXSLT_LIBS) $(LIBXML_LIBS) $(CMPRS_LIBS) $(PCRE_LIBS) \
       $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects LIBXML LIBXSLT -Cygwin


WATCHERS = bollin gotvyans

