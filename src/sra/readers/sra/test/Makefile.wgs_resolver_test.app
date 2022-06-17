#################################
# $Id$
# Author:  Eugene Vasilchenko
#################################

# Build application "wgs_test"
#################################

APP = wgs_resolver_test
SRC = wgs_resolver_test

LIB =   $(SRAREAD_LIBS) $(SOBJMGR_LIBS) $(CMPRS_LIB)
LIBS =  $(SRA_SDK_SYSLIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)
POST_LINK = $(VDB_POST_LINK)

REQUIRES = objects MT

CPPFLAGS = $(ORIG_CPPFLAGS) $(SRA_INCLUDE)

CHECK_REQUIRES = in-house-resources

WATCHERS = vasilche ucko
