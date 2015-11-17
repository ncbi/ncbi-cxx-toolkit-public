# $Id$

CPPFLAGS = $(BOOST_INCLUDE) $(ORIG_CPPFLAGS)

APP = test_netstorage
SRC = test_netstorage
LIB = netstorage ncbi_xcache_netcache xconnserv \
        xthrserv xconnect connssl xutil test_boost xncbi

LIBS = $(GNUTLS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test.Included

CHECK_REQUIRES = in-house-resources GNUTLS
CHECK_CMD = test_netstorage
CHECK_COPY = test_netstorage.ini
CHECK_TIMEOUT = 3000

WATCHERS = sadyrovr
