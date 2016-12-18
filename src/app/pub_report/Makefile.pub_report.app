# Build application "pub_report"
#################################

APP = pub_report
SRC = journal_hook journal_report pub_report seq_entry_hook seqid_hook unpub_hook \
      unpub_report utils

LIB = eutils_client hydra_client xmlwrapp xvalidate $(OBJEDIT_LIBS) xalnmgr \
      xobjutil valerr submit pubmed xconnect $(SOBJMGR_LIBS)

LIBS = $(LIBXML_LIBS) $(LIBXSLT_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

WATCHERS = dobronad