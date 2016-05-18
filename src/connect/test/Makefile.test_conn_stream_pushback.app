# $Id$

APP = test_conn_stream_pushback
SRC = test_conn_stream_pushback
LIB = xconnect xpbacktest xncbi

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
#LINK = purify $(ORIG_LINK)

CHECK_CMD = test_conn_stream_pushback.sh
CHECK_COPY = test_conn_stream_pushback.sh

WATCHERS = lavr elisovdn
