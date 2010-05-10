#################################
# $Id$
# Author:  Eugene Vasilchenko
#################################

# Build application "wig2table"
#################################

APP = wig2table
SRC = wig2table
LIB = xobjreadex xobjutil $(OBJMGR_LIBS)
LIBS = $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = objects

WATCHERS = vasilche
