# $Id$

APP = ncbi_applog
SRC = ncbi_applog ncbi_applog_url

LIB = xconnect xregexp $(PCRE_LIB) xncbi clog
LIBS = $(NETWORK_LIBS) $(PCRE_LIBS) $(ORIG_LIBS)

CPPFLAGS = $(PCRE_INCLUDE) $(ORIG_CPPFLAGS)

WATCHERS = ivanov

#CHECK_CMD=
