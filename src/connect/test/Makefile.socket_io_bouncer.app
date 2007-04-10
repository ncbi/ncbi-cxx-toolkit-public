# $Id$

APP = socket_io_bouncer
SRC = socket_io_bouncer
LIB = connect $(NCBIATOMIC_LIB)

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
#LINK = purify $(ORIG_LINK)
