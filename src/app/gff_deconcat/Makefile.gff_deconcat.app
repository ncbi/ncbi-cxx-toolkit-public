#################################
# $Id$
# Author:  Justin Foley
#################################

# Build application "gff_deconcat"
#################################

APP = gff_deconcat
SRC = gff_deconcat
LIB = $(OBJREAD_LIBS) xobjutil $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects -Cygwin


WATCHERS = foleyjp
