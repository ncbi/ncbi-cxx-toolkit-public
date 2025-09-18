# $Id$

ASN_DEP = psg_protobuf cdd_access

APP = pubseq_gateway
SRC = pubseq_gateway  \
      pending_operation pubseq_gateway_exception \
      uv_extra pubseq_gateway_utils pubseq_gateway_stat \
      pubseq_gateway_handlers pubseq_gateway_convert_utils \
      pubseq_gateway_cache_utils cass_fetch psgs_reply \
      exclude_blob_cache alerts timing cass_monitor id2info \
      insdc_utils introspection psgs_request getblob_processor cass_processor_base \
      cass_blob_base tse_chunk_processor resolve_processor resolve_base \
      async_resolve_base async_bioseq_info_base annot_processor \
      get_processor psgs_dispatcher cass_blob_id ipsgs_processor \
      cdd_processor \
      psgs_io_callbacks accession_version_history_processor psgs_uv_loop_binder \
      bioseq_info_record_selector split_info_utils split_info_cache \
      wgs_client wgs_processor cass_processor_dispatch snp_client snp_processor \
      psgs_seq_id_utils http_request http_connection http_reply http_proto \
      tcp_daemon http_daemon url_param_utils dummy_processor time_series_stat \
      ipg_resolve settings my_ncbi_cache myncbi_callback backlog_per_request \
      active_proc_per_request z_end_points myncbi_monitor throttling ssl \
      seq_id_classification_monitor

LIBS = $(PCRE_LIBS) $(OPENSSL_LIBS) $(H2O_STATIC_LIBS) $(CASSANDRA_STATIC_LIBS) \
       $(LIBXML_LIBS) $(LIBXSLT_LIBS) $(LIBUV_STATIC_LIBS) $(LMDB_STATIC_LIBS) $(PROTOBUF_LIBS) $(KRB5_LIBS) \
       $(PSG_CLIENT_LIBS) $(NETWORK_LIBS) $(CMPRS_LIBS) $(SRA_SDK_SYSLIBS) $(GRPC_LIBS) $(CURL_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(OPENSSL_INCLUDE) $(CASSANDRA_INCLUDE) $(H2O_INCLUDE) $(LMDB_INCLUDE) \
           $(PROTOBUF_INCLUDE) $(CMPRS_INCLUDE) $(SRA_INCLUDE) $(ORIG_CPPFLAGS)

LIB = $(SRAREAD_LIBS) dbsnp_ptis grpc_integration sraread \
      cdd_access psg_client id2 seqsplit seqset $(SEQ_LIBS) xregexp $(PCRE_LIB) $(CMPRS_LIB) \
      pub medline biblio general xser psg_ipg psg_cassandra psg_protobuf psg_cache \
      xcgi psg_myncbi xmlwrapp xconnext connext xconnserv xxconnect2 xconnect xcompress xutil xncbi

REQUIRES = CASSANDRA MT Linux H2O LMDB LIBUV LIBXML LIBXSLT PROTOBUF -ICC $(GRPC_OPT)

POST_LINK = $(VDB_POST_LINK)

WATCHERS = satskyse

CHECK_CMD=test/tccheck.sh

CHECK_COPY=test/ etc/pubseq_gateway.ini

