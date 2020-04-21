
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

LIB  = fix_pub eutils_client xmlwrapp xid_wgsdb xid_utils \
    $(ncbi_xreader_pubseqos2) xcleanup \
    $(OBJEDIT_LIBS) xobjutil valid taxon1 mlacli mla medlars pubmed \
    ncbi_xdbapi_ftds $(FTDS_LIB) xregexp $(PCRE_LIB) $(OBJMGR_LIBS)

LIBS = $(LIBXSLT_LIBS) $(LIBXML_LIBS) $(FTDS_LIBS) \
    $(GENBANK_THIRD_PARTY_LIBS) $(PCRE_LIBS) $(CMPRS_LIBS) $(ORIG_LIBS)

