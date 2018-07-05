# $Id$

APP = pubseq_gateway
SRC = pubseq_gateway acc_ver_cache_db acc_ver_cache_storage \
      pending_operation http_server_transport pubseq_gateway_exception \
      uv_extra pubseq_gateway_utils pubseq_gateway_stat

LIBS = $(CASSANDRA_STATIC_LIBS) $(LIBUV_STATIC_LIBS) $(H2O_STATIC_LIBS) $(LMDB_LIBS) $(NGHTTP2_STATIC_LIBS) $(ORIG_LIBS)
CPPFLAGS = $(CASSANDRA_INCLUDE) $(H2O_INCLUDE) $(LMDB_INCLUDE) $(ORIG_CPPFLAGS)
LIB = psg_rpc psg_cassandra connext xconnserv xconnect xutil xncbi

REQUIRES = CASSANDRA MT Linux H2O LMDB LIBUV NGHTTP2

WATCHERS = satskyse dmitrie1
