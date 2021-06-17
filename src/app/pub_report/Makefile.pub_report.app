# Build application "pub_report"
#################################

APP = pub_report
SRC = journal_hook journal_report pub_report seq_entry_hook seqid_hook unpub_hook \
      unpub_report utils

LIB = eutils_client hydra_client xmlwrapp xvalidate $(XFORMAT_LIBS) xalnmgr \
      $(OBJEDIT_LIBS) xobjutil taxon1 valerr xconnect xregexp $(PCRE_LIB) \
      tables $(SOBJMGR_LIBS)

LIBS = $(LIBXSLT_LIBS) $(LIBXML_LIBS) $(NETWORK_LIBS) $(PCRE_LIBS) $(ORIG_LIBS)

WATCHERS = dobronad choi

REQUIRES = objects LIBXML LIBXSLT
