#################################
# $Id$
#################################

APP = netcache_check
SRC = netcache_check

LIB = xconnserv$(FORCE_STATIC) xconnect xutil xncbi
LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)

CHECK_CMD  = netcache_check.sh
CHECK_COPY = netcache_check.sh
