
SRC     = \
    wgs_asn \
    wgs_errors \
    wgs_filelist \
    wgs_id1 \
    wgs_master \
    wgs_med \
    wgs_params \
    wgs_parse \
    wgs_pubs \
    wgs_sub \
    wgs_tax \
    wgs_text_accession \
    wgs_utils


APP     = wgs_parse

LIB     = fix_pub eutils_client xcleanup ncbi_xdbapi_ftds \
          ncbi_xreader_pubseqos ncbi_xreader_pubseqos2 \
          ncbi_xloader_genbank ncbi_xreader_id1 ncbi_xreader_id2 \
          ncbi_xreader_cache id2 ncbi_xreader dbapi_driver \
          ct_ftds100 tds_ftds100 xconnext xcompress \
          id1 xmlwrapp $(OBJEDIT_LIBS) \
          xobjutil valid taxon1 mlacli mla medlars pubmed xconnect \
          xregexp $(PCRE_LIB) $(SOBJMGR_LIBS)

LIBS    = $(NGHTTP2_LIBS) $(LIBUV_LIBS) $(SYBASE_LIBS) $(VDB_LIBS) $(FTDS_LIBS) $(PCRE_LIBS) $(LIBXSLT_LIBS) $(LIBXML_LIBS) $(ORIG_LIBS)
