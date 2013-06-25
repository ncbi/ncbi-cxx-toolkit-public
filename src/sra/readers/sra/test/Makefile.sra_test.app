#################################
# $Id$
# Author:  Eugene Vasilchenko
#################################

# Build application "sra_test"
#################################

APP = sra_test
SRC = sra_test

LIB =   $(SRAREAD_LIBS) $(OBJMGR_LIBS)
LIBS =  $(SRA_SDK_SYSLIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

REQUIRES = objects

CPPFLAGS = $(ORIG_CPPFLAGS) $(SRA_INCLUDE)

CHECK_CMD = sra_test

WATCHERS = vasilche ucko
