#################################
# $Id$
# Author:  Mati Shomrat
#################################

# Build application "asn2fasta"
#################################

APP = asn2fasta
SRC = asn2fasta
LIB = $(XFORMAT_LIBS) xobjutil xalnmgr xcleanup xregexp entrez2cli entrez2 tables $(OBJMGR_LIBS) $(PCRE_LIB)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(PCRE_LIBS) $(ORIG_LIBS)

REQUIRES = objects -Cygwin


WATCHERS = ludwigf
