#################################
# $Id$
# Author:  Eugene Vasilchenko
#################################

# Build application "snp_test"
#################################

APP = snp_ptis_test
SRC = snp_ptis_test

LIB = dbsnp_ptis grpc_integration $(SOBJMGR_LIBS)
LIBS = $(GRPC_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(ORIG_CPPFLAGS) $(GRPC_INCLUDE)

CHECK_REQUIRES = in-house-resources -MSWin -Solaris
CHECK_CMD = snp_ptis_test

WATCHERS = vasilche
