# $Id$

CPPFLAGS = $(BOOST_INCLUDE) $(ORIG_CPPFLAGS)

APP = test_netstorage
SRC = test_netstorage test_netstorage_common
LIB = ncbi_xcache_netcache netstorage xconnserv \
        xthrserv xconnect connssl xutil test_boost xncbi

LIBS = $(GNUTLS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

REQUIRES = Boost.Test.Included GNUTLS

CHECK_REQUIRES = in-house-resources GNUTLS
CHECK_CMD = test_netstorage
CHECK_TIMEOUT = 800

WATCHERS = kazimird
