#################################
# $Id$
# Author:  Eugene Vasilchenko
#################################

# Build application "snp_test"
#################################

APP = snp_ptis_test
SRC = snp_ptis_test

REQUIRES = $(GRPC_OPT)

LIB = dbsnp_ptis grpc_integration $(SEQ_LIBS) pub medline biblio general \
      xser $(XCONNEXT) xconnect xutil xncbi
LIBS = $(GRPC_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(ORIG_CPPFLAGS) $(GRPC_INCLUDE)

CHECK_COPY = fix_ptis_url.sh
CHECK_REQUIRES = in-house-resources -MSWin -Solaris
CHECK_CMD = fix_ptis_url.sh snp_ptis_test

WATCHERS = vasilche
