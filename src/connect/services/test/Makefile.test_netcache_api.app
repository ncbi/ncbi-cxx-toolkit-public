# $Id $

APP = test_netcache_api
SRC = test_netcache_api
LIB = xconnserv xthrserv xconnect xutil xncbi

LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)


CHECK_REQUIRES = in-house-resources
CHECK_CMD = test_netcache_api -repeat 1 NC_UnitTest
CHECK_TIMEOUT = 400

WATCHERS = kazimird ivanovp
