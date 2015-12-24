# $Id$

APP = db95_done_handling
SRC = done_handling common

CPPFLAGS = -DHAVE_CONFIG_H=1 -DNEED_FREETDS_SRCDIR $(FTDS95_INCLUDE) \
           $(ORIG_CPPFLAGS)
LIB      = sybdb_ftds95$(STATIC) tds_ftds95$(STATIC)
LIBS     = $(FTDS95_CTLIB_LIBS) $(NETWORK_LIBS) $(RT_LIBS) $(C_LIBS)
LINK     = $(C_LINK)

CHECK_CMD  = test-db95.sh --no-auto db95_done_handling
CHECK_COPY = test-db95.sh done_handling.sql

CHECK_REQUIRES = in-house-resources

WATCHERS = ucko
