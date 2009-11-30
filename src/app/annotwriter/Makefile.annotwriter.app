#################################
# $Id$
# Author:  Mati Shomrat
#################################

# Build application "annotwriter"
#################################

APP = annotwriter
SRC = annotwriter
LIB = xformat xobjutil submit gbseq xalnmgr xcleanup entrez2cli entrez2 tables $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects -Cygwin

