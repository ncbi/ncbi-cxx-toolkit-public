# Build application "pub_report"
#################################

APP = pub_report
SRC = journal_hook journal_report pub_report seq_entry_hook seqid_hook unpub_hook \
      unpub_report utils

LIB = eutils_client hydra_client pubmed xvalidate xconnect taxon3 xobjutil seq seqcode \
      xobjmgr genome_collection xncbi sequtil pub medline biblio general xser seqset \
      xutil xmlwrapp

LIBS = $(LIBXML_LIBS) $(LIBXSLT_LIBS) $(LIBEXSLT_LIBS) $(GNUTLS_LIBS)

WATCHERS = dobronad