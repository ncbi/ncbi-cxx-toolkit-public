# $Id$

CPPFLAGS = $(BOOST_INCLUDE) $(ORIG_CPPFLAGS)

APP = test_ic_client
SRC = test_ic_client
LIB = ncbi_xcache_netcache xconnserv xthrserv xconnect xutil test_boost xncbi

LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test.Included

CHECK_REQUIRES = in-house-resources
CHECK_CMD = test_ic_client

WATCHERS = sadyrovr gouriano
