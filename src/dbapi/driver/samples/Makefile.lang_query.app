# $Id$

APP = lang_query
SRC = lang_query

LIB  = dbapi_sample_base dbapi_driver xutil xncbi
LIBS = $(DL_LIBS) $(ORIG_LIBS)

CHECK_CMD  = run_sybase_app.sh test_lang_query.sh
CHECK_COPY = test_lang_query.sh
CHECK_TIMEOUT = 300
