#################################
# $Id$
# Author:  Colleen Bollin, based on file by Mati Shomrat
#################################

# Build application "biosample_chk"
#################################

APP = biosample_chk
SRC = biosample_chk util src_table_column struc_table_column
LIB = ncbi_xloader_wgs $(SRAREAD_LIBS) xvalidate xobjedit xcleanup $(XFORMAT_LIBS) \
      xalnmgr xobjutil valid valerr submit taxon3 gbseq \
      tables xregexp xmlwrapp xser $(PCRE_LIB) $(OBJMGR_LIBS)

LIBS = $(PCRE_LIBS) $(SRA_SDK_SYSLIBS) $(ORIG_LIBS) $(LIBXML_LIBS) $(LIBXSLT_LIBS)

REQUIRES = objects -BSD -Cygwin LIBXML LIBXSLT


WATCHERS = bollin
