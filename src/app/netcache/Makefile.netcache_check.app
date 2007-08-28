#################################
# $Id$
#################################

APP = netcache_check
SRC = netcache_check

LIB = xconnserv xthrserv xconnect xutil xncbi
LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CHECK_CMD     = netcache_check.sh
CHECK_COPY    = netcache_check.sh
CHECK_TIMEOUT = 250
CHECK_AUTHORS = ivanov