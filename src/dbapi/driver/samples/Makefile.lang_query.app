# $Id$

APP = lang_query
SRC = lang_query

LIB  = dbapi_sample_base dbapi_driver $(XCONNEXT) xconnect xutil xncbi
LIBS = $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CHECK_CMD  = test_lang_query.sh
CHECK_COPY = test_lang_query.sh
CHECK_TIMEOUT = 400
