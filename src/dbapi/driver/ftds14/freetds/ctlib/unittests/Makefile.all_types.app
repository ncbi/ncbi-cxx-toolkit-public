# $Id$

APP = ct14_all_types
SRC = all_types common

CPPFLAGS = -DHAVE_CONFIG_H=1 $(FTDS14_INCLUDE) $(ORIG_CPPFLAGS)
LIB      = ct_ftds14$(FORCE_STATIC) tds_ut_common_ftds14 tds_ftds14
LIBS     = $(FTDS14_CTLIB_LIBS) $(NETWORK_LIBS) $(RT_LIBS) $(C_LIBS)
LINK     = $(C_LINK)

CHECK_CMD  =

WATCHERS = ucko satskyse
