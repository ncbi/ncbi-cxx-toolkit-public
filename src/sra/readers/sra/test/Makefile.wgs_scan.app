#################################
# $Id$
# Author:  Eugene Vasilchenko
#################################

# Build application "wgs_test"
#################################

APP = wgs_scan
SRC = wgs_scan

LIB =   $(SRAREAD_LIBS) $(SOBJMGR_LIBS) $(CMPRS_LIB)
LIBS =  $(SRA_SDK_SYSLIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)
POST_LINK = $(VDB_POST_LINK)

REQUIRES = objects VDB

CPPFLAGS = $(ORIG_CPPFLAGS) $(SRA_INCLUDE)

WATCHERS = vasilche
