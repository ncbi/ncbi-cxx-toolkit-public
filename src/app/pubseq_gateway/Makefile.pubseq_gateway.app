# $Id$

ASN_DEP = psg_protobuf

APP = pubseq_gateway
SRC = pubseq_gateway  \
      pending_operation http_server_transport pubseq_gateway_exception \
      uv_extra pubseq_gateway_utils pubseq_gateway_stat \
      pubseq_gateway_handlers pubseq_gateway_convert_utils

LIBS = $(H2O_LIBS) $(CASSANDRA_STATIC_LIBS) $(LIBUV_STATIC_LIBS) $(LMDB_LIBS) $(PROTOBUF_LIBS) $(ORIG_LIBS)
CPPFLAGS = $(CASSANDRA_INCLUDE) $(H2O_INCLUDE) $(LMDB_INCLUDE) $(PROTOBUF_INCLUDE) $(ORIG_CPPFLAGS)
LIB = $(SEQ_LIBS) pub medline biblio general xser psg_cassandra psg_protobuf psg_cache connext xconnserv xconnect xutil xncbi

REQUIRES = CASSANDRA MT Linux H2O LMDB LIBUV PROTOBUF

WATCHERS = satskyse dmitrie1
