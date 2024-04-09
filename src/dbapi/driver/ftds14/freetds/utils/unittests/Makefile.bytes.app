# $Id$

APP = tds14_bytes
SRC = bytes

CPPFLAGS = -DHAVE_CONFIG_H=1 $(FTDS14_INCLUDE) -I$(srcdir)/.. $(ORIG_CPPFLAGS)
LIB      = tds_ftds14
LIBS     = $(FTDS14_CTLIB_LIBS) $(NETWORK_LIBS) $(RT_LIBS) $(C_LIBS)
LINK     = $(C_LINK)

CHECK_CMD =

WATCHERS = ucko satskyse
