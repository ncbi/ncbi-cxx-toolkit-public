#################################
# $Id$
# Author:  Mati Shomrat
#################################

# Build application "asn2fasta"
#################################

APP = asn2fasta
SRC = asn2fasta
LIB = $(XFORMAT_LIBS) ncbi_xloader_wgs $(SRAREAD_LIBS) xobjutil xalnmgr xcleanup xregexp entrez2cli entrez2 tables $(OBJMGR_LIBS) $(PCRE_LIB)

LIBS = $(SRA_SDK_SYSLIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(PCRE_LIBS) $(ORIG_LIBS)

REQUIRES = objects -BSD -Cygwin


WATCHERS = ludwigf kornbluh
