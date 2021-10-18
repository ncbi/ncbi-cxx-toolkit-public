# $Id$

APP = perf_view
SRC = perf_view

LIB  = xregexp $(PCRE_LIB) xncbi
LIBS = $(PCRE_LIBS) $(ORIG_LIBS)

CHECK_COPY = perf_view.OM perf_view.S212 perf_view.S235 perf_view.T212 perf_view.T235
CHECK_CMD = perf_view OM
CHECK_CMD = perf_view S212 S235 T212 T235

WATCHERS = vakatov
