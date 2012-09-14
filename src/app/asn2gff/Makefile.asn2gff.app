#################################
# $Id$
# Author:  Mati Shomrat
#################################

# Build application "asn2gff"
#################################

APP = asn2gff
SRC = asn2gff
LIB = $(XFORMAT_LIBS) xobjutil xalnmgr entrez2cli entrez2 tables $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects -Cygwin


WATCHERS = ludwigf
