# $Id$

APP = test_threaded_client
OBJ = test_threaded_client
LIB = xconnect xutil xncbi

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
#LINK = purify -best-effort CC

CHECK_CMD =
