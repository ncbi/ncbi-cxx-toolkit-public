# $Id$

APP = db14_empty_rowsets
SRC = empty_rowsets common

CPPFLAGS = -DHAVE_CONFIG_H=1 -DNEED_FREETDS_SRCDIR $(FTDS14_INCLUDE) \
           $(ORIG_CPPFLAGS)
LIB      = sybdb_ftds14 tds_ftds14
LIBS     = $(FTDS14_CTLIB_LIBS) $(NETWORK_LIBS) $(RT_LIBS) $(C_LIBS)
LINK     = $(C_LINK)

CHECK_CMD  = test-db14 db14_empty_rowsets
CHECK_COPY = empty_rowsets.sql

CHECK_REQUIRES = in-house-resources

WATCHERS = ucko satskyse
