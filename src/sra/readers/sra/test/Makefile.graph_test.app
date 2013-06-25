#################################
# $Id$
# Author:  Eugene Vasilchenko
#################################

# Build application "graph_test"
#################################

APP = graph_test
SRC = graph_test

LIB =   $(SRAREAD_LIBS) $(SOBJMGR_LIBS) $(CMPRS_LIB)
LIBS =  $(SRA_SDK_SYSLIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

REQUIRES = objects

CPPFLAGS = $(ORIG_CPPFLAGS) $(SRA_INCLUDE)

CHECK_REQUIRES = -AIX -BSD -MSWin -Solaris
CHECK_CMD = graph_test -q .

WATCHERS = vasilche ucko
