# $Id$

APP = lang_query
SRC = lang_query

LIB  = dbapi_driver dbapi_sample_base xutil xncbi
LIBS = $(DL_LIBS) $(ORIG_LIBS)

CHECK_CMD  = test_lang_query.sh
CHECK_COPY = test_lang_query.sh
