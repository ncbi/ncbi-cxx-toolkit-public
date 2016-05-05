#################################
# $Id$
# Author:  Colleen Bollin, based on file by Mati Shomrat
#################################

# Build application "tableval"
#################################

APP = tableval
SRC = tableval tab_table_reader col_validator

LIB  = xmlwrapp \
       xobjreadex $(XFORMAT_LIBS) taxon1 \
       $(BLAST_LIBS) xregexp $(PCRE_LIB) $(OBJMGR_LIBS)

LIBS = $(LIBXSLT_LIBS) $(LIBXML_LIBS) $(CMPRS_LIBS) $(PCRE_LIBS) \
       $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects LIBXSLT

WATCHERS = gotvyans

