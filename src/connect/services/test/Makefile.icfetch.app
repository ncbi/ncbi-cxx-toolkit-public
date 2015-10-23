# $Id$

APP = icfetch
SRC = icfetch
LIB = ncbi_xcache_netcache xconnserv xthrserv xconnect xutil xncbi

LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CHECK_REQUIRES = in-house-resources
CHECK_CMD = test_ic_client -service NC_UnitTest blobs

WATCHERS = sadyrovr gouriano
