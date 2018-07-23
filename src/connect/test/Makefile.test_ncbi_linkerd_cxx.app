# $Id$

APP = test_ncbi_linkerd_cxx
SRC = test_ncbi_linkerd_cxx

LIB = xregexp $(PCRE_LIB) xconnect xncbi
LIBS = $(PCRE_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

WATCHERS = mcelhany
