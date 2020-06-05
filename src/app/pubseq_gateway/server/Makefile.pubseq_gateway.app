# $Id$

ASN_DEP = psg_protobuf

APP = pubseq_gateway
SRC = pubseq_gateway  \
      pending_operation http_server_transport pubseq_gateway_exception \
      uv_extra pubseq_gateway_utils pubseq_gateway_stat \
      pubseq_gateway_handlers pubseq_gateway_convert_utils \
      pubseq_gateway_cache_utils cass_fetch psgs_reply \
      exclude_blob_cache alerts timing cass_monitor id2info \
      insdc_utils introspection psgs_request getblob_processor cass_processor_base \
      cass_blob_base tse_chunk_processor resolve_processor resolve_base \
      async_resolve_base async_bioseq_info_base annot_processor \
      get_processor

LIBS = $(H2O_STATIC_LIBS) $(CASSANDRA_STATIC_LIBS) $(LIBUV_STATIC_LIBS) $(LMDB_STATIC_LIBS) $(PROTOBUF_LIBS) $(ORIG_LIBS) $(KRB5_LIBS)
CPPFLAGS = $(CASSANDRA_INCLUDE) $(H2O_INCLUDE) $(LMDB_INCLUDE) $(PROTOBUF_INCLUDE) $(ORIG_CPPFLAGS)
LIB = $(SEQ_LIBS) pub medline biblio general xser psg_cassandra psg_protobuf psg_cache connext xconnserv xconnect xutil xncbi

REQUIRES = CASSANDRA MT Linux H2O LMDB LIBUV PROTOBUF

WATCHERS = satskyse

CHECK_CMD=test/tccheck.sh

CHECK_COPY=test/ etc/pubseq_gateway.ini

