# $Id$

APP = test_threaded_server
SRC = test_threaded_server
LIB = xconnect xutil xncbi

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
#LINK = purify -best-effort CC

CHECK_CMD =
