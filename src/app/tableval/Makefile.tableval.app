#################################
# $Id$
# Author:  Colleen Bollin, based on file by Mati Shomrat
#################################

# Build application "tableval"
#################################

APP = tableval
SRC = tableval tab_table_reader


LIB  = xmlwrapp \
       xobjreadex  \
       $(XFORMAT_LIBS) $(BLAST_LIBS) xregexp $(PCRE_LIB) $(OBJMGR_LIBS)

#LIB  = xalgophytree fastme prosplign xalgoalignutil xalgoseq xmlwrapp \
#       xcleanup xvalidate xobjreadex valid valerr taxon3 taxon1 biotree \
#       xobjedit \
#       $(XFORMAT_LIBS) $(BLAST_LIBS) xregexp $(PCRE_LIB) $(OBJMGR_LIBS)


LIBS = $(LIBXSLT_LIBS) $(LIBXML_LIBS) $(CMPRS_LIBS) $(PCRE_LIBS) \
       $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

#REQUIRES = objects LIBXML LIBXSLT -Cygwin


WATCHERS = gotvyans

