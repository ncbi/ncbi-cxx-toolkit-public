#################################
# $Id$
# Author:  Eugene Vasilchenko
#################################

# Build application "snp_test"
#################################

APP = snp_test
SRC = snp_test

LIB =   $(SRAREAD_LIBS) $(SOBJMGR_LIBS) $(CMPRS_LIB)
LIBS =  $(SRA_SDK_SYSLIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)
POST_LINK = $(VDB_POST_LINK)

REQUIRES = objects

CPPFLAGS = $(ORIG_CPPFLAGS) $(SRA_INCLUDE)

CHECK_REQUIRES = in-house-resources -MSWin -Solaris
#CHECK_CMD = snp_test

WATCHERS = vasilche
