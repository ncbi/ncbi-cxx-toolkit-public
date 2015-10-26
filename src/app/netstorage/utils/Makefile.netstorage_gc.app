#################################
# $Id$
#################################

WATCHERS = satskyse

APP = netstorage_gc
SRC = netstorage_gc netstorage_gc_database netstorage_gc_exception

REQUIRES = MT Linux


LIB =  netstorage ncbi_xcache_netcache xconnserv xthrserv \
       $(SDBAPI_LIB) xconnect connssl xutil xncbi
LIBS = $(SDBAPI_LIBS) $(NETWORK_LIBS) $(GNUTLS_LIBS) $(CMPRS_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(ORIG_CPPFLAGS)

