# $Id$

CPPFLAGS = $(BOOST_INCLUDE) $(ORIG_CPPFLAGS)

APP = test_netstorage
SRC = test_netstorage
LIB = ncbi_xcache_netcache xconnserv netstorage \
        xthrserv xconnect connssl xser xutil test_boost xncbi

LIBS = $(GNUTLS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test.Included

CHECK_REQUIRES = in-house-resources
CHECK_CMD = test_netstorage
CHECK_TIMEOUT = 800

WATCHERS = kazimird
