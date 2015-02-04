# $Id$

CPPFLAGS = $(BOOST_INCLUDE) $(ORIG_CPPFLAGS)

APP = test_netstorage_rpc
SRC = test_netstorage_common
LIB = xconnserv xthrserv xconnect xutil test_boost xncbi

LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test.Included

CHECK_REQUIRES = in-house-resources
# CHECK_CMD = test_netstorage_rpc
CHECK_TIMEOUT = 800

WATCHERS = kazimird sadyrovr
