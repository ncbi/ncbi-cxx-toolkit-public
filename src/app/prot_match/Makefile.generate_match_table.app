#################################
# $Id$
# Author:  Justin Foley
#################################

# Build application "generate_match_table"
#################################

APP = generate_match_table
SRC = generate_match_table
LIB = xobjwrite variation_utils $(OBJREAD_LIBS) xalnmgr xobjutil \
      gbseq entrez2cli entrez2 tables $(OBJMGR_LIBS)

LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects -Cygwin


WATCHERS = foleyjp
