#################################
# $Id$
# Author:  Eugene Vasilchenko
#################################

# Build application "csra_test_mt"
#################################

APP = csra_test_mt
SRC = csra_test_mt

LIB = test_mt $(SRAREAD_LIBS) $(SOBJMGR_LIBS) $(CMPRS_LIB)
LIBS =  $(SRA_SDK_SYSLIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)
POST_LINK = $(VDB_POST_LINK)

REQUIRES = objects

CPPFLAGS = $(ORIG_CPPFLAGS) $(SRA_INCLUDE)

CHECK_REQUIRES = -AIX -BSD -MSWin -Solaris
#CHECK_CMD = csra_test_mt

WATCHERS = vasilche
