# $Id$

ASN_DEP = psg_protobuf

APP = pubseq_gateway
SRC = pubseq_gateway  \
      pending_operation http_server_transport pubseq_gateway_exception \
      uv_extra pubseq_gateway_utils pubseq_gateway_stat \
      pubseq_gateway_handlers

LIBS = $(H2O_LIBS) $(CASSANDRA_STATIC_LIBS) $(LIBUV_STATIC_LIBS) $(LMDB_LIBS) $(NGHTTP2_STATIC_LIBS) $(PROTOBUF_LIBS) $(ORIG_LIBS)
CPPFLAGS = $(CASSANDRA_INCLUDE) $(H2O_INCLUDE) $(LMDB_INCLUDE) $(PROTOBUF_INCLUDE) $(ORIG_CPPFLAGS)
LIB = $(SEQ_LIBS) pub medline biblio general xser psg_rpc psg_cassandra psg_cache connext xconnserv xconnect xutil xncbi psg_protobuf

REQUIRES = CASSANDRA MT Linux H2O LMDB LIBUV NGHTTP2 PROTOBUF

WATCHERS = satskyse dmitrie1
