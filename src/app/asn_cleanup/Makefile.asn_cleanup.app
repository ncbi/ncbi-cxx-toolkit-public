#################################
# $Id$
# Author:  Colleen Bollin, based on file by Mati Shomrat
#################################

# Build application "asn_cleanup"
#################################

APP = asn_cleanup
SRC = asn_cleanup
LIB = xformat xobjutil submit gbseq xalnmgr xcleanup xregexp entrez2cli \
    entrez2 tables $(OBJMGR_LIBS) $(PCRE_LIBS)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(PCRE_LIBS) $(ORIG_LIBS)

REQUIRES = objects -Cygwin


WATCHERS = bollin
