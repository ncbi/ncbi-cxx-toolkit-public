# $Id$

APP = test_netcache_api
SRC = test_netcache_api
LIB = xconnserv xthrserv xconnect xutil xncbi

LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)


CHECK_REQUIRES = in-house-resources
CHECK_CMD = test_netcache_api.sh
CHECK_COPY = test_netcache_api.sh
CHECK_TIMEOUT = 800

WATCHERS = kazimird ivanovp
