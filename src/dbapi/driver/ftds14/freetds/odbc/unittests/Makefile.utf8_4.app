# $Id$

APP = odbc14_utf8_4
SRC = utf8_4 common

CPPFLAGS = -DHAVE_CONFIG_H=1 $(FTDS14_INCLUDE) $(ODBC_INCLUDE) $(ORIG_CPPFLAGS)
LIB      = odbc_ftds14$(FORCE_STATIC) tds_ut_common_ftds14 tds_ftds14 \
           odbc_ftds14$(FORCE_STATIC)
LIBS     = $(FTDS14_CTLIB_LIBS) $(NETWORK_LIBS) $(RT_LIBS) $(C_LIBS)
LINK     = $(C_LINK)

CHECK_CMD  =

WATCHERS = ucko satskyse
