
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

LIB     = fix_pub taxon1 mlacli mla medlars pubmed xconnect \
          id1cli id1 xcleanup taxon3 submit valid xregexp \
          seq seqcode xobjutil xobjmgr genome_collection xncbi sequtil pub medline biblio general \
          eutils_client xser seqset xutil xobjedit xmlwrapp $(PCRE_LIB)


LIBS    = $(SYBASE_LIBS) $(VDB_LIBS) $(FTDS_LIBS) $(PCRE_LIBS) $(LIBXSLT_LIBS) $(LIBXML_LIBS) $(ORIG_LIBS)
