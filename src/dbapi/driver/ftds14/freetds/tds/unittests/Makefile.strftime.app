# $Id$

APP = tds14_strftime
SRC = strftime

CPPFLAGS = -DHAVE_CONFIG_H=1 $(FTDS14_INCLUDE) -I$(srcdir)/.. $(ORIG_CPPFLAGS)
LIB      = tds_ut_common_ftds14 tds_ftds14
LIBS     = $(FTDS14_CTLIB_LIBS) $(NETWORK_LIBS) $(RT_LIBS) $(C_LIBS)
LINK     = $(C_LINK)

CHECK_CMD  =

WATCHERS = ucko satskyse
