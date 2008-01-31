#################################
# $Id$
# Author:  Mati Shomrat
#################################

# Build application "asn2fasta"
#################################

APP = asn2fasta
SRC = asn2fasta
LIB = xformat xobjutil submit gbseq xalnmgr xcleanup entrez2cli entrez2 tables $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects -Cygwin

