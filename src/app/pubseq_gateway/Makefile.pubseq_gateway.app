# $Id$

APP = pubseq_gateway
SRC = pubseq_gateway AccVerCacheDB AccVerCacheStorage pending_operation HttpServerTransport

LIBS = $(CASSANDRA_STATIC_LIBS) $(LIBUV_STATIC_LIBS) $(H2O_STATIC_LIBS) $(LMDB_LIBS) $(NGHTTP2_STATIC_LIBS) $(ORIG_LIBS)
CPPFLAGS = $(CASSANDRA_INCLUDE) $(H2O_INCLUDE) $(LMDB_INCLUDE) $(ORIG_CPPFLAGS)
LIB = psg_diag psg_rpc psg_cassandra xconnect xncbi

REQUIRES = CASSANDRA MT Linux H2O LMDB LIBUV NGHTTP2

WATCHERS = satskyse dmitrie1
