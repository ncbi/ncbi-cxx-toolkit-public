
SRC     = \
    wgs_asn \
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

LIB     = taxon1 mlacli mla medlars pubmed xconnect \
          id1cli id1 xcleanup taxon3 submit valid xregexp \
          seq seqcode xobjutil xobjmgr genome_collection xncbi sequtil pub medline biblio general \
          xser seqset xutil xobjedit $(PCRE_LIB)


LIBS    = $(SYBASE_LIBS) $(VDB_LIBS) $(FTDS_LIBS) $(PCRE_LIBS) $(ORIG_LIBS)
